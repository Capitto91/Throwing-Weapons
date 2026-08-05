// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/CatchSound.h"
#include "12.- AUDIO/FlightSound.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <vector>

namespace Return
{
	namespace
	{
		// Nodo de la mano derecha (mismo origen que Throw::GetLaunchOrigin
		// para el lanzamiento, ver 4.- THROW/ThrowManager): destino real
		// hacia el que vuela el arma, no la posición del actor.
		RE::NiPoint3 GetHandPosition(RE::Actor* a_player)
		{
			if (auto* handNode = a_player->GetNodeByName("WEAPON")) {
				return handNode->world.translate;
			}

			return a_player->GetPosition();
		}

		// Cuerpo real de BeginReturn (trayectoria curva + aceleración
		// híbrida, puntos 7-8), separado para que el temblor de
		// desprendimiento (punto 11, ver BeginReturn más abajo) pueda
		// insertarse antes sin duplicar esta lógica: se invoca tal cual al
		// terminar el temblor si la réplica venía clavada, o de inmediato
		// si venía en vuelo.
		void BeginReturnMovement(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, ReturnCallbacks a_callbacks, std::shared_ptr<Audio::CatchCue> a_catchCue, float a_shudderDuration)
		{
			auto replica = a_replicaHandle.get();
			if (!a_player || !replica) {
				logs::warn("Return::BeginReturnMovement: sin jugador o réplica válida, se aborta el regreso.");
				a_callbacks.onApproaching();
				a_callbacks.onArrived();
				return;
			}

			const auto start = replica->GetPosition();
			const auto initialHandPos = GetHandPosition(a_player);

			const float initialDistance = (initialHandPos - start).Length();
			float       acceleration = ComputeReturnAcceleration(initialDistance);
			const auto  controlPoint = ComputeReturnControlPoint(start, initialHandPos, GetPlayerRightVector(a_player), Constants::kReturnCurveAnchorFraction);

			// Duración estimada del regreso con la aceleración natural --
			// para el log, y también la que ya usó Return::BeginReturn
			// (antes de esta llamada) para calcular el retardo del sonido
			// de arranque del atrape (a_catchCue, ver Audio::CatchCue).
			const float naturalDuration = ComputeReturnDuration(acceleration, initialDistance);

			// Duración mínima que necesita el tramo de movimiento para que
			// el gesto de Atrape le dé tiempo a sincronizarse de verdad: el
			// margen de asentado del grafo tras Llamada
			// (Constants::kMinCatchAnimationDelay) menos lo que ya haya
			// cubierto el temblor de desprendimiento si lo hubo
			// (a_shudderDuration), más la duración propia de Catch.hkx
			// (Constants::kCatchAnimationLeadTime) -- nunca por debajo de
			// esta última sola, aunque el temblor ya cubriera de sobra el
			// margen de asentado. Si la distancia es tan corta que la
			// aceleración natural terminaría antes de este mínimo, se
			// ralentiza el vuelo (nunca se acelera) para que dure
			// exactamente lo necesario -- a petición del usuario
			// (2026-08-03): la sincronización animación/física no es
			// negociable, nunca se desacopla con temporizadores
			// independientes de la trayectoria real.
			const float requiredForSettle = Constants::kMinCatchAnimationDelay - a_shudderDuration + Constants::kCatchAnimationLeadTime;
			const float requiredMovementDuration = requiredForSettle > Constants::kCatchAnimationLeadTime ? requiredForSettle : Constants::kCatchAnimationLeadTime;
			if (naturalDuration < requiredMovementDuration) {
				acceleration = ComputeReturnAccelerationForDuration(initialDistance, requiredMovementDuration);
				logs::info(
					"Return::BeginReturnMovement: distancia muy corta ({:.1f}) -- regreso ralentizado a {:.2f}s (natural: {:.2f}s) para sincronizar con Atrape.",
					initialDistance, requiredMovementDuration, naturalDuration);
			}

			const float estimatedDuration = ComputeReturnDuration(acceleration, initialDistance);

			logs::info(
				"Return::BeginReturnMovement: distancia inicial {:.1f}, aceleración {:.1f}, duración estimada {:.2f}s",
				initialDistance, acceleration, estimatedDuration);

			// Sonido (ver 12.- AUDIO/FlightSound): mismo criterio que
			// Throw::LaunchWeapon -- silbido suelto al iniciar el tramo de
			// movimiento del regreso más el loop posicional que sigue a la
			// réplica mientras dura la curva de vuelta.
			auto flightSound = std::make_shared<Audio::FlightSound>();
			if (auto* node3D = replica->Get3D()) {
				Audio::PlaySoundOneShot(start, Constants::kThrowLaunchSoundLocalFormID);
				flightSound->Start(node3D, Constants::kFlightLoopSoundLocalFormID);
			}

			// Punto de partida real del giro para este tramo (ver
			// CLAUDE.md, "Arquitectura de física de proyectiles"): lo que
			// llevara el nodo de giro justo antes de empezar a mover la
			// réplica -- recién desprendida tras el temblor
			// (Return::BeginReturn), o todavía en pleno vuelo de ida si el
			// regreso lo dispara un auto-recall a media parábola. Sin
			// esto, el primer tick de este bucle saltaba de golpe a la
			// fórmula de giro calculada desde cero, mismo problema que en
			// el lanzamiento (ver Throw::LaunchWeapon). rootWorld se lee
			// una única vez: el nodo raíz de la réplica no vuelve a rotar
			// en lo que le queda de vida (nadie llama SetAngle sobre
			// ella).
			const RE::NiMatrix3 rootWorld = replica->Get3D() ? replica->Get3D()->world.rotate : RE::NiMatrix3{};
			const RE::NiMatrix3 movementBaseLocal = Animation::GetSpinLocalRotation(*replica);

			auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, rootWorld, movementBaseLocal, onArrived = a_callbacks.onArrived, onApproaching = a_callbacks.onApproaching, shudderDuration = a_shudderDuration, approachFired = false, arrivedFired = false, straightenStart = 0.0f, straightenBlendFromLocal = RE::NiMatrix3{}, elapsed = 0.0f, progressElapsed = 0.0f, hitActors = std::vector<RE::ActorHandle>{}, flightSound, catchCue = std::move(a_catchCue), loggedHandAxisDiagnostic = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				const auto handPos = GetHandPosition(a_player);

				// Una vez cruzado el umbral de llegada (más abajo), el
				// bucle ya no se detiene -- se queda vivo pegando la
				// réplica a la mano cada tick (siguiéndola si el jugador
				// se sigue moviendo) en vez de dejarla congelada en la
				// última posición de la curva, que podía quedar a
				// Constants::kReturnArrivalDistance de la mano: un hueco
				// visible entre "la réplica se para" y "el arma real
				// reaparece" (reportado por el usuario, 2026-08-04),
				// incluso con onApproaching ya bien sincronizado -- la
				// estimación de velocidad nunca es perfecta del todo
				// (mide un único tick, el suavizado del tramo final sigue
				// decelerando después de esa medición), así que un margen
				// residual siempre es posible; pegar la réplica a la mano
				// mientras dure ese margen convierte el hueco en, como
				// mucho, un cambio de malla en el sitio correcto, no un
				// salto de posición. El reequipado real (ReequipAndReset,
				// gatillado por la anotación de Catch.hkx) cancela este
				// bucle desde fuera cuando de verdad toca -- ver
				// WeaponManager::OnCatchReleaseAnimationEvent.
				if (arrivedFired) {
					a_refr.SetPosition(handPos);
					Physics::SyncHavok(a_refr, handPos, a_refr.GetAngle());
					return true;
				}

				// Punto 10: se calcula y escribe el giro a mano cada tick
				// (ver Animation::TickSpin), igual que en la ida -- salvo
				// durante la ventana de enderezado (arrancada junto con
				// onApproaching, ver más abajo), donde
				// Animation::TickSpinStraighten la sustituye para que el
				// giro llegue frenado y alineado con la orientación real
				// de la mano en vez de en mitad de una vuelta cualquiera
				// (segunda mitad del punto 10, bug reportado por el
				// usuario, 2026-08-04: el cambio a la pose real se notaba
				// como un salto brusco de rotación). El objetivo se
				// recalcula cada tick (GetHandBoneWorldRotation, no una
				// foto fija tomada al empezar la ventana) porque el
				// jugador puede seguir girando mientras dura -- así el
				// enderezado siempre converge a la orientación real en el
				// instante exacto de la llegada, no a la que tuviera la
				// mano medio segundo antes.
				if (approachFired) {
					const float         blend = (elapsed - straightenStart) / Constants::kSpinStraightenDuration;
					const RE::NiMatrix3 handTargetLocal = Math::LocalRotationFromWorld(rootWorld, Animation::GetHandBoneWorldRotation(*a_player));
					Animation::TickSpinStraighten(a_refr, straightenBlendFromLocal, handTargetLocal, blend);
				} else {
					Animation::TickSpin(a_refr, elapsed, movementBaseLocal);
				}

				// Mejora Kratos #4, campo 2 (diagnóstico, todavía sin usar en
				// la curva): antes de confiar en la rotación del hueso "WEAPON"
				// para el punto de control p2 (sin precedente en este proyecto
				// de leer la *rotación* de ese hueso, solo su posición), se
				// registran los tres ejes una vez por regreso para comprobar a
				// ojo cuál apunta de verdad "hacia fuera de la palma" -- no
				// asumir que es Y por analogía con la cámara (ver
				// PLAN-mejoras-kratos.md).
				if (!loggedHandAxisDiagnostic) {
					if (auto* handNode = a_player->GetNodeByName("WEAPON")) {
						const auto vecX = handNode->world.rotate.GetVectorX();
						const auto vecY = handNode->world.rotate.GetVectorY();
						const auto vecZ = handNode->world.rotate.GetVectorZ();
						logs::info(
							"Return::BeginReturnMovement: diagnóstico ejes hueso mano -- X=({:.2f},{:.2f},{:.2f}) Y=({:.2f},{:.2f},{:.2f}) Z=({:.2f},{:.2f},{:.2f})",
							vecX.x, vecX.y, vecX.z, vecY.x, vecY.y, vecY.z, vecZ.x, vecZ.y, vecZ.z);
					}
					loggedHandAxisDiagnostic = true;
				}

				// Suavizado del tramo final (ver Constants::kReturnTailDistance):
				// el tiempo "de progreso" que alimenta a
				// ComputeTraveledDistance avanza más despacio que el tiempo
				// real cuanto más cerca está la réplica de la mano -- el
				// perfil de aceleración creciente del punto 8 no cambia de
				// forma, solo se recorre más lento en tiempo real en el
				// último tramo. Se mide con la distancia del tick anterior
				// (previousPos), no de nextPos (todavía sin calcular), igual
				// que ya hace Collision::SweepRaycast unas líneas más abajo.
				const float previousDistanceToHand = (handPos - previousPos).Length();
				const float tailBlend = Constants::kReturnTailDistance > 0.0f ? std::clamp(previousDistanceToHand / Constants::kReturnTailDistance, 0.0f, 1.0f) : 1.0f;
				const float smoothTailBlend = tailBlend * tailBlend * (3.0f - 2.0f * tailBlend);
				const float timeRate = Constants::kReturnTailMinRate + (1.0f - Constants::kReturnTailMinRate) * smoothTailBlend;
				progressElapsed += a_deltaSeconds * timeRate;

				const float traveled = ComputeTraveledDistance(acceleration, progressElapsed);
				const float t = initialDistance > 0.0f ? std::clamp(traveled / initialDistance, 0.0f, 1.0f) : 1.0f;

				const auto nextPos = Math::EvaluateQuadraticBezier(start, controlPoint, handPos, t);

				// Punto 9: golpear a un enemigo durante el regreso ya no se
				// queda clavado, el vuelo por la curva continúa igual — solo
				// se aplica el golpe (stagger + daño reducido, ver
				// Combat::ApplyReturnHit) y se sigue. Un impacto contra algo
				// que no sea un actor (pared, suelo...) se ignora sin más: a
				// diferencia de la ida, el regreso no choca contra el
				// escenario. Cada actor solo recibe el golpe una vez por
				// regreso (hitActors), para no repetirlo tick a tick mientras
				// la réplica pasa cerca de él.
				const auto hit = Collision::SweepRaycast(previousPos, nextPos, Constants::kThrowCollisionRadius, a_player, &a_refr);
				if (auto* actor = hit.hit && hit.target ? hit.target->As<RE::Actor>() : nullptr) {
					RE::ActorHandle actorHandle(actor);
					if (std::ranges::find(hitActors, actorHandle) == hitActors.end()) {
						hitActors.push_back(actorHandle);
						Combat::ApplyReturnHit(a_player, actor);
					}
				}

				a_refr.SetPosition(nextPos);
				Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());

