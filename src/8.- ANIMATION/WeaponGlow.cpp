// Implementación del destello. Ver el header para la arquitectura
// completa y el porqué de un Activator + PlaceObjectAtMe en vez de
// BSTempEffectParticle (como WeaponTrail).

#include "8.- ANIMATION/WeaponGlow.h"

#include "1.- CORE/Constants.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <cmath>
#include <numbers>
#include <thread>

namespace Animation
{
	namespace
	{
		// Mismo margen que Physics::SpawnReplica/Animation::WeaponVFX --
		// ~800ms de sobra para lo que suele tardar el 3D de una referencia
		// recién colocada en cargar en segundo plano.
		constexpr int kMax3DWaitAttempts = 50;

		// Formulario resuelto una sola vez por sesión -- mismo patrón que
		// GetOnActivatorForm/GetOffActivatorForm en WeaponVFX.cpp.
		RE::TESBoundObject* GetGlowActivatorForm()
		{
			static RE::TESBoundObject* cache = nullptr;
			static bool                lookupDone = false;
			if (!lookupDone) {
				lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					cache = dataHandler->LookupForm<RE::TESObjectACTI>(Constants::kWeaponGlowActivatorLocalFormID, Constants::kSoundPluginName);
				}
				if (!cache) {
					logs::warn("Animation::WeaponGlow: no se encontró el Activator (FormID local 0x{:03X}) en \"{}\".",
						Constants::kWeaponGlowActivatorLocalFormID, Constants::kSoundPluginName);
				}
			}
			return cache;
		}

		// Único destello activo en todo el plugin -- coherente con que
		// solo hay un ciclo de arma a la vez. A diferencia de
		// Animation::WeaponVFX, no hace falta ningún token de
		// "activación de secuencia" (StartActivatingSequence): este .nif
		// es una malla estática con shader de brillo, no un sistema de
		// partículas con NiControllerManager que activar.
		RE::ObjectRefHandle g_activeHandle;
		Physics::TickToken  g_tickToken;

		// Luz real. SEGUNDO INTENTO (2026-08-27) -- ver CHANGELOG.md para
		// el primero (RE::NiPointLight::Create() + RE::NiNode::AttachChild
		// + RE::ShadowSceneNode::AddLight a mano, técnica de
		// powerof3/LightPlacer): descartado tras confirmar en el juego 5
		// crashes en 3 pruebas (0 antes de añadir la luz), siempre la
		// MISMA dirección exacta de excepción incluso después de intentar
		// arreglar el AttachChild directo pasándolo por
		// RE::TaskQueueInterface -- indica que el problema real no era
		// (solo) el AttachChild, sino algo más de fondo en reimplementar a
		// mano la creación/registro de la luz en vez de dejar que el
		// propio motor lo haga.
		//
		// Este intento usa RE::TESObjectLIGH::GenDynamic en su lugar --
		// función nativa real del motor (RELOCATION_ID(17208, 17610), ver
		// TESObjectLIGH.h), la vía que Bethesda usa internamente para
		// generar una luz dinámica a partir de un formulario Light y
		// engancharla a un nodo, en vez de reconstruir a mano con
		// NiPointLight::Create() + ShadowSceneNode::AddLight. Semántica
		// exacta de los tres parámetros char sin confirmar (código de
		// ingeniería inversa sin comentarios) -- se usan valores
		// razonables (forceDynamic=1, useLightRadius=1, affectRefOnly=0)
		// a falta de mejor fuente.
		RE::NiPointer<RE::NiLight> g_niLight;

		// Valor "pleno" de fade de la luz (capturado del formulario en
		// AttachGlowLight) -- 0.0 hasta que haya una luz real adjunta.
		float g_lightTargetFade = 0.0f;

		// Fundido de encendido/apagado (Constants::kGlowFadeDuration, a
		// petición del usuario 2026-08-27) -- afecta tanto a la escala del
		// nodo raíz del destello (malla visible creciendo/encogiendo)
		// como al "fade" (dimmer) de la luz real, en paralelo. kSteady =
		// ya a fuerza plena, TickGlowFade no hace nada (evita recalcular
		// la misma escala/fade cada tick sin necesidad). g_phaseElapsed se
		// reinicia cada vez que StartTicking/StopWeaponGlow cambian de
		// fase.
		enum class GlowPhase
		{
			kFadingIn,
			kSteady,
			kFadingOut
		};
		GlowPhase g_phase = GlowPhase::kFadingIn;
		float     g_phaseElapsed = 0.0f;

