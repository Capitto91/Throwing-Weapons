// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/PerfTimer.h"
#include "3.- WEAPON/WeaponManager.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

namespace Return
{
	namespace
	{
		// A diferencia del sondeo de Throw::TrackProjectile (una comprobación
		// cada ~50ms, un hilo nuevo por comprobación), aquí el movimiento debe
		// verse fluido: se usa un único hilo persistente que duerme un
		// intervalo corto y reencola en el hilo principal en bucle, en vez de
		// crear un hilo nuevo en cada paso.
		constexpr auto  kTickInterval = std::chrono::milliseconds(16);
		constexpr float kTickDeltaSeconds = 0.016f;

		// Margen de espera (en pasos de kTickInterval) para que el 3D de una
		// réplica recién creada termine de cargar en segundo plano antes de
		// darla por perdida (ver WaitFor3DThenStart). ~800ms de sobra para
		// lo que en el juego ha tardado unos pocos frames.
		constexpr int kMax3DWaitAttempts = 50;

		// Solo puede haber un regreso en marcha a la vez (arma arrojadiza
		// única); controla si el hilo de fondo sigue reencolando pasos.
		std::atomic<bool> g_active{ false };

		// Aceleración (punto 8 de Mecanica del arma.txt) y tiempo
		// transcurrido, y geometría de la curva de Bezier (punto 7) del regreso
		// en curso; todas se fijan una vez en StartControlling y Tick las va
		// consumiendo. Al ser un arma única, solo hay un regreso en marcha a
		// la vez, y todas estas variables solo se tocan desde el hilo
		// principal (dentro de tareas de
		// SKSE::GetTaskInterface()->AddTask, que se ejecutan en orden), así
		// que no hacen falta atómicas.
		float        g_acceleration{ 0.0f };
		float        g_elapsedTime{ 0.0f };
		RE::NiPoint3 g_startPosition{};
		RE::NiPoint3 g_controlPoint{};
		float        g_totalDistance{ 0.0f };

		// Ángulo de giro sobre sí misma acumulado (punto 11), en radianes,
		// envuelto a [0, 2π) cada tick para que combinarlo con el ángulo de
		// enderezado (Tick) no acumule valores cada vez mayores. Ahora se
		// aplica a la guiñada (Z), no al cabeceo (X) — ver comentario en
		// Tick sobre por qué (probado en el juego: girar sobre el cabeceo
		// dejaba el modelo "estirado" hacia la dirección de vuelo en vez
		// de un giro de hacha).
		float g_spinAngle{ 0.0f };

		// Cabeceo (pitch) hacia la mano en el instante de arrancar el
		// regreso, fijado una vez en StartControlling. Mientras el arma
		// gira rápido sobre sí misma (ahora en la guiñada), el cabeceo se
		// mantiene en este valor fijo en vez de seguir la curva tick a
		// tick — combinar un ángulo cambiante con el giro rápido componía
		// un efecto de tornillo en vez de un giro limpio (probado en el
		// juego). Solo se interpola hacia el cabeceo real de la curva
		// durante el enderezado final.
		float g_initialPitch{ 0.0f };

		RE::NiPoint3 GetHandPosition(RE::Actor* a_player)
		{
			if (auto* weaponNode = a_player->GetNodeByName("WEAPON")) {
				return weaponNode->world.translate;
			}

			return a_player->GetPosition();
		}

		// Vector "derecha" del jugador en el mundo (eje X local de su nodo
		// 3D raíz, misma convención que Throw::GetCameraAimAngles usa para
		// "adelante" con el eje Y): se usa para que la curva del regreso
		// (punto 7) entre siempre por el lado de la mano derecha, donde se
		// recoge el arma, en vez de un lado arbitrario.
		RE::NiPoint3 GetPlayerRightVector(RE::Actor* a_player)
		{
			auto* node = a_player->Get3D();
			if (!node) {
				return { 1.0f, 0.0f, 0.0f };
			}

			return node->world.rotate.GetVectorX();
		}

