// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/CatchSound.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponTrail.h"
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

		// Tiempo real que falta de verdad hasta la llegada, simulando hacia
		// delante en pasos de Constants::kTickDeltaSeconds con las mismas
		// fórmulas que el bucle de tick real (perfil de aceleración +
		// suavizado del tramo final, ComputeTraveledDistance/
		// Math::EvaluateQuadraticBezier) -- a_handPos se asume fija durante
		// la simulación (aproximación razonable para el horizonte corto que
		// hace falta aquí, nunca más de a_maxLookahead).
		//
		// Reemplaza a un cálculo cerrado de un solo paso (tiempo de
		// progreso restante / timeRate de este mismo tick, descartado --
		// bug reportado por el usuario, 2026-08-08, ver CLAUDE.md): asumir
		// que timeRate se queda constante para el resto del vuelo
		// subestimaba el tiempo real que falta de verdad, porque timeRate
		// sigue bajando según la réplica se acerca más
		// (Constants::kReturnTailMinRate es un suelo que se alcanza al
		// final, no un valor ya alcanzado a mitad de trayecto). Esa
		// subestimación disparaba onApproaching demasiado pronto en la
		// práctica: la anotación de Catch.hkx (temporizada sobre esa
		// subestimación) llegaba antes de que la réplica hubiera llegado de
		// verdad a la mano, cortando la animación de Atrape a medias y
		// cancelando el bucle de tick de este archivo antes de que
		// detectara la llegada física por su cuenta -- por eso tampoco
		// llegaba a sonar nunca Audio::CatchCue::PlayEnd (solo se dispara
		// desde dentro de ese bucle, al detectar la llegada). Simular paso
		// a paso con las fórmulas reales, en vez de asumir una tasa
		// constante, no tiene ese sesgo.
		//
		// a_maxLookahead acota la simulación (rendimiento y para no tener
		// que simular el vuelo entero cuando falta mucho de verdad): no
		// hace falta precisión más allá del mayor de los umbrales que
		// consultan los llamantes (Constants::kCatchAnimationLeadTime), así
		// que al alcanzar el límite se devuelve directamente ese límite --
		// cualquier valor por encima del umbral real usado por el llamante
		// sirve igual para una comparación "<=".
		float SimulateRemainingReturnTime(const RE::NiPoint3& a_currentPos, const RE::NiPoint3& a_handPos, const RE::NiPoint3& a_start, const RE::NiPoint3& a_controlPoint, float a_acceleration, float a_initialDistance, float a_progressElapsed, float a_maxLookahead)
		{
			float        progressElapsed = a_progressElapsed;
			RE::NiPoint3 currentPos = a_currentPos;
			float        remaining = 0.0f;

			while (remaining < a_maxLookahead) {
				const float distanceToHand = (a_handPos - currentPos).Length();
				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					return remaining;
				}

				const float tailBlend = Constants::kReturnTailDistance > 0.0f ? std::clamp(distanceToHand / Constants::kReturnTailDistance, 0.0f, 1.0f) : 1.0f;
				const float smoothTailBlend = tailBlend * tailBlend * (3.0f - 2.0f * tailBlend);
				const float timeRate = Constants::kReturnTailMinRate + (1.0f - Constants::kReturnTailMinRate) * smoothTailBlend;

				progressElapsed += Constants::kTickDeltaSeconds * timeRate;
				remaining += Constants::kTickDeltaSeconds;

				const float traveled = ComputeTraveledDistance(a_acceleration, progressElapsed);
				const float t = a_initialDistance > 0.0f ? std::clamp(traveled / a_initialDistance, 0.0f, 1.0f) : 1.0f;
				currentPos = Math::EvaluateQuadraticBezier(a_start, a_controlPoint, a_handPos, t);

				if (t >= 1.0f) {
					return remaining;
				}
			}

			return a_maxLookahead;
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
			// A velocidad/aceleración siempre natural (cambio de criterio
			// 2026-08-07, ver CLAUDE.md): el vuelo de vuelta ya no se
			// ralentiza para dar tiempo a que la animación de Atrape se
			// sincronice -- en distancias medias/cortas eso se notaba poco
			// "poderoso" para un arma como esta (a petición del usuario). La
			// sincronización sigue siendo obligatoria, pero ahora es
			// Return::BeginReturn quien la garantiza por el otro lado,
			// alargando si hace falta el temblor de desprendimiento del
			// punto 11 (a_shudderDuration, ver Animation::TickShudder) en
			// vez de tocar esta aceleración. Sigue acotada por
			// Constants::kReturnMaxDuration (ComputeReturnAcceleration), que
			// es un límite distinto y sin relación con la animación de
			// Atrape -- ver el punto 8 de Mecanica del arma.txt.
			float acceleration = ComputeReturnAcceleration(initialDistance);

			// Excepción, solo si no hubo temblor (a_shudderDuration<=0.0f --
			// a_wasStuck=false en Return::BeginReturn, p. ej. recuperar el
			// arma en pleno vuelo de ida antes de impactar): ahí no hay
			// ningún temblor que alargar para darle al vuelo el mínimo real
			// (Constants::kMinCatchAnimationDelay + kCatchAnimationLeadTime)
			// que necesita la sincronización con Atrape -- sin este mínimo,
			// el propio grafo de animación puede confundirse si Atrape se
			// dispara demasiado pronto tras Llamada (ver kMinCatchAnimationDelay),
			// y además no queda margen real para que el enderezado (más
			// abajo) tenga tiempo de converger antes de la llegada física.
			// Caso raro (el habitual es recuperar con el arma ya clavada,
			// que siempre tiene temblor de sobra) así que esta ralentización
			// puntual no contradice la decisión de no ralentizar el caso
			// normal.
			float       estimatedDuration = ComputeReturnDuration(acceleration, initialDistance);
			const float naturalDuration = estimatedDuration;
			if (a_shudderDuration <= 0.0f) {
				const float requiredMovementDuration = Constants::kMinCatchAnimationDelay + Constants::kCatchAnimationLeadTime;
				if (estimatedDuration < requiredMovementDuration) {
					acceleration = ComputeReturnAccelerationForDuration(initialDistance, requiredMovementDuration);
					estimatedDuration = requiredMovementDuration;
					logs::info(
						"Return::BeginReturnMovement: regreso sin temblor y demasiado corto ({:.2f}s) -- ralentizado a {:.2f}s para dejar margen a la sincronización con Atrape.",
						naturalDuration, estimatedDuration);
				}
			}

			const auto controlPoint = ComputeReturnControlPoint(start, initialHandPos, GetPlayerRightVector(a_player), Constants::kReturnCurveAnchorFraction);

			logs::info(
				"Return::BeginReturnMovement: distancia inicial {:.1f}, aceleración {:.1f}, duración estimada {:.2f}s",
				initialDistance, acceleration, estimatedDuration);

			// Sin silbido de lanzamiento aquí (retirado 2026-08-08, a
			// petición del usuario): sonaba a la vez que el propio sonido
			// de arranque del atrape (Audio::CatchCue), duplicando/
			// solapando innecesariamente -- el silbido queda reservado
			// solo para el lanzamiento real (Throw::LaunchWeapon).

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

			// Estela de rayo (ver Constants.h, "-- Estela de rayo durante
			// el vuelo --"): creada aquí, no en el temblor de
			// desprendimiento previo (BeginReturn -- ahí la réplica no se
			// mueve, no tiene sentido muestrear posición todavía).
			auto trail = std::make_shared<Animation::WeaponTrail>();
			trail->Start(replica->GetParentCell(), start);

			auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, rootWorld, movementBaseLocal, trail, onArrived = a_callbacks.onArrived, onApproaching = a_callbacks.onApproaching, shudderDuration = a_shudderDuration, catchTriggered = false, straightening = false, arrivedFired = false, straightenStart = 0.0f, straightenDuration = Constants::kSpinStraightenLeadTime, straightenBlendFromLocal = RE::NiMatrix3{}, elapsed = 0.0f, progressElapsed = 0.0f, hitActors = std::vector<RE::ActorHandle>{}, catchCue = std::move(a_catchCue), loggedHandAxisDiagnostic = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
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
				// durante la ventana de enderezado (más abajo,
				// Constants::kSpinStraightenLeadTime antes de la llegada
				// real -- disparador independiente de onApproaching, ver
				// CLAUDE.md 2026-08-07), donde Animation::TickSpinStraighten
				// la sustituye para que el giro llegue frenado y alineado
				// con la orientación real de la mano en vez de en mitad de
				// una vuelta cualquiera (segunda mitad del punto 10, bug
				// reportado por el usuario, 2026-08-04: el cambio a la pose
				// real se notaba como un salto brusco de rotación). El
				// objetivo se recalcula cada tick (GetHandBoneWorldRotation,
				// no una foto fija tomada al empezar la ventana) porque el
				// jugador puede seguir girando mientras dura -- así el
				// enderezado siempre converge a la orientación real en el
				// instante exacto de la llegada, no a la que tuviera la
				// mano medio segundo antes.
				if (straightening) {
					const float         blend = (elapsed - straightenStart) / straightenDuration;
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

				// Estela de rayo: deja de alimentarse en cuanto
				// arrivedFired sea true (chequeo al principio del bucle,
				// más arriba) -- los segmentos ya añadidos se apagan solos
				// por su propio Constants::kTrailSegmentLifetime, no hace
				// falta seguir muestreando la posición estática de la mano.
				trail->Update(nextPos, a_deltaSeconds);

				const float distanceToHand = (handPos - nextPos).Length();

				// Sincronización en vivo del gesto de Atrape con el vuelo
				// real (a petición del usuario, 2026-08-03: nunca
				// desacoplar animación y física con un temporizador
				// precalculado de antemano) -- tiempo real que falta de
				// verdad calculado por simulación hacia delante
				// (SimulateRemainingReturnTime, ver ese comentario para el
				// porqué de simular en vez de estimar con una fórmula de un
				// solo paso), exacto en vez de aproximado.
				//
				// Dos disparadores independientes a partir del mismo tiempo
				// restante simulado (cambio de criterio 2026-08-07, ver
				// CLAUDE.md): onApproaching (arranca el gesto de Atrape,
				// Constants::kCatchAnimationLeadTime antes de la llegada --
				// atado a la duración real de Catch.hkx, no negociable) y el
				// inicio visual del enderezado (Constants::kSpinStraightenLeadTime
				// antes, deliberadamente mucho más tarde/corto) ya NO
				// comparten instante. Compartirlo hacía que la ventana de
				// enderezado (kCatchAnimationLeadTime, 0,5s) consumiera la
				// mayor parte -- o la totalidad -- de un regreso corto o
				// medio, y el arma apenas llegaba a girar (bug reportado por
				// el usuario: "el enderezado se produce desde muy lejos").
				if (!catchTriggered || !straightening) {
					// Por encima del mayor umbral real que se consulta más
					// abajo (kCatchAnimationLeadTime + kCatchApproachSafetyMargin)
					// con su propio margen -- si el valor devuelto fuera
					// exactamente igual al umbral (limitado por este tope en
					// vez de calculado de verdad), una comparación "<=" podría
					// disparar por error.
					constexpr float kLookaheadCap = Constants::kCatchAnimationLeadTime + Constants::kCatchApproachSafetyMargin + 0.1f;
					const float     estimatedTimeToArrival = SimulateRemainingReturnTime(nextPos, handPos, start, controlPoint, acceleration, initialDistance, progressElapsed, kLookaheadCap);

					if (!catchTriggered) {
						const bool settledSinceCall = shudderDuration + elapsed >= Constants::kMinCatchAnimationDelay;
						// + Constants::kCatchApproachSafetyMargin (2026-08-08,
						// ver CLAUDE.md y ese comentario): confirmado con logs
						// reales que, sin este margen, la anotación real de
						// Catch.hkx (temporización fija) y la detección
						// interna de llegada física de este mismo bucle
						// competían por quién llegaba primero -- la simulación
						// es exacta a pocos milisegundos, pero eso no basta
						// frente al jitter real de un tick, y la anotación
						// ganaba la carrera con la frecuencia suficiente como
						// para que Audio::CatchCue::PlayEnd casi nunca sonara.
						if (settledSinceCall && estimatedTimeToArrival <= Constants::kCatchAnimationLeadTime + Constants::kCatchApproachSafetyMargin) {
							catchTriggered = true;
							logs::info(
								"Return::BeginReturnMovement: onApproaching disparado -- elapsed={:.3f}s, distanceToHand={:.1f}, estimatedTimeToArrival={:.3f}s.",
								elapsed, distanceToHand, estimatedTimeToArrival);
							onApproaching();
						}
					}

					if (!straightening && estimatedTimeToArrival <= Constants::kSpinStraightenLeadTime) {
						straightening = true;
						straightenStart = elapsed;
						// La ventana de enderezado dura lo que quede de
						// verdad hasta la llegada (misma estimación en vivo
						// de arriba), no siempre Constants::kSpinStraightenLeadTime
						// tal cual: con la aceleración de llegada constante
						// (Constants::kReturnTargetArrivalSpeed), un regreso
						// muy corto puede tener menos tiempo real restante
						// que esa constante, y forzarla igual dejaría el
						// enderezado a medias cuando la llegada física ya se
						// hubiera disparado. Suelo pequeño para evitar una
						// ventana de longitud ~0 (blend saltando
						// prácticamente de golpe en vez de con la curva
						// suave de TickSpinStraighten).
						constexpr float kMinStraightenBlendDuration = 0.05f;
						straightenDuration = estimatedTimeToArrival > kMinStraightenBlendDuration ? estimatedTimeToArrival : kMinStraightenBlendDuration;
						straightenBlendFromLocal = Animation::GetSpinLocalRotation(a_refr);
					}
				}

				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					logs::info("Return::BeginReturnMovement: la réplica ha llegado a la mano.");
					// Golpe final del atrape: siempre, sin condición (ver
					// Audio::CatchCue::PlayEnd), no depende de que el
					// arranque haya llegado a sonar.
					Audio::CatchCue::PlayEnd(handPos);
					// Redes de seguridad: con el vuelo ya ajustado lo
					// necesario más arriba, esto no debería hacer falta en
					// la práctica, pero garantiza que tanto onApproaching
					// como el enderezado visual se disparen siempre antes de
					// onArrived si por lo que sea no lo hicieron por su
					// cuenta (p. ej. un tramo tan corto que ni un solo tick
					// intermedio llegó a evaluar el disparador en vivo).
					if (!catchTriggered) {
						catchTriggered = true;
						onApproaching();
					}
					if (!straightening) {
						straightening = true;

						// Sin margen real para ninguna ventana de enderezado
						// progresivo (se está detectando la llegada en el
						// mismo tick, sin haber pasado antes por la
						// estimación en vivo de arriba) -- y una vez
						// arrivedFired se ponga a true dos líneas más abajo,
						// el bucle ya no vuelve a llamar a
						// TickSpin/TickSpinStraighten nunca más (ver el
						// chequeo de arrivedFired al principio del tick), así
						// que esperar a que el "siguiente tick" complete el
						// fundido no funciona -- no hay siguiente tick. Se
						// fuerza aquí mismo el enderezado completo (blend=1,
						// sin fundido visible) en vez de dejar la última pose
						// de TickSpin de este mismo tick (ya calculada más
						// arriba, antes de saber que llegaba): un salto de
						// rotación sin fundir es preferible a que el arma se
						// quede definitivamente a mitad de girar (bug
						// reportado por el usuario, 2026-08-07: "llega de
						// cualquier manera").
						const RE::NiMatrix3 handTargetLocal = Math::LocalRotationFromWorld(rootWorld, Animation::GetHandBoneWorldRotation(*a_player));
						Animation::TickSpinStraighten(a_refr, Animation::GetSpinLocalRotation(a_refr), handTargetLocal, 1.0f);
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

		// Distancia/aceleración/duración prevista del tramo de movimiento --
		// misma fórmula natural que recalculará luego BeginReturnMovement
		// para la curva real (duplicado a propósito, esta es solo una
		// estimación, no necesita compartir estado con el cálculo real).
		// Ya no se ralentiza para sincronizar con Atrape (ver
		// BeginReturnMovement/CLAUDE.md, 2026-08-07) -- se usa tal cual,
		// tanto para el retardo del sonido de arranque del atrape como para
		// calcular más abajo cuánto hay que alargar el temblor.
		const float initialDistanceForCue = (GetHandPosition(a_player) - replica->GetPosition()).Length();
		const float accelerationForCue = ComputeReturnAcceleration(initialDistanceForCue);
		const float predictedMovementDuration = ComputeReturnDuration(accelerationForCue, initialDistanceForCue);

		// Duración real del temblor de desprendimiento (punto 11): antes
		// fija (Constants::kStickShudderDuration), ahora su suelo -- si el
		// tramo de movimiento (ya a velocidad natural, sin ralentizar) no
		// dejaría tiempo suficiente para que la animación de Atrape se
		// sincronice de verdad, se alarga el temblor lo necesario en vez de
		// tocar la velocidad del vuelo (cambio de criterio 2026-08-07, ver
		// CLAUDE.md y BeginReturnMovement). El total (temblor + movimiento)
		// requerido es el mismo de siempre (Constants::kMinCatchAnimationDelay
		// + Constants::kCatchAnimationLeadTime, ver el análisis en
		// BeginReturnMovement de versiones anteriores de este archivo) --
		// solo cambia qué lado de la suma se ajusta para cubrirlo. Sin
		// efecto si el arma no estaba clavada (a_wasStuck=false): no hay
		// temblor que alargar en ese caso (auto-recall a media parábola,
		// caso raro), así que la sincronización ahí depende solo de la
		// estimación en vivo de BeginReturnMovement (onApproaching).
		const float requiredTotalForSettle = Constants::kMinCatchAnimationDelay + Constants::kCatchAnimationLeadTime;
		const float shudderDeficit = requiredTotalForSettle - predictedMovementDuration;
		const float shudderDuration = a_wasStuck ?
		                                   (shudderDeficit > Constants::kStickShudderDuration ? shudderDeficit : Constants::kStickShudderDuration) :
		                                   0.0f;

		// Retardo del sonido de arranque del atrape (Audio::CatchCue),
		// calculado una única vez aquí -- antes incluso del temblor de
		// desprendimiento si lo hay -- para que su reloj interno cuente ese
		// temblor (ya con su duración real, posiblemente alargada) además
		// del tramo de movimiento, tal como pidió el usuario.
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
		// Al agotarse shudderDuration (calculado arriba -- Constants::
		// kStickShudderDuration como mínimo), este mismo bucle de tick
		// arranca BeginReturnMovement y termina el suyo devolviendo false --
		// WeaponState solo ve un token activo cada vez (primero el de este
		// temblor, luego el del movimiento), porque ambos pasan por el
		// mismo a_callbacks.onTickStarted.
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

		logs::info("Return::BeginReturn: arma clavada, temblor de desprendimiento ({:.2f}s, mínimo {:.2f}s) antes de regresar.", shudderDuration, Constants::kStickShudderDuration);

		auto shudderToken = Physics::StartTickLoop(a_replicaHandle, [a_player, a_replicaHandle, callbacks = a_callbacks, baseRotation, catchCue, shudderDuration, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			elapsed += a_deltaSeconds;

			// El arma no se mueve durante el temblor, pero el reloj del
			// sonido de arranque (Audio::CatchCue) sigue contando -- ver
			// Return::BeginReturn para el porqué.
			catchCue->UpdateStart(a_refr.GetPosition(), a_deltaSeconds);

			if (elapsed >= shudderDuration) {
				BeginReturnMovement(a_player, a_replicaHandle, std::move(callbacks), std::move(catchCue), shudderDuration);
				return false;
			}

			// TickShudder solo escribe la rotación local del nodo de giro;
			// a diferencia del vuelo (donde Physics::SyncHavok ya llama a
			// Update3DPosition en cada tick porque la posición también
			// cambia), aquí la réplica no se mueve, así que sin esta
			// llamada el motor nunca propaga ese cambio a world.rotate y el
			// temblor no llega a verse aunque el cálculo sea correcto.
			Animation::TickShudder(a_refr, baseRotation, elapsed, shudderDuration);
			a_refr.Update3DPosition(true);
			return true;
		});

		a_callbacks.onTickStarted(shudderToken);
	}
}