				// Sonido de arranque del atrape (ver Audio::CatchCue): su
				// reloj interno ya viene contando desde Return::BeginReturn
				// (temblor incluido si lo hubo), así que aquí solo hay que
				// seguir alimentándolo con el tiempo de este tick.
				catchCue->UpdateStart(nextPos, a_deltaSeconds);

				const float distanceToHand = (handPos - nextPos).Length();

				// Sincronización en vivo del gesto de Atrape con el vuelo
				// real (a petición del usuario, 2026-08-03: nunca
				// desacoplar animación y física con un temporizador
				// precalculado de antemano -- diverge de la trayectoria
				// real a medida que avanza el regreso, sobre todo por el
				// suavizado del tramo final, que alarga la duración real
				// más allá de lo que predice ComputeReturnDuration, ver
				// Constants::kReturnTailDistance/kReturnTailMinRate).
				// Se mide la velocidad real de este mismo tick (distancia
				// cerrada / tiempo) para estimar cuánto falta de verdad en
				// segundos reales, en vez de fiarse de una predicción hecha
				// al principio del regreso -- esto absorbe automáticamente
				// el suavizado del tramo final y cualquier desviación por
				// que el jugador se haya movido mientras tanto (handPos se
				// recalcula cada tick). shudderDuration + elapsed cubre el
				// margen de asentado del grafo tras Llamada
				// (Constants::kMinCatchAnimationDelay) -- ya garantizado
				// por construcción una vez el vuelo se ha ralentizado lo
				// necesario más arriba, comprobado aquí igualmente por si
				// acaso.
				if (!approachFired) {
					const float closedThisTick = previousDistanceToHand - distanceToHand;
					const float speed = a_deltaSeconds > 0.0f && closedThisTick > 0.0f ? closedThisTick / a_deltaSeconds : 0.0f;

					// Objetivo: no distanceToHand==0, sino el mismo umbral
					// que de verdad usa la condición de llegada dos bloques
					// más abajo (Constants::kReturnArrivalDistance) -- si no,
					// la estimación apunta a una llegada "completa" que el
					// propio bucle nunca espera: el bucle congela la réplica
					// en cuanto cruza ese umbral, antes de que
					// distanceToHand llegue a 0, así que apuntar a 0 dispara
					// onApproaching demasiado tarde y la réplica se queda
					// visiblemente quieta esperando el resto del margen
					// (bug reportado por el usuario, 2026-08-03: "el arma se
					// detiene un momento muy corto pero visible justo antes
					// de llegar a la mano").
					const float remainingDistance = distanceToHand - Constants::kReturnArrivalDistance;
					// std::numeric_limits<float>::max() evitado a propósito:
					// Windows.h define max como macro (mismo problema ya
					// documentado en el proyecto para std::min/std::max, ver
					// BeginReturn más abajo) -- un literal centinela
					// "efectivamente infinito" en su lugar, solo hace falta
					// ser mayor que Constants::kCatchAnimationLeadTime.
					constexpr float kEffectivelyInfinite = 1.0e9f;
					const float     estimatedTimeToArrival = remainingDistance <= 0.0f ? 0.0f : (speed > 0.0f ? remainingDistance / speed : kEffectivelyInfinite);
					const bool      settledSinceCall = shudderDuration + elapsed >= Constants::kMinCatchAnimationDelay;
					if (settledSinceCall && estimatedTimeToArrival <= Constants::kCatchAnimationLeadTime) {
						approachFired = true;
						straightenStart = elapsed;
						straightenBlendFromLocal = Animation::GetSpinLocalRotation(a_refr);
						onApproaching();
					}
				}

				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					logs::info("Return::BeginReturnMovement: la réplica ha llegado a la mano.");
					// Golpe final del atrape: siempre, sin condición (ver
					// Audio::CatchCue::PlayEnd), no depende de que el
					// arranque haya llegado a sonar.
					Audio::CatchCue::PlayEnd(handPos);
					// Red de seguridad: con el vuelo ya ralentizado lo
					// necesario más arriba, esto no debería hacer falta en
					// la práctica, pero garantiza que onApproaching se
					// dispara siempre antes de onArrived si por lo que sea
					// no lo hizo por su cuenta (p. ej. un único tick final
					// demasiado brusco para medir velocidad).
					if (!approachFired) {
						approachFired = true;
						straightenStart = elapsed;
						straightenBlendFromLocal = Animation::GetSpinLocalRotation(a_refr);
						onApproaching();
					}
					onArrived();
					// No se devuelve false aquí -- el bucle sigue vivo
					// pegando la réplica a la mano (ver el chequeo de
					// arrivedFired al principio del tick) hasta que
					// ReequipAndReset lo cancele desde fuera.
					arrivedFired = true;
					return true;
				}