		// TESObjectREFR::SetPosition solo actualiza la posición "lógica" del
		// objeto; el bhkRigidBody de Havok, que sigue existiendo aunque esté
		// en modo kKeyframed, no se entera por sí solo. bhkRigidBody trabaja
		// en unidades de Havok (metros), de ahí bhkWorld::GetWorldScale()
		// para convertir desde unidades de juego.
		void SetHavokPosition(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position)
		{
			auto* node = a_refr.Get3D();
			auto* collisionObj = node ? node->GetCollisionObject() : nullptr;
			auto* rigidBody = collisionObj ? collisionObj->GetRigidBody() : nullptr;
			if (!rigidBody) {
				return;
			}

			RE::hkVector4 havokPosition(a_position * RE::bhkWorld::GetWorldScale());
			rigidBody->SetPosition(havokPosition);
		}

		// Mismo motivo que SetHavokPosition, pero para la rotación (punto
		// 11): TESObjectREFR::SetAngle actualiza el nodo 3D visual, pero no
		// el bhkRigidBody de Havok. EulerAnglesToAxesZXY usa la misma
		// convención de ejes que el motor (ver Math::ComputeLookAtAngles).
		void SetHavokRotation(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_angle)
		{
			auto* node = a_refr.Get3D();
			auto* collisionObj = node ? node->GetCollisionObject() : nullptr;
			auto* rigidBody = collisionObj ? collisionObj->GetRigidBody() : nullptr;
			if (!rigidBody) {
				return;
			}

			RE::NiMatrix3 rotationMatrix;
			rotationMatrix.EulerAnglesToAxesZXY(a_angle);
			const RE::NiQuaternion niRotation(rotationMatrix);

			RE::hkQuaternion havokRotation;
			havokRotation.vec = RE::hkVector4(niRotation.x, niRotation.y, niRotation.z, niRotation.w);
			rigidBody->SetRotation(havokRotation);
		}

		// Detiene el hilo de fondo, borra la réplica controlada a mano y
		// notifica a WeaponManager para que reequipe el arma real. Comprueba
		// el estado antes de notificar por si el ciclo ya terminó por otra
		// vía (p. ej. recuperación instantánea al cerrar una pantalla de
		// carga a mitad del regreso), para no reequipar ni resetear el
		// estado dos veces.
		void FinishReturn(RE::ObjectRefHandle a_handle)
		{
			g_active = false;

			if (auto refr = a_handle.get()) {
				logs::info("Return::FinishReturn llegó a ({:.1f},{:.1f},{:.1f})", refr->GetPosition().x, refr->GetPosition().y, refr->GetPosition().z);
				refr->Disable();
				refr->SetDelete(true);
			}

			if (Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kReturning) {
				Weapon::WeaponManager::GetSingleton()->OnReturnComplete();
			}
		}