		// Aplica el fundido en curso (si lo hay) sobre a_refr (el propio
		// destello) -- llamada desde los tres bucles de tick
		// (StartTicking/RetargetWeaponGlowToReplica/ToActor), igual que
		// TickGlowUVScroll. No toca nada si ya está en fase kSteady.
		void TickGlowFade(RE::TESObjectREFR& a_refr, float a_deltaSeconds)
		{
			if (g_phase == GlowPhase::kSteady) {
				return;
			}

			g_phaseElapsed += a_deltaSeconds;
			float t = g_phaseElapsed / Constants::kGlowFadeDurationSeconds;
			t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

			if (g_phase == GlowPhase::kFadingOut) {
				t = 1.0f - t;
			} else if (t >= 1.0f) {
				g_phase = GlowPhase::kSteady;
			}

			if (auto* node3D = a_refr.Get3D()) {
				node3D->local.scale = t;
				node3D->world.scale = t;
			}
			if (g_niLight) {
				g_niLight->GetLightRuntimeData().fade = g_lightTargetFade * t;
			}
		}

		// Formulario resuelto una sola vez por sesión -- mismo patrón que
		// GetGlowActivatorForm.
		RE::TESObjectLIGH* GetGlowLightForm()
		{
			static RE::TESObjectLIGH* cache = nullptr;
			static bool               lookupDone = false;
			if (!lookupDone) {
				lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					cache = dataHandler->LookupForm<RE::TESObjectLIGH>(Constants::kWeaponGlowLightLocalFormID, Constants::kSoundPluginName);
				}
				if (!cache) {
					logs::warn("Animation::WeaponGlow: no se encontró el TESObjectLIGH (FormID local 0x{:03X}) en \"{}\".",
						Constants::kWeaponGlowLightLocalFormID, Constants::kSoundPluginName);
				}
			}
			return cache;
		}

		// Genera y adjunta la luz real sobre a_root (el propio nodo raíz
		// del Activator del destello, misma referencia que a_ref) -- ver
		// el comentario de g_niLight para el porqué de GenDynamic en vez
		// de construir el NiLight a mano.
		void AttachGlowLight(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root)
		{
			auto* lightForm = GetGlowLightForm();
			auto* rootNode = a_root ? a_root->AsNode() : nullptr;
			if (!lightForm || !rootNode || !a_ref) {
				return;
			}

			auto* niLight = lightForm->GenDynamic(a_ref, rootNode, 1, 1, 0);
			if (!niLight) {
				logs::warn("Animation::WeaponGlow: TESObjectLIGH::GenDynamic devolvió nullptr -- sin luz real.");
				return;
			}

			g_niLight = RE::NiPointer<RE::NiLight>(niLight);

			// Valor "pleno" hacia el que rampea TickGlowFade -- capturado
			// aquí (el propio formulario, no el runtime data del NiLight
			// recién creado) porque GenDynamic puede dejarlo ya puesto a
			// este mismo valor de fábrica; guardarlo aparte evita
			// depender de en qué momento exacto lo escribe GenDynamic por
			// dentro.
			g_lightTargetFade = lightForm->fade;
		}

		// Suelta nuestra propia referencia a la luz -- sin ningún
		// desenganche manual del árbol de escena (a diferencia del primer
		// intento): se confía en que destruir la propia referencia del
		// destello (Physics::DestroyReplica, ver StopWeaponGlow) limpia
		// también la luz que GenDynamic le enganchó, igual que le pasa a
		// cualquier luz dinámica nativa del juego atada a una referencia
		// que se deshabilita/borra. Sin confirmar todavía si hace falta
		// algo más -- si la luz "sobrevive" visualmente a la destrucción
		// del destello, seguir investigando desde aquí.
		void DetachGlowLight()
		{
			g_niLight.reset();
		}

		// Shader del único BSTriShape que lleva la animación de "V Offset"
		// horneada en el NIF (bajo el NiBillboardNode) -- resuelto una vez
		// en StartTicking, en cuanto el 3D está listo. El controller
		// horneado (BSEffectShaderPropertyFloatController) nunca llega a
		// reproducirse solo en el juego real (confirmado: el motor no
		// llama NiTimeController::Update() por su cuenta sobre un
		// controller de una referencia colocada en tiempo de ejecución --
		// mismo motivo exacto ya documentado en CLAUDE.md para el giro,
		// ahí con un NiTransformController en vez de éste), así que
		// TickGlowUVScroll escribe el offset directamente cada tick, en
		// vez de depender de él.
		RE::NiPointer<RE::BSEffectShaderProperty> g_shaderProperty;

		// Busca la única geometría colgada del (primer) NiBillboardNode
		// hijo de a_root -- la pieza concreta que lleva la animación de UV
		// en este NIF (las otras dos BSTriShape del fichero, fuera del
		// billboard, no tienen ningún controller que sustituir). Recorrido
		// manual en vez de GetObjectByName -- este NIF no tiene un nombre
		// de nodo estable que buscar (exportado con pyNifly, nombres tipo
		// "3Auwc6F" sin significado), así que se localiza por estructura.
		RE::BSGeometry* FindGlowScrollGeometry(RE::NiAVObject* a_root)
		{
			auto* rootNode = a_root ? a_root->AsNode() : nullptr;
			if (!rootNode) {
				return nullptr;
			}

			for (auto& child : rootNode->GetChildren()) {
				if (!child) {
					continue;
				}
				if (auto* billboard = netimmerse_cast<RE::NiBillboardNode*>(child.get())) {
					for (auto& grandchild : billboard->GetChildren()) {
						if (grandchild) {
							if (auto* geometry = grandchild->AsGeometry()) {
								return geometry;
							}
						}
					}
				}
			}

			return nullptr;
		}

		// Avanza el scroll de "V Offset" a mano -- ver el comentario de
		// g_shaderProperty. BSShaderMaterial::texCoordOffset[2] es el
		// mismo campo que el controller horneado escribiría cada frame si
		// el motor lo ejecutara -- aquí se escribe directamente, igual que
		// Animation::TickSpin escribe NiAVObject::local.rotate en vez de
		// depender de un NiTransformController. Solo el índice [0]
		// (confirmado por decodificación real del propio NIF, ver
		// Constants::kGlowUVScrollSpeed) -- el [1] no lo anima ningún
		// controller en este fichero.
		void TickGlowUVScroll(float a_deltaSeconds)
		{
			if (!g_shaderProperty) {
				return;
			}

			if (auto* material = g_shaderProperty->GetMaterial()) {
				material->texCoordOffset[0].y += Constants::kGlowUVScrollSpeed * a_deltaSeconds;
			}
		}

		// Shader del BSTriShape "RingGlow" -- resuelto una vez en
		// StartTicking (mismo momento que g_shaderProperty), por nombre
		// real esta vez (Constants::kGlowRingGlowNodeName), no por
		// estructura.
		RE::NiPointer<RE::BSEffectShaderProperty> g_ringGlowShaderProperty;
		float                                      g_pulseElapsed = 0.0f;

		// Pulso de energía sobre "RingGlow" -- ver
		// Constants::kGlowPulseFrequencyHz/kGlowPulseScaleMin/Max. Oscila
		// BSEffectShaderMaterial::baseColorScale con una onda seno
		// reescalada a [min, max] -- mismo motivo que TickGlowUVScroll
		// para escribirlo a mano en vez de hornear un controller (no se
		// reproduciría solo, misma causa ya confirmada en este archivo).
		void TickGlowPulse(float a_deltaSeconds)
		{
			if (!g_ringGlowShaderProperty) {
				return;
			}

			g_pulseElapsed += a_deltaSeconds;

			constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
			const float     sine01 = 0.5f * (1.0f + std::sin(twoPi * Constants::kGlowPulseFrequencyHz * g_pulseElapsed));
			const float     scale = Constants::kGlowPulseScaleMin + (Constants::kGlowPulseScaleMax - Constants::kGlowPulseScaleMin) * sine01;

			if (auto* material = g_ringGlowShaderProperty->GetMaterial()) {
				material->baseColorScale = scale;
			}
		}

		// Guarda de reentrancia para los reintentos de espera de 3D (ver
		// WaitFor3DThenStartTicking) -- mismo patrón que
		// Animation::WeaponVFX::g_generation: si StopWeaponGlow corre
		// mientras un reintento sigue dormido, éste se descarta en
		// silencio en vez de arrancar un tick loop sobre un handle ya
		// destruido.
		std::atomic<std::uint64_t> g_generation{ 0 };

		// Reevaluado cada tick (no una foto fija) -- mismo criterio que
		// Animation::WeaponVFX::GetPlayerHandPosition, pero leyendo la
		// posición de "Gold" (ver GetGlowAnchorPosition) en vez de la del
		// propio hueso "WEAPON".
		RE::NiPoint3 GetPlayerHandGlowPosition()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* handNode = player ? player->GetNodeByName("WEAPON") : nullptr;
			return GetGlowAnchorPosition(handNode);
		}

		// Modo Havok "movido por código" + arranca el bucle de tick que
		// sigue a_getTargetPosition -- mismo patrón que
		// Animation::WeaponVFX::StartTicking, sin el aparato de
		// activación de secuencia (no aplica aquí, ver arriba).
		void StartTicking(RE::ObjectRefHandle a_handle, std::function<RE::NiPoint3()> a_getTargetPosition)
		{
			auto  ref = a_handle.get();
			auto* node3D = ref ? ref->Get3D() : nullptr;
			if (!node3D) {
				return;
			}

			node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);

			// Nace a escala 0 (invisible) -- TickGlowFade la sube a 1
			// durante Constants::kGlowFadeDuration, mismo criterio que
			// WeaponTrail::Start para evitar un fotograma a fuerza plena
			// antes de que el primer tick del fundido corra.
			g_phase = GlowPhase::kFadingIn;
			g_phaseElapsed = 0.0f;
			node3D->local.scale = 0.0f;
			node3D->world.scale = 0.0f;

			// Resuelto una sola vez, aquí, en cuanto el 3D está listo -- ver
			// el comentario de g_shaderProperty.
			if (auto* geometry = FindGlowScrollGeometry(node3D)) {
				g_shaderProperty = RE::NiPointer<RE::BSEffectShaderProperty>(
					skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
				if (!g_shaderProperty) {
					logs::warn("Animation::WeaponGlow: geometría del destello sin BSEffectShaderProperty -- sin scroll de UV.");
				}
			} else {
				logs::warn("Animation::WeaponGlow: no se encontró la geometría del NiBillboardNode -- sin scroll de UV.");
			}

			// Shader de "RingGlow" -- resuelto por nombre real (ver
			// Constants::kGlowRingGlowNodeName), para el pulso de energía
			// (TickGlowPulse).
			g_pulseElapsed = 0.0f;
			if (auto* ringGlow = node3D->GetObjectByName(Constants::kGlowRingGlowNodeName)) {
				if (auto* geometry = ringGlow->AsGeometry()) {
					g_ringGlowShaderProperty = RE::NiPointer<RE::BSEffectShaderProperty>(
						skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
				}
			}
			if (!g_ringGlowShaderProperty) {
				logs::warn("Animation::WeaponGlow: no se encontró \"{}\" -- sin pulso de energía.", Constants::kGlowRingGlowNodeName);
			}

			// Luz real -- adjuntada al propio nodo raíz del Activator, se
			// mueve gratis con él cada tick (ver AttachGlowLight/GenDynamic).
			AttachGlowLight(ref.get(), node3D);

			g_tickToken = Physics::StartTickLoop(a_handle, [getPos = std::move(a_getTargetPosition)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) {
				const auto pos = getPos();
				a_refr.SetPosition(pos);
				Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				TickGlowUVScroll(a_deltaSeconds);
				TickGlowPulse(a_deltaSeconds);
				TickGlowFade(a_refr, a_deltaSeconds);
				return true;
			});

			logs::info("Animation::WeaponGlow: siguiendo por tick, posición inicial ({:.1f},{:.1f},{:.1f}).",
				node3D->world.translate.x, node3D->world.translate.y, node3D->world.translate.z);
		}

		// Sondeo con el mismo patrón hilo-que-duerme-y-reencola del resto
		// del proyecto hasta que el 3D de la referencia recién colocada
		// esté listo -- ver Animation::WeaponVFX::WaitFor3DThenStartTicking.
		void WaitFor3DThenStartTicking(RE::ObjectRefHandle a_handle, std::function<RE::NiPoint3()> a_getTargetPosition, int a_attemptsLeft, std::uint64_t a_generation)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto ref = a_handle.get();
			if (!ref) {
				return;
			}

			if (ref->Get3D()) {
				StartTicking(a_handle, std::move(a_getTargetPosition));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponGlow: el 3D del destello nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_handle, getPos = std::move(a_getTargetPosition), a_attemptsLeft, a_generation]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_handle, getPos = std::move(getPos), a_attemptsLeft, a_generation]() mutable {
					WaitFor3DThenStartTicking(a_handle, std::move(getPos), a_attemptsLeft - 1, a_generation);
				});
			}).detach();
		}
	}

	// Posición mundial del nodo "Gold" (cabeza del martillo,
	// Constants::kWeaponHammerHeadNodeName) bajo a_root -- a_root puede ser
	// el hueso "WEAPON" del actor (mientras el arma sigue en la mano, ver
	// GetPlayerHandGlowPosition) o Get3D() de la réplica en vuelo/clavada
	// (ver RetargetWeaponGlowToReplica y Throw::LaunchWeapon vía
	// Animation::SpawnImpactVFX). Corregido 2026-08-27: el destello colgaba
	// de a_root directamente (base del mango en los dos casos), no de la
	// cabeza -- "Gold" es un nodo real y nombrado dentro del NIF del arma,
	// así que se busca y se lee su posición real cada vez en vez de aplicar
	// un offset fijo calculado a ojo (a diferencia de
	// Constants::kTrailAnchorLocalOffset, que sí necesita un offset porque
	// el nodo raíz de la réplica no corresponde a ningún punto nombrado del
	// NIF). Si no se encuentra (NIF sin el nodo, o todavía sin 3D), cae de
	// vuelta a la posición de a_root -- mejor que nada, con aviso en el
	// log. Expuesta en el header (ya no en el namespace anónimo) para que
	// otros VFX anclados a la cabeza del arma puedan reutilizarla.
	RE::NiPoint3 GetGlowAnchorPosition(RE::NiAVObject* a_root)
	{
		if (!a_root) {
			return RE::NiPoint3{};
		}

		if (auto* goldNode = a_root->GetObjectByName(Constants::kWeaponHammerHeadNodeName)) {
			return goldNode->world.translate + goldNode->world.rotate * Constants::kGlowAnchorLocalOffset;
		}

		logs::warn("Animation::WeaponGlow: nodo \"{}\" no encontrado -- usando la posición de a_root de reserva.",
			Constants::kWeaponHammerHeadNodeName);
		return a_root->world.translate;
	}

	void StartWeaponGlow(RE::Actor& a_actor)
	{
		if (g_activeHandle) {
			if (g_phase != GlowPhase::kFadingOut) {
				logs::warn("Animation::StartWeaponGlow: ya hay un destello activo -- no-op.");
				return;
			}

			// El anterior todavía está en su margen de fundido de salida
			// (ver StopWeaponGlow, Constants::kGlowFadeDuration) -- se
			// cierra de golpe aquí mismo en vez de esperar a que su
			// propio margen diferido lo haga, para que este ciclo nuevo
			// pueda arrancar sin conflicto. Invalida ese cierre diferido
			// (guardado por generación) antes de tocar nada.
			++g_generation;
			Physics::CancelTickLoop(g_tickToken);
			g_tickToken = {};
			g_shaderProperty.reset();
			g_ringGlowShaderProperty.reset();
			DetachGlowLight();
			Physics::DestroyReplica(g_activeHandle);
			g_activeHandle = {};
		}

		auto* form = GetGlowActivatorForm();
		if (!form) {
			return;
		}

		auto ref = a_actor.PlaceObjectAtMe(form, false);
		if (!ref) {
			logs::warn("Animation::StartWeaponGlow: PlaceObjectAtMe devolvió nullptr.");
			return;
		}

		// Igual que Physics::SpawnReplica/Animation::WeaponVFX: sin esto,
		// el jugador podría activarlo/recogerlo con la tecla de activar.
		ref->SetActivationBlocked(true);

		g_activeHandle = RE::ObjectRefHandle(ref.get());

		const auto generation = ++g_generation;
		WaitFor3DThenStartTicking(g_activeHandle, GetPlayerHandGlowPosition, kMax3DWaitAttempts, generation);
	}

	void RetargetWeaponGlowToReplica(RE::ObjectRefHandle a_handle)
	{
		if (!g_activeHandle) {
			return;
		}

		auto  replica = a_handle.get();
		auto* root = replica ? replica->Get3D() : nullptr;
		if (!replica || !root) {
			logs::warn("Animation::RetargetWeaponGlowToReplica: réplica sin 3D todavía.");
			return;
		}

		// Cancelar el bucle anterior antes de arrancar el nuevo es
		// obligatorio -- sin esto, los dos escribirían la posición cada
		// tick a la vez (ver Physics::TickToken).
		Physics::CancelTickLoop(g_tickToken);

		// "Auto-reparable", mismo mecanismo que
		// Animation::StartMovementVFXOnReplica: si la réplica deja de
		// existir mientras este retargeteo sigue activo, el destello se
		// congela en su última posición conocida en vez de saltar al
		// origen del mundo.
		g_tickToken = Physics::StartTickLoop(g_activeHandle, [handle = a_handle, lastPosition = GetGlowAnchorPosition(root)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			auto  replicaRef = handle.get();
			auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
			if (replicaRoot) {
				lastPosition = GetGlowAnchorPosition(replicaRoot);
			}
			a_refr.SetPosition(lastPosition);
			Physics::SyncHavok(a_refr, lastPosition, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
			TickGlowUVScroll(a_deltaSeconds);
			TickGlowPulse(a_deltaSeconds);
			TickGlowFade(a_refr, a_deltaSeconds);
			return true;
		});
	}

	void RetargetWeaponGlowToActor(RE::Actor& a_actor)
	{
		if (!g_activeHandle) {
			return;
		}

		if (!a_actor.GetNodeByName("WEAPON")) {
			logs::warn("Animation::RetargetWeaponGlowToActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = Physics::StartTickLoop(g_activeHandle, [](RE::TESObjectREFR& a_refr, float a_deltaSeconds) {
			const auto pos = GetPlayerHandGlowPosition();
			a_refr.SetPosition(pos);
			Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
			TickGlowUVScroll(a_deltaSeconds);
			TickGlowPulse(a_deltaSeconds);
			TickGlowFade(a_refr, a_deltaSeconds);
			return true;
		});
	}

	void StopWeaponGlow()
	{
		if (!g_activeHandle || g_phase == GlowPhase::kFadingOut) {
			// Sin destello activo, o ya apagándose -- no-op (evita
			// solapar dos fundidos de salida si algo llama de más).
			return;
		}

		// No se cancela el bucle de tick ni se destruye nada todavía --
		// se deja correr Constants::kGlowFadeDuration más, con
		// TickGlowFade encogiendo la malla y atenuando la luz en paralelo
		// (mismos tres bucles de tick de siempre, sin ningún cambio en
		// ellos), y solo entonces se limpia de verdad. Guardado por
		// generación, mismo patrón que Animation::WeaponVFX: si
		// StartWeaponGlow arranca un destello nuevo mientras este margen
		// todavía está corriendo, este cierre diferido se descarta en
		// silencio en vez de limpiar el destello equivocado.
		g_phase = GlowPhase::kFadingOut;
		g_phaseElapsed = 0.0f;

		const auto generation = ++g_generation;
		std::thread([generation]() {
			std::this_thread::sleep_for(Constants::kGlowFadeDuration);
			SKSE::GetTaskInterface()->AddTask([generation]() {
				if (g_generation.load() != generation) {
					return;
				}

				Physics::CancelTickLoop(g_tickToken);
				g_tickToken = {};

				g_shaderProperty.reset();
				g_ringGlowShaderProperty.reset();
				DetachGlowLight();

				if (g_activeHandle) {
					Physics::DestroyReplica(g_activeHandle);
					g_activeHandle = {};
				}
			});
		}).detach();
	}
}
