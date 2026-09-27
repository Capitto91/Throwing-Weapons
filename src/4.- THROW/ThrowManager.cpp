// Implementación del sistema de lanzamiento.
// Controla la creación, activación y seguimiento del arma lanzada.

#include "4.- THROW/ThrowManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
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

		// Dirección de lanzamiento estilo flecha vanilla (2026-09-27, a
		// petición del usuario): línea recta desde el origen en la mano
		// hacia el punto al que apunta la mirilla, sin ninguna compensación
		// balística -- la gravedad hace caer el arma por debajo de la
		// mirilla a media/larga distancia y es el jugador quien tiene que
		// apuntar más alto, igual que con un arco (Projectile::LaunchData
		// solo recibe origen + ángulos, nunca un punto de destino).
		// Sustituye a SolveLowArcPitch, que resolvía el ángulo para que la
		// parábola cayera exactamente en la mirilla.
		//
		// El raycast desde la cámara se mantiene (corrección de paralaje
		// cámara/mano, fallo detectado en la iteración anterior): el origen
		// está en la mano, no en la cámara, así que usar la dirección de la
		// cámara tal cual no pasaría por la mirilla ni siquiera sin
		// gravedad. Se busca primero el punto real bajo la mirilla hasta
		// Constants::kAimRaycastDistance y se apunta desde la mano hacia él.
		RE::NiPoint3 ComputeAimedDirection(RE::Actor* a_shooter, const RE::NiPoint3& a_origin)
		{
			const auto cameraPos = GetCameraPosition();
			const auto forward = GetCameraForward();
			const auto rayEnd = cameraPos + forward * Constants::kAimRaycastDistance;

			const auto hit = Collision::Raycast(cameraPos, rayEnd, a_shooter);
			const auto aimPoint = hit.hit ? hit.point : rayEnd;

			const RE::NiPoint3 toAimPoint = aimPoint - a_origin;
			const float        length = toAimPoint.Length();
			return length > 0.0f ? toAimPoint / length : forward;
		}

		// Gravedad real del mundo de Havok (componente Z, en unidades de
		// juego/s², negativa) de la celda del lanzador: hkpWorld::gravity
		// está en unidades de Havok, se pasa a unidades de juego dividiendo
		// por bhkWorld::GetWorldScale() (mismo factor que ya usa
		// Physics::SyncHavok en sentido contrario). Devuelve
		// Constants::kThrowFallbackWorldGravity si no hay mundo de Havok
		// accesible o el valor leído no es válido. Registra en el log el
		// primer valor leído, para confirmar la gravedad real del juego.
		float GetWorldGravity(RE::Actor* a_shooter)
		{
			auto* cell = a_shooter->GetParentCell();
			auto* bhkWorld = cell ? cell->GetbhkWorld() : nullptr;
			auto* world = bhkWorld ? bhkWorld->GetWorld1() : nullptr;
			const float scale = RE::bhkWorld::GetWorldScale();

			if (!world || scale <= 0.0f) {
				logs::warn("Throw::GetWorldGravity: sin mundo de Havok accesible, se usa el valor de respaldo {:.3f}.", Constants::kThrowFallbackWorldGravity);
				return Constants::kThrowFallbackWorldGravity;
			}

			alignas(16) float components[4];
			_mm_store_ps(components, world->gravity.quad);
			const float gravity = components[2] / scale;

			if (!std::isfinite(gravity) || gravity >= 0.0f) {
				logs::warn("Throw::GetWorldGravity: valor leído no válido ({}), se usa el valor de respaldo {:.3f}.", gravity, Constants::kThrowFallbackWorldGravity);
				return Constants::kThrowFallbackWorldGravity;
			}

			static bool logged = false;
			if (!logged) {
				logs::info("Throw::GetWorldGravity: gravedad del mundo = {:.3f} u/s² ({:.4f} en unidades de Havok, escala {:.6f}).", gravity, components[2], scale);
				logged = true;
			}
			return gravity;
		}

		// Gravedad constante desde el instante cero (posición(t) = origen +
		// velocidad0·t + ½·gravedad·t²), igual que una flecha vanilla --
		// sin rampa de arranque (Mejora Kratos #1, retirada 2026-08-05, ver
		// CHANGELOG.md).
		float ComputeGravityDrop(float a_elapsed, float a_gravity)
		{
			return 0.5f * a_gravity * a_elapsed * a_elapsed;
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
		const float        gravity = GetWorldGravity(a_shooter) * Constants::kThrowGravityMult;

		// Sonido del silbido de lanzamiento: disparado aquí mismo, síncrono,
		// en vez de dentro del callback de Physics::SpawnReplica más abajo
		// (bug reportado por el usuario, 2026-08-08: sonaba demasiado
		// tarde) -- solo necesita origin, ya conocido en este punto, no
		// hace falta esperar a que el 3D de la réplica cargue (~unos pocos
		// Constants::kTickInterval de retraso real, ver Physics::SpawnReplica)
		// para reproducirlo.
		Audio::PlayFileOneShot(origin, Constants::kThrowLaunchSoundFilePath, Constants::kSoundHandleVolume);

		// Punto de partida real del giro (ver CLAUDE.md, "Arquitectura de
		// física de proyectiles"): la rotación mundial que tenía la malla
		// del arma equipada un instante antes de convertirse en réplica --
		// captura síncrona, antes de que Physics::SpawnReplica arranque la
		// espera asíncrona por el 3D de la réplica, así que sigue siendo
		// la última pose real visible aunque WeaponManager::ThrowWeapon ya
		// la haya ocultado (SetEquippedWeaponHidden no toca la
		// transformación, solo la visibilidad).
		const RE::NiMatrix3 capturedWeaponWorldRotation = Animation::GetEquippedWeaponWorldRotation(*a_shooter);

		Physics::SpawnReplica(a_shooter, a_weapon, origin, [a_shooter, a_weapon, origin, velocity0, gravity, capturedWeaponWorldRotation, callbacks = a_callbacks](RE::ObjectRefHandle a_handle) {
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
			auto token = Physics::StartTickLoop(a_handle, [a_shooter, a_handle, origin, velocity0, gravity, launchBaseLocal, trail, onStuck = callbacks.onStuck, onAutoRecall = callbacks.onAutoRecall, onTickStarted = callbacks.onTickStarted, elapsed = 0.0f, loggedFirstGravitySample = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				// Punto 10: se calcula y escribe el giro a mano cada tick
				// (ver Animation::TickSpin), compuesto permanentemente
				// sobre launchBaseLocal -- la pose real que tenía el arma
				// al salir de la mano marca su rotación durante todo el
				// vuelo, no solo los primeros instantes.
				Animation::TickSpin(a_refr, elapsed, launchBaseLocal);

				const float gravityDrop = ComputeGravityDrop(elapsed, gravity);

				// Log de verificación campo a campo, solo el primer tick
				// (no en cada uno, para no inundar el log).
				if (!loggedFirstGravitySample) {
					logs::info("Throw::LaunchWeapon: gravedad {:.2f} u/s², ComputeGravityDrop primer tick t={:.3f}s -> drop={:.2f}", gravity, elapsed, gravityDrop);
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
					// este punto (SetPosition+SyncHavok justo arriba), así
					// que la posición se calcula ya (solo lectura, sin
					// riesgo) -- pero el propio Animation::SpawnImpactVFX
					// (PlaceObjectAtMe) se difiere un tick en vez de
					// llamarse aquí mismo.
					//
					// CRASH real confirmado por el usuario (2026-08-28):
					// llamar a Animation::SpawnImpactVFX síncronamente
					// desde aquí (dentro del propio callback de
					// Physics::StartTickLoop, ya en marcha dentro de una
					// tarea de SKSE::GetTaskInterface()->AddTask) colgaba
					// el juego unos segundos y acababa crasheando --mismo
					// tipo de problema ya documentado en CLAUDE.md
					// ("nunca reencolar AddTask desde dentro de la propia
					// tarea que se ejecuta"), aquí con PlaceObjectAtMe en
					// vez de un AddTask explícito, pero la misma clase de
					// reentrada dentro del bucle de tareas del motor.
					// Mismo patrón hilo-que-duerme-y-reencola de siempre
					// para salir de ese contexto antes de tocar el motor
					// otra vez -- se usa a_shooter (el jugador, estable)
					// en vez de a_refr/a_handle para no depender de que la
					// réplica siga viva cuando despierte el hilo.
					const auto impactVfxPosition = a_refr.Get3D() ? Animation::GetGlowAnchorPosition(a_refr.Get3D()) : stickPoint;
					// (void): disparo suelto, nada cancela esto desde fuera.
					(void)Scheduler::After(Constants::kTickInterval, [a_shooter, impactVfxPosition]() {
						if (a_shooter) {
							Animation::SpawnImpactVFX(*a_shooter, impactVfxPosition);
						}
					});

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