				return true;
			});

			a_callbacks.onTickStarted(token);
		}
	}

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, ReturnCallbacks a_callbacks)
	{
		auto replica = a_replicaHandle.get();
		if (!a_player || !replica) {
			logs::warn("Return::BeginReturn: sin jugador o réplica válida, se aborta el regreso.");
			a_callbacks.onApproaching();
			a_callbacks.onArrived();
			return;
		}

		// Retardo del sonido de arranque del atrape (Audio::CatchCue),
		// calculado una única vez aquí -- antes incluso del temblor de
		// desprendimiento si lo hay -- para que su reloj interno cuente
		// ese temblor (Constants::kStickShudderDuration) además del
		// tramo de movimiento, tal como pidió el usuario. Misma distancia/
		// aceleración/duración prevista que recalculará luego
		// BeginReturnMovement para la curva real -- duplicado a propósito
		// (esta es solo una estimación para el sonido, no necesita
		// compartir estado con el cálculo de la trayectoria real).
		const float initialDistanceForCue = (GetHandPosition(a_player) - replica->GetPosition()).Length();
		const float accelerationForCue = ComputeReturnAcceleration(initialDistanceForCue);
		const float predictedMovementDuration = ComputeReturnDuration(accelerationForCue, initialDistanceForCue);
		const float shudderDuration = a_wasStuck ? Constants::kStickShudderDuration : 0.0f;
		// std::max evitado a propósito: Windows.h define max como macro
		// (mismo problema ya documentado en el proyecto para std::min en
		// WeaponTrail.cpp), así que un ternario aquí en su lugar.
		const float rawStartDelay = shudderDuration + predictedMovementDuration - Constants::kCatchStartSoundLeadTime;
		const float startDelay = rawStartDelay > 0.0f ? rawStartDelay : 0.0f;
		auto        catchCue = std::make_shared<Audio::CatchCue>(startDelay);

		if (!a_wasStuck) {
			BeginReturnMovement(a_player, a_replicaHandle, std::move(a_callbacks), catchCue, shudderDuration);
			return;
		}

		// Punto 11: temblor de desprendimiento antes de iniciar el
		// movimiento de vuelta -- sin mover la réplica (posición y Havok
		// intactos), solo escribe una oscilación de frecuencia y amplitud
		// crecientes en el nodo de giro visual (ver Animation::TickShudder).
		// Al agotarse Constants::kStickShudderDuration, este mismo bucle de
		// tick arranca BeginReturnMovement y termina el suyo devolviendo
		// false -- WeaponState solo ve un token activo cada vez (primero el
		// de este temblor, luego el del movimiento), porque ambos pasan por
		// el mismo a_callbacks.onTickStarted.
		//
		// baseRotation: la rotación del nodo de giro justo en el instante
		// de clavarse (viene de un ángulo de vuelo arbitrario, congelado
		// desde que Throw::LaunchWeapon dejó de llamar a TickSpin sobre esta
		// réplica) -- capturada una única vez aquí para que TickShudder
		// oscile sobre ella en vez de sustituirla por una rotación absoluta
		// desde cero, que producía un salto visual perceptible al empezar
		// el temblor (reportado por el usuario como "cambia de posición").
		RE::NiMatrix3 baseRotation;
		if (auto* root = replica->Get3D()) {
			if (auto* spinNode = root->GetObjectByName(Constants::kWeaponSpinNodeName)) {
				baseRotation = spinNode->local.rotate;
			}
		}

		logs::info("Return::BeginReturn: arma clavada, temblor de desprendimiento ({:.2f}s) antes de regresar.", Constants::kStickShudderDuration);

		auto shudderToken = Physics::StartTickLoop(a_replicaHandle, [a_player, a_replicaHandle, callbacks = a_callbacks, baseRotation, catchCue, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			elapsed += a_deltaSeconds;

			// El arma no se mueve durante el temblor, pero el reloj del
			// sonido de arranque (Audio::CatchCue) sigue contando -- ver
			// Return::BeginReturn para el porqué.
			catchCue->UpdateStart(a_refr.GetPosition(), a_deltaSeconds);

			if (elapsed >= Constants::kStickShudderDuration) {
				BeginReturnMovement(a_player, a_replicaHandle, std::move(callbacks), std::move(catchCue), Constants::kStickShudderDuration);
				return false;
			}

			// TickShudder solo escribe la rotación local del nodo de giro;
			// a diferencia del vuelo (donde Physics::SyncHavok ya llama a
			// Update3DPosition en cada tick porque la posición también
			// cambia), aquí la réplica no se mueve, así que sin esta
			// llamada el motor nunca propaga ese cambio a world.rotate y el
			// temblor no llega a verse aunque el cálculo sea correcto.
			Animation::TickShudder(a_refr, baseRotation, elapsed);
			a_refr.Update3DPosition(true);
			return true;
		});

		a_callbacks.onTickStarted(shudderToken);
	}
}
