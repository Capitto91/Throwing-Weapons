// Diagnóstico temporal de primera persona -- ver FirstPersonDiag.h.

#include "11.- SKYRIM/FirstPersonDiag.h"

#include "1.- CORE/Constants.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>

namespace Diag
{
	namespace
	{
		// -- Traza --
		// Fin de la traza en ms de steady_clock (lo lee el sink desde los hilos de animación).
		std::atomic<std::int64_t> g_traceUntilMs{ 0 };

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		bool TraceActive()
		{
			return NowMs() < g_traceUntilMs.load();
		}

		// -- Caja negra --
		// Últimas entradas (ms de steady_clock, texto); se escriben desde los hilos de animación y el principal.
		struct RecorderEntry
		{
			std::int64_t ms;
			std::string  text;
		};
		constexpr std::size_t     kRecorderSize = 60;
		std::mutex                g_recorderMutex;
		std::deque<RecorderEntry> g_recorder;

		// Estado del enganche en el fotograma anterior (solo hilo principal).
		bool g_weaponWasAttached = false;

		// Fuentes de evento ya enganchadas y la cámara con la que se engancharon (solo para etiquetar).
		std::mutex                                             g_sourcesMutex;
		std::vector<std::pair<const void*, const char*>>       g_sources;

		const char* SourceLabel(const void* a_source)
		{
			std::lock_guard lock(g_sourcesMutex);
			for (const auto& [source, label] : g_sources) {
				if (source == a_source) {
					return label;
				}
			}
			return "?";
		}

		class TraceSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				Record(std::format("evento '{}' payload='{}'", a_event->tag.c_str(), a_event->payload.c_str()));
				if (TraceActive()) {
					logs::info("[traza] evento '{}' payload='{}' grafo {}", a_event->tag.c_str(), a_event->payload.c_str(), SourceLabel(a_source));
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		TraceSink g_traceSink;

		// Engancha el sink a todos los grafos del gestor de animación activo (cambia con la cámara).
		void RegisterOnCurrentGraphs(RE::PlayerCharacter& a_player)
		{
			RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
			if (!a_player.GetAnimationGraphManager(manager) || !manager) {
				return;
			}

			const char* label = ActorUtils::IsPlayerInFirstPerson() ? "1a" : "3a";
			for (auto& graph : manager->graphs) {
				auto* source = graph ? graph->GetEventSource<RE::BSAnimationGraphEvent>() : nullptr;
				if (!source) {
					continue;
				}
				{
					std::lock_guard lock(g_sourcesMutex);
					bool known = false;
					for (const auto& entry : g_sources) {
						known = known || entry.first == source;
					}
					if (known) {
						continue;
					}
					g_sources.emplace_back(source, label);
				}
				source->AddEventSink(&g_traceSink);
			}
		}

		const char* WeaponStateName(RE::WEAPON_STATE a_state)
		{
			switch (a_state) {
			case RE::WEAPON_STATE::kSheathed:
				return "envainada";
			case RE::WEAPON_STATE::kWantToDraw:
				return "quiere desenvainar";
			case RE::WEAPON_STATE::kDrawing:
				return "desenvainando";
			case RE::WEAPON_STATE::kDrawn:
				return "desenvainada";
			case RE::WEAPON_STATE::kWantToSheathe:
				return "quiere envainar";
			case RE::WEAPON_STATE::kSheathing:
				return "envainando";
			default:
				return "?";
			}
		}

		int WeaponNodeChildren(RE::PlayerCharacter& a_player, bool a_firstPerson)
		{
			auto* root = a_player.Get3D(a_firstPerson);
			auto* bone = root ? root->GetObjectByName("WEAPON") : nullptr;
			auto* node = bone ? bone->AsNode() : nullptr;
			return node ? static_cast<int>(node->GetChildren().size()) : -1;
		}

		std::string g_lastSnapshot;
		bool        g_traceLoopRunning = false;

		// Estado del jugador en una línea; se registra solo si cambia respecto al anterior.
		void LogSnapshotIfChanged(RE::PlayerCharacter& a_player)
		{
			auto*         actorState = a_player.AsActorState();
			auto*         right = a_player.GetEquippedObject(false);
			std::int32_t  rightHandType = -1;
			bool          skipEquip = false;
			(void)a_player.GetGraphVariableInt(Constants::kRightHandTypeGraphVariable, rightHandType);
			(void)a_player.GetGraphVariableBool("SkipEquipAnimation", skipEquip);

			auto snapshot = std::format("cámara {} | ataque={} arma {} | mano der.='{}' | WEAPON 3a={} 1a={} | iRightHandType={} SkipEquipAnimation={}",
				ActorUtils::IsPlayerInFirstPerson() ? "1a" : "3a",
				actorState ? static_cast<int>(actorState->GetAttackState()) : -1,
				actorState ? WeaponStateName(actorState->GetWeaponState()) : "?",
				right ? right->GetName() : "-", WeaponNodeChildren(a_player, false), WeaponNodeChildren(a_player, true), rightHandType, skipEquip);
			if (snapshot != g_lastSnapshot) {
				logs::info("[traza] estado | {}", snapshot);
				g_lastSnapshot = std::move(snapshot);
			}
		}
	}