		void Tick(RE::ObjectRefHandle a_handle, RE::ActorHandle a_playerHandle)
		{
			// Diagnóstico temporal (ver PerfTimer.h): Tick() corre en el
			// hilo principal hasta ~60 veces/segundo durante todo el
			// regreso, así que cualquier coste aquí es coste de fotograma
			// directo.
			Perf::ScopedTimer timer{ "Return::Tick", std::chrono::microseconds{ 2000 } };

			if (Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kReturning) {
				// El ciclo salió de "regresando" por otra vía; nada que
				// mover ni que limpiar aquí (ya se habrá encargado quien
				// haya provocado el cambio de estado).
				g_active = false;
				return;
			}

			auto refr = a_handle.get();
			auto player = a_playerHandle.get();
			if (!refr || !player) {
				FinishReturn(a_handle);
				return;
			}

			const auto handPos = GetHandPosition(player.get());
			const auto currentPos = refr->GetPosition();

			if ((handPos - currentPos).Length() <= Constants::kReturnArrivalDistance) {
				FinishReturn(a_handle);
				return;
			}

			// Aceleración constante partiendo de velocidad cero (punto 8 de
			// Mecanica del arma.txt): la distancia recorrida hasta este tick
			// es ½·aceleración·tiempo², no una velocidad plana.
			g_elapsedTime += kTickDeltaSeconds;
			const float traveled = ComputeTraveledDistance(g_acceleration, g_elapsedTime);
			const float t = g_totalDistance > 0.0f ? std::clamp(traveled / g_totalDistance, 0.0f, 1.0f) : 1.0f;

			// Curva de Bezier cuadrática (punto 7: el regreso nunca es una
			// línea recta). g_startPosition y g_controlPoint se fijaron una
			// vez en StartControlling; el punto final sigue a la mano del
			// jugador aunque se mueva durante el regreso.
			const auto nextPos = Math::EvaluateQuadraticBezier(g_startPosition, g_controlPoint, handPos, t);
			refr->SetPosition(nextPos);
			SetHavokPosition(*refr, nextPos);

			// Punto 11: gira sobre sí misma durante el vuelo (velocidad
			// constante) y, en el último tramo antes de llegar a la mano,
			// se endereza para apuntar en la dirección de vuelo — tangente
			// real de la curva en este punto, no la línea recta hacia la
			// mano, para que el enderezado seleccione la orientación
			// coherente con hacia dónde está curvándose el arma.
			//
			// Probado en el juego: girar sobre el cabeceo (X, como en la
			// primera versión) dejaba el modelo "estirado" hacia la
			// dirección de vuelo en vez de parecer el giro de un hacha
			// arrojadiza. Corregido con un giro fijo de 90° sobre el
			// alabeo (Y, Constants::kReturnModelRollOffset — el eje
			// mango→cabeza del modelo) para dejarlo "de lado", y moviendo
			// el giro en vuelo a la guiñada (Z) en su lugar. El cabeceo
			// pasa a mantenerse fijo en g_initialPitch mientras dura el
			// giro (mismo motivo que antes con la guiñada: dos ángulos
			// cambiando a la vez componía el efecto de tornillo) y solo se
			// interpola hacia el cabeceo real de la curva al enderezar.
			const auto tangent = Math::EvaluateQuadraticBezierTangent(g_startPosition, g_controlPoint, handPos, t);
			const auto lookAngles = Math::ComputeLookAtAngles(tangent);

			constexpr float kTwoPi = 6.283185307179586f;
			g_spinAngle = std::fmod(g_spinAngle + Math::DegreesToRadians(Constants::kReturnSpinDegreesPerSecond) * kTickDeltaSeconds, kTwoPi);

			const float distanceToHand = (handPos - nextPos).Length();
			const float straightenWeight = 1.0f - std::clamp(distanceToHand / Constants::kReturnStraightenDistance, 0.0f, 1.0f);

			const float pitch = Math::LerpAngle(g_initialPitch, lookAngles.x, straightenWeight);
			const float yaw = Math::LerpAngle(g_spinAngle, lookAngles.z, straightenWeight);

			const RE::NiPoint3 angle{ pitch, Constants::kReturnModelRollOffset, yaw };
			refr->SetAngle(angle);
			SetHavokRotation(*refr, angle);

			refr->Update3DPosition(true);
		}

		void RunLoop(RE::ObjectRefHandle a_handle, RE::ActorHandle a_playerHandle)
		{
			while (g_active.load()) {
				std::this_thread::sleep_for(kTickInterval);
				if (!g_active.load()) {
					return;
				}

				SKSE::GetTaskInterface()->AddTask([a_handle, a_playerHandle]() { Tick(a_handle, a_playerHandle); });
			}
		}

