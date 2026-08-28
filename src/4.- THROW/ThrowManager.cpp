// Implementación del sistema de lanzamiento.
// Controla la creación, activación y seguimiento del arma lanzada.

#include "4.- THROW/ThrowManager.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponGlow.h"
#include "8.- ANIMATION/WeaponImpactVFX.h"
#include "8.- ANIMATION/WeaponTrailGroup.h"
#include "9.- MATH/RotationMath.h"

#include <cmath>
#include <numbers>
#include <optional>

namespace Throw
{
	namespace
	{
		// Punto de origen del lanzamiento: el nodo del arma en la mano
		// derecha, para que la réplica aparezca en la misma posición que el
		// arma (Mecanica del arma.txt, punto 2), no en la cámara.
		RE::NiPoint3 GetLaunchOrigin(RE::Actor* a_shooter)
		{
			if (auto* weaponNode = a_shooter->GetNodeByName("WEAPON")) {
				return weaponNode->world.translate;
			}

			return a_shooter->GetPosition();
		}

		RE::NiPoint3 GetCameraPosition()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			return camera && camera->cameraRoot ? camera->cameraRoot->world.translate : RE::NiPoint3{};
		}

		RE::NiPoint3 GetCameraForward()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !camera->cameraRoot) {
				return { 0.0f, 1.0f, 0.0f };
			}

			return camera->cameraRoot->world.rotate.GetVectorY();
		}

		// Ángulo de tiro (pitch, radianes) que hace que una parábola con
		// velocidad Constants::kThrowInitialSpeed y gravedad constante
		// Constants::kThrowGravity pase exactamente por el punto apuntado,
		// dada su distancia horizontal y diferencia de altura respecto al
		// origen -- solución cerrada de "Solving Ballistic Trajectories"
		// (forrestthewoods.com), caso de velocidad fija / objetivo
		// estático: tan(θ) = (v² ± √(v⁴ − g·(g·x² + 2·y·v²))) / (g·x).
		// Se toma siempre la raíz de arco bajo (signo -), la más directa --
		// la de arco alto queda "cómicamente alta" a media/larga distancia
		// (razón dada en el propio artículo, y no encaja con un lanzamiento
		// de martillo). La fórmula asume gravedad constante desde el
		// instante cero -- ComputeGravityDrop, más abajo, ya no aplica
		// ninguna rampa de gravedad por este mismo motivo (ver su
		// comentario).
		//
		// Sin solución real (discriminante negativo, objetivo fuera del
		// alcance máximo que esa velocidad puede cubrir) devuelve
		// nullopt -- no ocurre dentro de Constants::kAimRaycastDistance
		// con las constantes actuales (alcance máximo teórico v²/g ≈ 8398
		// unidades, por encima de las 6000 de kAimRaycastDistance, que
		// solo limita hasta dónde se busca el punto al que apunta la
		// mirilla, sin relación con si la parábola llega o no), pero se
		// contempla por seguridad para cualquier ajuste futuro de las
		// constantes.
		std::optional<float> SolveLowArcPitch(float a_horizontalDistance, float a_heightDiff)
		{
			constexpr float speed = Constants::kThrowInitialSpeed;
			constexpr float gravity = -Constants::kThrowGravity;  // magnitud positiva

			const float speedSq = speed * speed;
			const float discriminant = speedSq * speedSq - gravity * (gravity * a_horizontalDistance * a_horizontalDistance + 2.0f * a_heightDiff * speedSq);
			if (discriminant < 0.0f) {
				return std::nullopt;
			}

			const float tangent = (speedSq - std::sqrt(discriminant)) / (gravity * a_horizontalDistance);
			return std::atan(tangent);
		}

		// Corrección de paralaje cámara/mano (fallo detectado en la
		// iteración anterior: usar la dirección de la cámara tal cual,
		// aplicada desde el origen en la mano, no converge en el punto al
		// que apunta la mirilla). Se calcula primero el punto real al que
		// apunta la mirilla con un raycast desde la cámara hasta
		// Constants::kAimRaycastDistance, y la dirección horizontal de
		// lanzamiento va desde el origen en la mano hacia ese punto — así
		// el origen visual coincide con lo que el jugador ve en la
		// mirilla, sea cual sea la distancia. El pitch (componente
		// vertical) ya no apunta en línea recta al punto: se resuelve con
		// SolveLowArcPitch para que la parábola completa termine ahí, no
		// solo la línea recta inicial (sin esto, el arma cae muy por
		// debajo de la mirilla a corta/media distancia, ver CHANGELOG.md).
		RE::NiPoint3 ComputeAimedDirection(RE::Actor* a_shooter, const RE::NiPoint3& a_origin)
		{
			const auto cameraPos = GetCameraPosition();
			const auto forward = GetCameraForward();
			const auto rayEnd = cameraPos + forward * Constants::kAimRaycastDistance;

			const auto hit = Collision::Raycast(cameraPos, rayEnd, a_shooter);
			const auto aimPoint = hit.hit ? hit.point : rayEnd;

			const RE::NiPoint3 toAimPoint = aimPoint - a_origin;
			const RE::NiPoint3 horizontal{ toAimPoint.x, toAimPoint.y, 0.0f };
			const float        horizontalDistance = horizontal.Length();

			// Tiro (casi) vertical puro, u objetivo fuera del alcance
			// balístico (SolveLowArcPitch devuelve nullopt): apuntar en
			// línea recta al punto en vez de resolver el ángulo -- ver
			// comentario de SolveLowArcPitch.
			const auto pitch = horizontalDistance > 1.0f ? SolveLowArcPitch(horizontalDistance, toAimPoint.z) : std::nullopt;
			if (!pitch) {
				const float length = toAimPoint.Length();
				return length > 0.0f ? toAimPoint / length : forward;
			}

			const RE::NiPoint3 horizontalDir = horizontal / horizontalDistance;
			return horizontalDir * std::cos(*pitch) + RE::NiPoint3{ 0.0f, 0.0f, 1.0f } * std::sin(*pitch);
		}

		// Gravedad constante desde el instante cero (posición(t) = origen +
		// velocidad0·t + ½·gravedad·t²) -- ya no hay rampa de arranque
		// (Mejora Kratos #1, retirada 2026-08-05, ver CHANGELOG.md): la
		// mantenía "plana" al salir de la mano, pero Throw::SolveLowArcPitch
		// necesita gravedad constante desde t=0 para que la parábola
		// resuelta pase exactamente por el punto apuntado (ver esa
		// función) -- con rampa, quedaba un residuo por debajo de la
		// mirilla en tiros cortos.
		float ComputeGravityDrop(float a_elapsed)
		{
			return 0.5f * Constants::kThrowGravity * a_elapsed * a_elapsed;
		}
	}

	void LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon, LaunchCallbacks a_callbacks)
	{
		if (!a_shooter || !a_weapon) {
			a_callbacks.onSpawned({});
			return;
		}

		const auto         origin = GetLaunchOrigin(a_shooter);
		const auto         direction = ComputeAimedDirection(a_shooter, origin);
		const RE::NiPoint3 velocity0 = direction * Constants::kThrowInitialSpeed;

		// Sonido del silbido de lanzamiento: disparado aquí mismo, síncrono,
		// en vez de dentro del callback de Physics::SpawnReplica más abajo
		// (bug reportado por el usuario, 2026-08-08: sonaba demasiado
		// tarde) -- solo necesita origin, ya conocido en este punto, no
		// hace falta esperar a que el 3D de la réplica cargue (~unos pocos
		// Constants::kTickInterval de retraso real, ver Physics::SpawnReplica)
		// para reproducirlo.
		Audio::PlayReliableOneShot(origin, Constants::kThrowLaunchSoundLocalFormID, Constants::kThrowLaunchSoundEditorID);

		// Punto de partida real del giro (ver CLAUDE.md, "Arquitectura de
		// física de proyectiles"): la rotación mundial que tenía la malla
		// del arma equipada un instante antes de convertirse en réplica --
		// captura síncrona, antes de que Physics::SpawnReplica arranque la
		// espera asíncrona por el 3D de la réplica, así que sigue siendo
		// la última pose real visible aunque WeaponManager::ThrowWeapon ya
		// la haya ocultado (SetEquippedWeaponHidden no toca la
		// transformación, solo la visibilidad).
		const RE::NiMatrix3 capturedWeaponWorldRotation = Animation::GetEquippedWeaponWorldRotation(*a_shooter);

		Physics::SpawnReplica(a_shooter, a_weapon, origin, [a_shooter, a_weapon, origin, velocity0, capturedWeaponWorldRotation, callbacks = a_callbacks](RE::ObjectRefHandle a_handle) {
			callbacks.onSpawned(a_handle);

			if (!a_handle.get()) {
				logs::warn("Throw::LaunchWeapon: la réplica nunca cargó su 3D, se aborta el lanzamiento.");
				return;
			}

			logs::info("Throw::LaunchWeapon: réplica lista, iniciando vuelo parabólico.");

			// Rotación LOCAL (respecto al nodo raíz de la réplica) que
			// reproduce exactamente la pose capturada -- ver
			// Math::LocalRotationFromWorld. rootWorld es la rotación
			// mundial del nodo raíz en el instante de creación, constante
			// durante toda la vida de la réplica (nadie llama SetAngle
			// sobre ella, ver CLAUDE.md), así que basta con leerla una vez
			// aquí. TickSpin compone esta base sobre el giro calculado
			// durante TODO el tramo de vuelo, no solo al principio -- ver
			// WeaponAnimation.h (bug "se aplana momentos después",
			// 2026-08-06).
			RE::NiMatrix3 launchBaseLocal;
			// Estela de rayo (ver Constants.h, "-- Estela de rayo durante
			// el vuelo --"): un único WeaponTrail para todo el tramo,
			// creado aquí junto al resto del estado de arranque y
			// capturado por la lambda del bucle de tick de abajo -- nunca
			// un Physics::StartTickLoop propio para el trail.
			auto trail = std::make_shared<Animation::WeaponTrailGroup>();
			if (auto replica = a_handle.get()) {
				// Plano de la estela (ver WeaponTrail.h, a_upReference).
				// Primer intento (2026-08-26, versión anterior de esta
				// misma sesión): el eje Z del nodo raíz de la réplica,
				// fijo durante todo el vuelo -- arregló el desencaje de
				// plano contra el ángulo del arma, pero como la parábola
				// gira dentro de su propio plano vertical (eje X/Y
				// constante, solo cae en Z -- ComputeGravityDrop), un eje
				// fijo que no está garantizado perpendicular a ESE plano
				// obliga a la cinta a "bancarse"/torcerse según la
				// trayectoria se inclina, más notorio cuanto más lejos del
				// ángulo de salida (la cola, la parte más antigua) --
				// confirmado con datos reales del log que las posiciones
				// centrales de la estela son perfectamente rectas en
				// X/Y, así que el "escorado hacia la izquierda" reportado
				// por el usuario no podía ser la trayectoria, solo la
				// orientación de la cinta.
				//
				// Arreglo: en vez de un eje del arma, la normal real del
				// plano de la parábola (perpendicular a la dirección de
				// vuelo Y al eje vertical del mundo a la vez) -- por
				// construcción, la dirección de vuelo nunca sale de ese
				// plano en toda la ida, así que la cinta no se banca en
				// absoluto, curve la parábola lo que curve.
				//
				// Segunda vuelta (mismo día, reportado tras subir
				// Constants::kTrailSegmentScale): con solo la normal de
				// trayectoria, la cinta ya no tiene NINGÚN grado de
				// libertad relacionado con el ángulo real del arma (esa
				// normal es geometría pura de la parábola) -- se veía
				// "definitivamente desalineada" en cuanto se hizo más
				// grande. Tercera vuelta: derivarlo del eje Z real del
				// arma (Math::ComputeRoll) dio resultados poco fiables
				// (plano equivocado, invertido, "vuelve a verse curvo") --
				// Constants::kTrailRollDegrees (ángulo fijo dado
				// directamente por el usuario) sustituye ese cálculo. El
				// único ángulo (no la referencia de plano en sí) es lo que
				// se mantiene fijo el resto del vuelo, así que encaja con
				// el ángulo deseado sin volver a bancarse.
				RE::NiPoint3 trailUpReference = velocity0.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
				const float  trailUpLength = trailUpReference.Length();
				trailUpReference = trailUpLength > 0.0f ? trailUpReference / trailUpLength : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };

				const float trailRoll = Constants::kTrailRollDegrees * std::numbers::pi_v<float> / 180.0f;

				// Punto de anclaje (ver WeaponTrail.h, a_anchorWorldOffset):
				// el nodo raíz de la réplica cae en la base del mango, no
				// en el centro visual del hacha (confirmado en NifSkope,
				// ver Constants::kTrailAnchorLocalOffset) -- transformado
				// por rootWorld (constante en vuelo, no el nodo de giro)
				// para que el offset se mueva rígido con el arma sin
				// orbitar con Animation::TickSpin.
				RE::NiPoint3 trailAnchorWorldOffset{ 0.0f, 0.0f, 0.0f };

				if (auto* node3D = replica->Get3D()) {
					const RE::NiMatrix3 rootWorld = node3D->world.rotate;
					launchBaseLocal = Math::LocalRotationFromWorld(rootWorld, capturedWeaponWorldRotation);
					trailAnchorWorldOffset = rootWorld * Constants::kTrailAnchorLocalOffset;

					// Esta llamada a elapsed=0 solo evita que el nodo de
					// giro muestre la rotación de reposo del NIF durante el
					// hueco real (~un Constants::kTickInterval) hasta que
					// el bucle de abajo dispare su primer tick.
					Animation::TickSpin(*replica, 0.0f, launchBaseLocal);
				}

				trail->Start(replica->GetParentCell(), replica->GetPosition(), trailUpReference, trailRoll, trailAnchorWorldOffset);
			}

			// Trayectoria parabólica propia (punto 3): posición(t) =
			// origen + velocidad0·t + ½·gravedad·t², sin depender de Havok
			// (la réplica está en modo kKeyframed, sin fuerzas/gravedad
			// del motor). Forma cerrada en vez de acumular velocidad tick
			// a tick, para no arrastrar deriva numérica.
			auto token = Physics::StartTickLoop(a_handle, [a_shooter, a_handle, origin, velocity0, launchBaseLocal, trail, onStuck = callbacks.onStuck, onAutoRecall = callbacks.onAutoRecall, onTickStarted = callbacks.onTickStarted, elapsed = 0.0f, loggedFirstGravitySample = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				// Punto 10: se calcula y escribe el giro a mano cada tick
				// (ver Animation::TickSpin), compuesto permanentemente
				// sobre launchBaseLocal -- la pose real que tenía el arma
				// al salir de la mano marca su rotación durante todo el
				// vuelo, no solo los primeros instantes.
				Animation::TickSpin(a_refr, elapsed, launchBaseLocal);

				const float gravityDrop = ComputeGravityDrop(elapsed);

				// Log de verificación campo a campo, solo el primer tick
				// (no en cada uno, para no inundar el log).
				if (!loggedFirstGravitySample) {
					logs::info("Throw::LaunchWeapon: ComputeGravityDrop primer tick t={:.3f}s -> drop={:.2f}", elapsed, gravityDrop);
					loggedFirstGravitySample = true;
				}

				RE::NiPoint3 nextPos = origin + velocity0 * elapsed;
				nextPos.z += gravityDrop;

				// Colisión "gruesa" (varios rayos en cruz, ver
				// Collision::SweepRaycast) desde la posición anterior a la
				// siguiente, no solo un punto ni un único rayo fino: a la
				// velocidad del lanzamiento, un rayo infinitamente fino
				// podía pasar de largo junto a geometría irregular o un
				// actor en movimiento, y además clavarse más hundido en la
				// malla (comprobado en el juego). Se ignoran el lanzador y
				// la propia réplica (CFilter no permite excluirlos de la
				// consulta, ver CLAUDE.md).
				const auto hit = Collision::SweepRaycast(previousPos, nextPos, Constants::kThrowCollisionRadius, a_shooter, &a_refr);
				if (hit.hit) {
					auto* actor = hit.target ? hit.target->As<RE::Actor>() : nullptr;

					const auto  travel = nextPos - previousPos;
					const float travelLength = travel.Length();
					const auto  travelDir = travelLength > 0.0f ? travel / travelLength : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };

					// El punto del rayo es donde la línea (infinitamente
					// fina) cruza la superficie golpeada. Contra una
					// superficie normal, el origen del modelo puesto justo
					// ahí deja parte de la malla del arma (con volumen
					// real) hundida dentro de ella, así que se retrocede
					// (comprobado en el juego). Contra un actor es al
					// revés: la capa golpeada (CharController) es una
					// cápsula de colisión más grande que la malla visual
					// real, muy notable en objetivos pequeños — retroceder
					// igual que con una pared deja el arma flotando lejos
					// del cuerpo, así que en vez de eso se avanza
					// (comprobado en el juego).
					const auto stickPoint = actor ?
					                            hit.point + travelDir * Constants::kActorStickForwardOffset :
					                            hit.point - travelDir * Constants::kStickEmbedBackoff;

					a_refr.SetPosition(stickPoint);
					Physics::SyncHavok(a_refr, stickPoint, a_refr.GetAngle());
					trail->Update(stickPoint, a_deltaSeconds);
					logs::info("Throw::LaunchWeapon: impacto en ({:.1f},{:.1f},{:.1f})", hit.point.x, hit.point.y, hit.point.z);

					// VFX de impacto -- fire-and-forget, ver
					// Animation::SpawnImpactVFX. Se dispara aquí tanto
					// contra actor como contra superficie (a petición del
					// usuario, solo en el impacto de la ida, no en los
					// golpes de paso del regreso en ReturnManager.cpp).
					//
					// Anclado a la cabeza del martillo (Animation::
					// GetGlowAnchorPosition, mismo mecanismo ya usado por
					// el destello -- nodo "Gold" + Constants::
					// kGlowAnchorLocalOffset), no en stickPoint crudo: el
					// nodo raíz de la réplica (lo que stickPoint posiciona)
					// cae en la base del mango del modelo, no en la cabeza
					// (ver CLAUDE.md) -- a petición del usuario, para que
					// el destello nazca de donde golpea de verdad el arma,
					// no del mango. a_refr ya tiene su 3D actualizado en
					// este punto (SetPosition+SyncHavok justo arriba).
					const auto impactVfxPosition = a_refr.Get3D() ? Animation::GetGlowAnchorPosition(a_refr.Get3D()) : stickPoint;
					Animation::SpawnImpactVFX(a_refr, impactVfxPosition);

					// Punto 10 (segunda mitad, caso impacto): eliminado el
					// enderezado al clavarse (decisión del usuario,
					// 2026-08-08, ver Constants::kSpinStraightenLeadTime
					// para el porqué) -- el arma se queda congelada en el
					// ángulo de vuelo arbitrario que tuviera al golpear, sin
					// ningún ajuste posterior.
					//
					// Punto 6: contra un actor, no basta con detenerse — hay
					// que aplicar daño/parálisis y seguir su posición
					// mientras el arma siga clavada
					// (Combat::BeginEmbeddedEffect arranca su propio bucle
					// de tick, sustituyendo a este, y decide si llamar a
					// onStuck —clavada de verdad— o a onAutoRecall —objetivo
					// inmune, p. ej. un dragón—). Contra una superficie no
					// hay enemigo que seguir, así que basta con marcarla
					// como clavada aquí mismo.
					if (actor) {
						Combat::BeginEmbeddedEffect(a_shooter, actor, a_handle, onStuck, onAutoRecall, onTickStarted);
					} else {
						onStuck(RE::ActorHandle{});
					}

					return false;
				}

				// El agua no es una superficie donde clavarse (caso no
				// cubierto por el documento): se trata igual que no
				// impactar contra nada (punto 5).
				if (a_refr.IsInWater()) {
					logs::info("Throw::LaunchWeapon: ha caído al agua, recuperando automáticamente.");
					onAutoRecall();
					return false;
				}

				a_refr.SetPosition(nextPos);
				Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());
				trail->Update(nextPos, a_deltaSeconds);
				return true;
			});

			callbacks.onTickStarted(token);
		});
	}
}