	void StartTrace(std::string_view a_reason, float a_seconds)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		const auto until = NowMs() + static_cast<std::int64_t>(a_seconds * 1000.0f);
		if (until > g_traceUntilMs.load()) {
			g_traceUntilMs.store(until);
		}
		logs::info("[traza] inicio: {} ({:.1f} s)", a_reason, a_seconds);

		RegisterOnCurrentGraphs(*player);
		g_lastSnapshot.clear();
		LogSnapshotIfChanged(*player);

		if (g_traceLoopRunning) {
			return;
		}
		g_traceLoopRunning = true;

		// Cada fotograma: engancha los grafos de la cámara activa y registra el estado si cambia.
		(void)Physics::StartTickLoop(player->GetHandle(), [](RE::TESObjectREFR&, float) {
			auto* actor = RE::PlayerCharacter::GetSingleton();
			if (!actor || !TraceActive()) {
				g_traceLoopRunning = false;
				logs::info("[traza] fin");
				return false;
			}
			RegisterOnCurrentGraphs(*actor);
			LogSnapshotIfChanged(*actor);
			return true;
		});
	}

	void NoteSent(std::string_view a_what, std::string_view a_who)
	{
		Record(std::format("enviado '{}' ({})", a_what, a_who));
		if (TraceActive()) {
			logs::info("[traza] enviado '{}' ({})", a_what, a_who);
		}
	}

	void Record(std::string a_text)
	{
		const auto      now = NowMs();
		std::lock_guard lock(g_recorderMutex);

		// Los dos grafos del jugador emiten lo mismo: se omite el duplicado inmediato.
		if (!g_recorder.empty() && g_recorder.back().text == a_text && now - g_recorder.back().ms < 20) {
			return;
		}
		g_recorder.push_back({ now, std::move(a_text) });
		if (g_recorder.size() > kRecorderSize) {
			g_recorder.pop_front();
		}
	}

	void PollWeaponAttach()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// La caja negra necesita los eventos siempre, no solo durante la traza.
		RegisterOnCurrentGraphs(*player);

		// Sin 3D (carga, cambio de celda) no se juzga nada.
		const int third = WeaponNodeChildren(*player, false);
		const int first = WeaponNodeChildren(*player, true);
		if (third < 0 || first < 0) {
			return;
		}

		const bool attached = third > 0 || first > 0;
		if (attached == g_weaponWasAttached) {
			return;
		}
		g_weaponWasAttached = attached;

		auto*      right = player->GetEquippedObject(false);
		auto*      actorState = player->AsActorState();
		const bool drawn = actorState && actorState->IsWeaponDrawn();
		if (attached) {
			Record(std::format("arma enganchada a la mano (WEAPON 3a={} 1a={})", third, first));
			return;
		}
		if (!right || !drawn) {
			Record(std::format("arma fuera de la mano: {}", !right ? "desequipada" : "envainando o envainada"));
			return;
		}