		void StartControlling(RE::Actor* a_player, RE::ObjectRefHandle a_handle)
		{
			auto refr = a_handle.get();
			if (!a_player || !refr) {
				return;
			}

			// Modo Havok "movido por código": deja de recibir fuerzas/
			// gravedad de la simulación pero conserva colisión; sin esto,
			// forzar la posición de un cuerpo simulado activamente produce
			// tirones/clipping (ver CLAUDE.md).
			auto* node3D = refr->Get3D();
			if (node3D) {
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
			}
			SetHavokPosition(*refr, refr->GetPosition());
			refr->Update3DPosition(true);

			// Aceleración híbrida (punto 8): por defecto salvo que a esa
			// aceleración, partiendo de velocidad cero, tardara más de
			// Constants::kReturnMaxDuration en cubrir la distancia actual a
			// la mano — entonces se aumenta lo justo para cumplir ese
			// límite.
			const auto  handPos = GetHandPosition(a_player);
			const auto  startPos = refr->GetPosition();
			const float distance = (handPos - startPos).Length();
			g_acceleration = ComputeReturnAcceleration(distance, Constants::kReturnAcceleration, Constants::kReturnMaxDuration);
			g_elapsedTime = 0.0f;
			g_spinAngle = 0.0f;
			g_initialPitch = Math::ComputeLookAtAngles(handPos - startPos).x;

			// Curva de Bezier cuadrática (punto 7): el punto de control se
			// calcula una única vez aquí, a partir de la mano en este
			// instante; el punto final de la curva (Tick) sí sigue a la mano
			// en cada tick si el jugador se mueve durante el regreso.
			g_startPosition = startPos;
			g_controlPoint = ComputeReturnControlPoint(startPos, handPos, GetPlayerRightVector(a_player));
			g_totalDistance = distance;

			logs::info("Return::StartControlling arrancando en ({:.1f},{:.1f},{:.1f}), distancia={:.1f}, aceleración={:.1f}",
				startPos.x, startPos.y, startPos.z, distance, g_acceleration);

			g_active = true;
			std::thread(RunLoop, a_handle, RE::ActorHandle(a_player)).detach();
		}

		// La réplica recién creada (ver Throw::SpawnWeaponReplicaAt) no
		// tiene el modelo 3D listo de inmediato — carga en segundo plano —
		// y hasta entonces SetMotionType/SetPosition no tienen ningún
		// efecto. Se espera a que Get3D() deje de ser nulo antes de empezar
		// a controlarla.
		void WaitFor3DThenStart(RE::Actor* a_player, RE::ObjectRefHandle a_handle, int a_attemptsLeft)
		{
			auto refr = a_handle.get();
			if (!a_player || !refr) {
				return;
			}

			if (refr->Get3D()) {
				StartControlling(a_player, a_handle);
				return;
			}

			if (a_attemptsLeft <= 0) {
				// El 3D nunca llegó a cargar; red de seguridad para no
				// dejar el ciclo bloqueado en "regresando" para siempre.
				logs::warn("WaitFor3DThenStart: agotados los reintentos, el 3D nunca llegó a cargar");
				refr->Disable();
				refr->SetDelete(true);
				if (Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kReturning) {
					Weapon::WeaponManager::GetSingleton()->OnReturnComplete();
				}
				return;
			}

			{
				// Diagnóstico temporal (ver PerfTimer.h): esta creación de
				// hilo ocurre en el hilo principal, hasta ~60 veces/segundo
				// mientras se espera a que cargue el 3D de la réplica.
				Perf::ScopedTimer timer{ "Return::WaitFor3DThenStart creación de hilo", std::chrono::microseconds{ 1000 } };
				std::thread([a_player, a_handle, a_attemptsLeft]() {
					std::this_thread::sleep_for(kTickInterval);
					SKSE::GetTaskInterface()->AddTask([a_player, a_handle, a_attemptsLeft]() {
						WaitFor3DThenStart(a_player, a_handle, a_attemptsLeft - 1);
					});
				}).detach();
			}
		}
	}

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_handle)
	{
		WaitFor3DThenStart(a_player, a_handle, kMax3DWaitAttempts);
	}
}