		std::int32_t rightHandType = -1;
		(void)player->GetGraphVariableInt(Constants::kRightHandTypeGraphVariable, rightHandType);
		logs::warn("[caja negra] ARMA SOLTADA DE LA MANO estando equipada ('{}') y desenvainada | cámara {} | arma {} | iRightHandType={}",
			right->GetName(), ActorUtils::IsPlayerInFirstPerson() ? "1a" : "3a", actorState ? WeaponStateName(actorState->GetWeaponState()) : "?", rightHandType);

		const auto now = NowMs();
		std::lock_guard lock(g_recorderMutex);
		for (const auto& entry : g_recorder) {
			logs::info("  [caja negra] hace {:>5} ms: {}", now - entry.ms, entry.text);
		}
	}

	namespace
	{
		// Estado de cámara en el último PollCamera (-1: ninguno todavía).
		int g_lastCameraState = -1;

		int CurrentCameraState()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			return camera && camera->currentState ? static_cast<int>(camera->currentState->id) : -1;
		}

		const char* CameraStateName(int a_state)
		{
			switch (a_state) {
			case static_cast<int>(RE::CameraStates::kFirstPerson):
				return "1a persona";
			case static_cast<int>(RE::CameraStates::kThirdPerson):
				return "3a persona";
			default:
				return "otra";
			}
		}

		std::string Pos(const RE::NiPoint3& a_p)
		{
			return std::format("({:.0f}, {:.0f}, {:.0f})", a_p.x, a_p.y, a_p.z);
		}

		// Hueso "WEAPON" y mano derecha de un esqueleto: hijos, arma colgada, oculta, posición y distancia a la cámara.
		void DumpSkeleton(const char* a_label, RE::NiAVObject* a_root, RE::NiAVObject* a_byName, const RE::NiPoint3& a_cameraPos)
		{
			if (!a_root) {
				logs::info("  [diag] {}: sin 3D", a_label);
				return;
			}

			auto* weapon = a_root->GetObjectByName("WEAPON");
			auto* hand = a_root->GetObjectByName("NPC R Hand [RHnd]");
			if (!weapon) {
				logs::info("  [diag] {}: raíz '{}' sin hueso WEAPON", a_label, a_root->name.c_str());
				return;
			}

			std::size_t children = 0;
			std::string firstChild = "-";
			bool        hidden = false;
			bool        gold = false;
			if (auto* node = weapon->AsNode()) {
				children = node->GetChildren().size();
				for (auto& child : node->GetChildren()) {
					if (child) {
						firstChild = child->name.c_str();
						hidden = child->GetFlags().any(RE::NiAVObject::Flag::kHidden);
						break;
					}
				}
				gold = node->GetObjectByName("Gold") != nullptr;
			}

			logs::info("  [diag] {}: raíz '{}' | WEAPON hijos={} primero='{}' oculto={} Gold={} pos={} dist.cámara={:.0f} | mano der. pos={} | es el de GetNodeByName={}",
				a_label, a_root->name.c_str(), children, firstChild, hidden, gold, Pos(weapon->world.translate),
				weapon->world.translate.GetDistance(a_cameraPos), hand ? Pos(hand->world.translate) : std::string("-"), weapon == a_byName);
		}
	}

	void DumpHands(std::string_view a_reason)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		auto*        camera = RE::PlayerCamera::GetSingleton();
		RE::NiPoint3 cameraPos = camera && camera->cameraRoot ? camera->cameraRoot->world.translate : RE::NiPoint3{};

		auto* current = player->Get3D();
		auto* third = player->Get3D(false);
		auto* first = player->Get3D(true);
		auto* infoFirst = player->GetInfoRuntimeData().firstPerson3D.get();
		auto* byName = player->GetNodeByName("WEAPON");

		const int state = CurrentCameraState();
		logs::info("[diag] {} | cámara {} ({}) pos={} | Get3D() es {} | Get3D(true)==firstPerson3D: {} | Get3D(true)==Get3D(false): {}",
			a_reason, CameraStateName(state), state, Pos(cameraPos),
			current == first ? "el de 1a" : (current == third ? "el de 3a" : "otro"),
			static_cast<RE::NiAVObject*>(infoFirst) == first, first == third);
		DumpSkeleton("esqueleto 3a", third, byName, cameraPos);
		DumpSkeleton("esqueleto 1a", first, byName, cameraPos);
	}

	void PollCamera()
	{
		const int state = CurrentCameraState();
		if (state == g_lastCameraState) {
			return;
		}

		const bool firstCall = g_lastCameraState == -1;
		g_lastCameraState = state;
		Record(std::format("cambio de cámara a {} ({})", CameraStateName(state), state));
		if (!firstCall) {
			DumpHands(std::format("cambio de cámara a {}", CameraStateName(state)));
		}
	}

	void DumpAim(RE::Actor* a_shooter, const RE::NiPoint3& a_origin, const RE::NiPoint3& a_cameraPos, const RE::NiPoint3& a_forward, const RE::NiPoint3& a_aimPoint, bool a_hitUsed)
	{
		constexpr float kRadToDeg = 57.2957795f;
		constexpr float kRayLength = 6000.0f;

		auto*      camera = RE::PlayerCamera::GetSingleton();
		const int  state = CurrentCameraState();
		const auto pitchOf = [kRadToDeg](const RE::NiPoint3& a_dir) { return std::asin(std::clamp(-a_dir.z, -1.0f, 1.0f)) * kRadToDeg; };

		std::string firstPersonInfo = "-";
		if (camera && camera->currentState && state == static_cast<int>(RE::CameraStates::kFirstPerson)) {
			auto* fps = static_cast<RE::FirstPersonState*>(camera->currentState.get());
			if (fps && fps->firstPersonCameraObj) {
				const auto fwd = fps->firstPersonCameraObj->world.rotate.GetVectorY();
				firstPersonInfo = std::format("pos={} inclinación={:.1f}°", Pos(fps->firstPersonCameraObj->world.translate), pitchOf(fwd));
			}
		}

		// Primer impacto del rayo, sin descartar nada (lo que veía el Raycast antes de saltar impactos).
		std::string firstHit = "nada";
		if (auto* tes = RE::TES::GetSingleton()) {
			const float     scale = RE::bhkWorld::GetWorldScale();
			RE::bhkPickData pick;
			pick.rayInput.from = RE::hkVector4(a_cameraPos * scale);
			pick.rayInput.to = RE::hkVector4((a_cameraPos + a_forward * kRayLength) * scale);
			pick.rayInput.filterInfo.SetCollisionLayer(RE::COL_LAYER::kProjectile);
			tes->Pick(pick);
			if (pick.rayOutput.HasHit() && pick.rayOutput.rootCollidable) {
				auto*       ref = RE::TESHavokUtilities::FindCollidableRef(*pick.rayOutput.rootCollidable);
				const auto  layer = pick.rayOutput.rootCollidable->broadPhaseHandle.collisionFilterInfo.GetCollisionLayer();
				const char* name = ref ? ref->GetName() : nullptr;
				firstHit = std::format("a {:.0f} u, capa {}, ref '{}'{}", kRayLength * pick.rayOutput.hitFraction, static_cast<int>(layer),
					name ? name : "-", ref == a_shooter ? " (EL JUGADOR)" : "");
			}
		}

		RE::NiPoint3 toAim = a_aimPoint - a_origin;
		const float  toAimLength = toAim.Length();
		if (toAimLength > 0.0f) {
			toAim /= toAimLength;
		}
		const float deviation = std::acos(std::clamp(toAim.Dot(a_forward), -1.0f, 1.0f)) * kRadToDeg;

		const float rootPitch = camera && camera->cameraRoot ? pitchOf(camera->cameraRoot->world.rotate.GetVectorY()) : 0.0f;
		logs::info("[diag] apuntado | cámara {} pos={} inclinación usada={:.1f}° cameraRoot={:.1f}° | jugador inclinación={:.1f}° | 1P cameraObj {} | mano={} (a {:.0f} u de la cámara)",
			CameraStateName(state), Pos(a_cameraPos), pitchOf(a_forward), rootPitch, a_shooter ? a_shooter->GetAngleX() * kRadToDeg : 0.0f, firstPersonInfo,
			Pos(a_origin), a_origin.GetDistance(a_cameraPos));
		logs::info("  [diag] primer impacto sin filtrar: {} | punto de mira usado: {} a {:.0f} u de la cámara ({}) | desvío mano->mira respecto a la cámara={:.1f}°",
			firstHit, Pos(a_aimPoint), a_aimPoint.GetDistance(a_cameraPos), a_hitUsed ? "impacto" : "final del rayo", deviation);
	}

	void DumpDashEffects(RE::BGSArtObject* a_art, RE::TESEffectShader* a_shader, float a_secondsSinceStart)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* processLists = RE::ProcessLists::GetSingleton();
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!player || !processLists) {
			return;
		}

		const auto         handle = player->CreateRefHandle();
		const RE::NiPoint3 cameraPos = camera && camera->cameraRoot ? camera->cameraRoot->world.translate : RE::NiPoint3{};
		std::vector<std::string> lines;

		processLists->ForEachMagicTempEffect([&](RE::BSTempEffect* a_tempEffect) {
			auto* referenceEffect = a_tempEffect->As<RE::ReferenceEffect>();
			if (!referenceEffect || referenceEffect->target != handle) {
				return RE::BSContainer::ForEachResult::kContinue;
			}

			auto*       modelEffect = a_tempEffect->As<RE::ModelReferenceEffect>();
			const auto* shaderEffect = a_tempEffect->As<RE::ShaderReferenceEffect>();
			const bool  isArt = a_art && modelEffect && modelEffect->artObject == a_art;
			const bool  isShader = a_shader && shaderEffect && shaderEffect->effectData == a_shader;
			if (!isArt && !isShader) {
				return RE::BSContainer::ForEachResult::kContinue;
			}

			std::string detail;
			if (isArt) {
				auto*         art3D = modelEffect->artObject3D.get();
				std::uint32_t particles = 0;
				int           emitters = 0;
				if (art3D) {
					RE::BSVisit::TraverseScenegraphGeometries(art3D, [&](RE::BSGeometry* a_geometry) {
						if (auto* particleGeom = a_geometry->AsParticlesGeom()) {
							++emitters;
							if (const auto& data = particleGeom->GetParticlesRuntimeData().particleData) {
								particles += data->GetActiveVertexCount();
							}
						}
						return RE::BSVisit::BSVisitControl::kContinue;
					});
				}
				detail = std::format("arte | 3D={} padre='{}' 3a persona={} enganchado={} | sistemas={} partículas vivas={} | dist.cámara={:.0f}",
					art3D != nullptr, art3D && art3D->parent ? art3D->parent->name.c_str() : "-",
					modelEffect->flags.any(RE::ModelReferenceEffect::Flags::kThirdPerson), modelEffect->flags.any(RE::ModelReferenceEffect::Flags::kAttached),
					emitters, particles, art3D ? art3D->world.translate.GetDistance(cameraPos) : -1.0f);
			} else {
				detail = std::format("shader | 3a persona={} suspendido={}", shaderEffect->flags.any(RE::ShaderReferenceEffect::Flag::kThirdPerson),
					shaderEffect->flags.any(RE::ShaderReferenceEffect::Flag::kSuspended));
			}

			// Controlador de efecto mágico (ActiveEffect) frente a efecto suelto (ApplyEffectShader/ApplyArtObject).
			const auto* controller = referenceEffect->controller;
			const bool  fromMagicEffect = controller && *reinterpret_cast<const std::uintptr_t*>(controller) == RE::VTABLE_ActiveEffectReferenceEffectController[0].address();
			lines.push_back(std::format("    [{:p}] {} | edad={:.2f} vida={:.2f} terminado={} | controlador={}", static_cast<const void*>(a_tempEffect), detail,
				a_tempEffect->age, a_tempEffect->lifetime, referenceEffect->finished, fromMagicEffect ? "efecto mágico" : (controller ? "otro" : "ninguno")));
			return RE::BSContainer::ForEachResult::kContinue;
		});

		logs::info("[diag] dash +{:.2f} s | cámara {} | jugador z={:.0f} en el aire={} | efectos del dash: {}", a_secondsSinceStart,
			CameraStateName(CurrentCameraState()), player->GetPosition().z, player->IsInMidair(), lines.size());
		for (const auto& line : lines) {
			logs::info("{}", line);
		}
	}
}
