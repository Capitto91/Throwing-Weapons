// Destello con luz -- ver WeaponGlow.h.

#include "8.- ANIMATION/WeaponGlow.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

namespace Animation
{
	namespace
	{
		// Fase del fundido de encendido/apagado (escala del destello y fade de la luz).
		enum class GlowPhase
		{
			kFadingIn,
			kSteady,
			kFadingOut
		};

		// Un destello: clon de ThorMjolnirLight.nif colgado de la mano o de la réplica, su luz y su estado. Lo mantiene
		// vivo su bucle por fotograma (TickGlow), el único que lo cuelga, lo mueve y lo suelta de la escena.
		struct Glow
		{
			RE::NiPointer<RE::NiNode> root;
			RE::NiPointer<RE::NiNode> parent;       // nodo del que cuelga root
			RE::NiPoint3              anchorLocal;  // posición en el espacio de parent; se conserva si "Gold" desaparece

			// A quién sigue: la mano de actor o, con followReplica, la réplica.
			RE::ActorHandle     actor;
			RE::ObjectRefHandle replica;
			bool                followReplica{ false };

			// Luz: NiPointLight hijo de root y su registro en el ShadowSceneNode.
			RE::NiPointer<RE::NiPointLight> niLight;
			RE::NiPointer<RE::BSLight>      bsLight;
			float                           lightTargetFade{ 0.0f };

			// Malla con scroll de "V Offset" (bajo el NiBillboardNode) y "RingGlow" (pulso y giro).
			RE::NiPointer<RE::BSEffectShaderProperty> scrollShader;
			RE::NiPointer<RE::BSEffectShaderProperty> ringShader;
			RE::NiPointer<RE::NiAVObject>             ringNode;
			RE::NiMatrix3                             ringBaseLocalRotation;
			float                                     pulseElapsed{ 0.0f };
			float                                     ringRotationElapsed{ 0.0f };

			GlowPhase phase{ GlowPhase::kFadingIn };
			float     phaseElapsed{ 0.0f };
			bool      closeNow{ false };  // cierre sin fundido: el siguiente tick lo suelta
		};

		// Destello activo; uno que se apaga sigue vivo en su bucle hasta terminar.
		std::shared_ptr<Glow> g_active;

		// Avanza el fundido de a_glow y devuelve su factor (0 apagado, 1 pleno).
		float AdvanceFade(Glow& a_glow, float a_deltaSeconds)
		{
			if (a_glow.phase == GlowPhase::kSteady) {
				return 1.0f;
			}

			a_glow.phaseElapsed += a_deltaSeconds;
			const float t = std::clamp(a_glow.phaseElapsed / Constants::kGlowFadeDurationSeconds, 0.0f, 1.0f);
			if (a_glow.phase == GlowPhase::kFadingOut) {
				return 1.0f - t;
			}

			if (t >= 1.0f) {
				a_glow.phase = GlowPhase::kSteady;
			}
			return t;
		}

		// Posición mundial del destello a partir del nodo "Gold" (cabeza del martillo) y su offset.
		RE::NiPoint3 AnchorFromGold(const RE::NiAVObject& a_gold)
		{
			return a_gold.world.translate + a_gold.world.rotate * Constants::kGlowAnchorLocalOffset;
		}

		// Busca la geometría del primer NiBillboardNode hijo de a_root (por estructura, sin nombre).
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

		// Clon propio de ThorMjolnirLight.nif, cargado con BSModelDB desde el modelo del Activator del destello.
		RE::NiPointer<RE::NiNode> LoadGlowModel()
		{
			auto*       form = Forms::weaponGlowActivator;
			const char* modelPath = form ? form->GetModel() : nullptr;
			if (!modelPath || !*modelPath) {
				logs::warn("Animation::StartWeaponGlow: el Activator del destello no tiene modelo.");
				return {};
			}

			RE::NiPointer<RE::NiNode>                   loaded;
			constexpr RE::BSModelDB::DBTraits::ArgsType args{};
			if (const auto error = RE::BSModelDB::Demand(modelPath, loaded, args); error != RE::BSResource::ErrorCode::kNone || !loaded) {
				logs::warn("Animation::StartWeaponGlow: no se pudo cargar \"{}\" (código {}).", modelPath, std::to_underlying(error));
				return {};
			}

			const RE::NiPointer<RE::NiObject> clone(loaded->Clone());
			auto*                             cloneNode = clone ? netimmerse_cast<RE::NiNode*>(clone.get()) : nullptr;
			if (!cloneNode) {
				logs::warn("Animation::StartWeaponGlow: el clon de \"{}\" no es un NiNode.", modelPath);
				return {};
			}
			return RE::NiPointer<RE::NiNode>(cloneNode);
		}

		// Shaders y nodo que se animan cada tick, resueltos sobre el clon. A la malla con scroll se le quitan los
		// controladores horneados: el scroll lo escribe TickGlowUVScroll.
		void ResolveShaders(Glow& a_glow)
		{
			if (auto* geometry = FindGlowScrollGeometry(a_glow.root.get())) {
				a_glow.scrollShader = RE::NiPointer<RE::BSEffectShaderProperty>(
					skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
				if (!a_glow.scrollShader) {
					logs::warn("Animation::WeaponGlow: geometría del destello sin BSEffectShaderProperty -- sin scroll de UV.");
				}
			} else {
				logs::warn("Animation::WeaponGlow: no se encontró la geometría del NiBillboardNode -- sin scroll de UV.");
			}

			if (a_glow.scrollShader) {
				std::vector<RE::NiTimeController*> controllers;
				for (auto* controller = a_glow.scrollShader->GetControllers(); controller; controller = controller->GetNext()) {
					controllers.push_back(controller);
				}
				for (auto* controller : controllers) {
					a_glow.scrollShader->RemoveController(controller);
				}
			}

			if (auto* ringGlow = a_glow.root->GetObjectByName(Constants::kGlowRingGlowNodeName)) {
				a_glow.ringNode = RE::NiPointer<RE::NiAVObject>(ringGlow);
				a_glow.ringBaseLocalRotation = ringGlow->local.rotate;
				if (auto* geometry = ringGlow->AsGeometry()) {
					a_glow.ringShader = RE::NiPointer<RE::BSEffectShaderProperty>(
						skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
				}
			}
			if (!a_glow.ringNode) {
				logs::warn("Animation::WeaponGlow: no se encontró \"{}\" -- sin pulso de energía ni rotación.", Constants::kGlowRingGlowNodeName);
			} else if (!a_glow.ringShader) {
				logs::warn("Animation::WeaponGlow: \"{}\" sin BSEffectShaderProperty -- sin pulso de energía (la rotación sí aplica).", Constants::kGlowRingGlowNodeName);
			}
		}

		// Luz del formulario como NiPointLight hijo del clon, apagada; RegisterGlowLight la da de alta en la escena.
		void CreateGlowLight(Glow& a_glow)
		{
			auto* lightForm = Forms::weaponGlowLight;
			if (!lightForm) {
				return;
			}

			auto* niLight = RE::NiPointLight::Create();
			if (!niLight) {
				logs::warn("Animation::WeaponGlow: NiPointLight::Create devolvió nullptr -- sin luz real.");
				return;
			}
			a_glow.niLight = RE::NiPointer<RE::NiPointLight>(niLight);
			a_glow.root->AttachChild(niLight, false);

			auto&       data = niLight->GetLightRuntimeData();
			RE::NiColor color(lightForm->data.color);
			const float radius = static_cast<float>(lightForm->data.radius);
			data.ambient = RE::NiColor();
			data.diffuse = lightForm->data.flags.any(RE::TES_LIGHT_FLAGS::kNegative) ? -color : color;
			data.radius = RE::NiPoint3(radius, radius, radius);
			data.fade = 0.0f;
			niLight->SetLightAttenuation(radius);

			// Fade pleno tomado del formulario.
			a_glow.lightTargetFade = lightForm->fade;
		}

		// Da de alta la luz de a_glow en el ShadowSceneNode como luz dinámica sin sombra. Lo llama TickGlow una vez.
		void RegisterGlowLight(Glow& a_glow)
		{
			auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
			auto* lightForm = Forms::weaponGlowLight;
			if (!a_glow.niLight || a_glow.bsLight || !shadowSceneNode || !lightForm) {
				return;
			}

			RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
			params.dynamic = true;
			params.shadowLight = false;
			params.portalStrict = lightForm->data.flags.any(RE::TES_LIGHT_FLAGS::kPortalStrict);
			params.affectLand = true;
			params.affectWater = true;
			params.neverFades = true;
			params.fov = Constants::kGlowLightFov;
			params.falloff = Constants::kGlowLightFalloff;
			params.nearDistance = Constants::kGlowLightNearDistance;
			params.depthBias = 0.0f;
			params.sceneGraphIndex = 0;
			params.restrictedNode = nullptr;
			params.lensFlareData = lightForm->lensFlare;
			a_glow.bsLight = RE::NiPointer<RE::BSLight>(shadowSceneNode->AddLight(a_glow.niLight.get(), params));
		}

		// Suelta el clon de su padre actual.
		void DetachGlow(Glow& a_glow)
		{
			if (auto* current = a_glow.root->parent) {
				current->DetachChild(a_glow.root.get());
			}
			a_glow.parent.reset();
		}

		// Cuelga el clon de a_parent; la posición guardada se reinicia (es de otro espacio local).
		void AttachGlow(Glow& a_glow, RE::NiNode& a_parent)
		{
			DetachGlow(a_glow);
			a_parent.AttachChild(a_glow.root.get(), false);
			a_glow.parent = RE::NiPointer<RE::NiNode>(&a_parent);
			a_glow.anchorLocal = RE::NiPoint3{};
			logs::info("Animation::WeaponGlow: destello colgado de \"{}\".", a_parent.name.c_str());
		}

		// Da de baja la luz y suelta el clon de la escena.
		void ReleaseGlow(Glow& a_glow)
		{
			if (a_glow.bsLight) {
				if (auto* shadowSceneNode = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0]) {
					shadowSceneNode->RemoveLight(a_glow.bsLight);
				}
				a_glow.bsLight.reset();
			}
			DetachGlow(a_glow);
		}

		// Muestra u oculta el clon; oculto, la luz se apaga.
		void SetGlowVisible(Glow& a_glow, bool a_visible)
		{
			a_glow.root->GetFlags().set(!a_visible, RE::NiAVObject::Flag::kHidden);
			if (!a_visible && a_glow.niLight) {
				a_glow.niLight->GetLightRuntimeData().fade = 0.0f;
			}
		}

		// Nodo del que cuelga el destello y raíz bajo la que se busca "Gold".
		struct AttachPoint
		{
			RE::NiNode*     parent{ nullptr };
			RE::NiAVObject* anchorRoot{ nullptr };
		};

		// Réplica: el padre de "Gold", que gira con ella. Mano: el hueso "WEAPON", que sigue ahí con el arma oculta o ya
		// desequipada. Vacío si no hay 3D.
		AttachPoint ResolveAttachPoint(Glow& a_glow)
		{
			if (a_glow.followReplica) {
				auto  replicaRef = a_glow.replica.get();
				auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
				if (!replicaRoot) {
					return {};
				}
				auto* gold = replicaRoot->GetObjectByName(Constants::kWeaponHammerHeadNodeName);
				return { gold && gold->parent ? gold->parent : replicaRoot->AsNode(), replicaRoot };
			}

			auto  actor = a_glow.actor.get();
			auto* bone = actor ? ActorUtils::GetWeaponBone(*actor) : nullptr;
			return { bone ? bone->AsNode() : nullptr, bone };
		}

		// Diagnóstico, una vez por sesión: si el motor pide su cola de tareas para tocar la escena desde este bucle.
		void LogTaskQueueOnce()
		{
			static bool logged = false;
			if (!logged) {
				logged = true;
				logs::info("Animation::WeaponGlow: ShouldUseTaskQueue en el bucle por fotograma = {}.", RE::TaskQueueInterface::ShouldUseTaskQueue());
			}
		}

		// Avanza texCoordOffset[0] cada tick (Constants::kGlowUVScrollSpeed).
		void TickGlowUVScroll(Glow& a_glow, float a_deltaSeconds)
		{
			if (!a_glow.scrollShader) {
				return;
			}

			if (auto* material = a_glow.scrollShader->GetMaterial()) {
				material->texCoordOffset[0].y += Constants::kGlowUVScrollSpeed * a_deltaSeconds;
			}
		}

		// Pulso de baseColorScale de "RingGlow" con una onda seno.
		void TickGlowPulse(Glow& a_glow, float a_deltaSeconds)
		{
			if (!a_glow.ringShader) {
				return;
			}

			a_glow.pulseElapsed += a_deltaSeconds;

			constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
			const float     sine01 = 0.5f * (1.0f + std::sin(twoPi * Constants::kGlowPulseFrequencyHz * a_glow.pulseElapsed));
			const float     scale = Constants::kGlowPulseScaleMin + (Constants::kGlowPulseScaleMax - Constants::kGlowPulseScaleMin) * sine01;

			if (auto* material = a_glow.ringShader->GetMaterial()) {
				material->baseColorScale = scale;
			}
		}

		// Giro continuo de "RingGlow" sobre su eje.
		void TickGlowRingRotation(Glow& a_glow, float a_deltaSeconds)
		{
			if (!a_glow.ringNode) {
				return;
			}

			a_glow.ringRotationElapsed += a_deltaSeconds;

			RE::NiMatrix3 spin;
			spin.MakeRotation(Constants::kGlowRingRotationSpeed * a_glow.ringRotationElapsed, Constants::kGlowRingRotationAxisLocal);
			a_glow.ringNode->local.rotate = a_glow.ringBaseLocalRotation * spin;
		}

		// Un tick de a_glow: fundido, nodo del que cuelga, transformación local, luz y shaders. false al terminar (cierre o
		// fin del fundido de salida), ya fuera de la escena. Lo llama el bucle que arranca StartWeaponGlow.
		bool TickGlow(Glow& a_glow, float a_deltaSeconds)
		{
			const float fade = AdvanceFade(a_glow, a_deltaSeconds);
			if (a_glow.closeNow || (a_glow.phase == GlowPhase::kFadingOut && fade <= 0.0f)) {
				ReleaseGlow(a_glow);
				if (g_active.get() == &a_glow) {
					g_active.reset();
				}
				return false;
			}

			LogTaskQueueOnce();

			const auto point = ResolveAttachPoint(a_glow);
			if (!point.parent) {
				SetGlowVisible(a_glow, false);
				return true;
			}

			if (point.parent != a_glow.parent.get()) {
				AttachGlow(a_glow, *point.parent);
			}
			RegisterGlowLight(a_glow);

			// Cabeza del martillo en espacio mundial (sin rotación, con la escala del fundido) pasada al espacio del padre:
			// el motor la lleva con él al propagar su transformación. Sin "Gold", la última posición relativa.
			RE::NiTransform world;
			world.scale = fade;
			auto* gold = point.anchorRoot->GetObjectByName(Constants::kWeaponHammerHeadNodeName);
			world.translate = gold ? AnchorFromGold(*gold) : point.parent->world.translate;

			auto local = Math::LocalTransformFromWorld(*a_glow.root, world);
			if (gold) {
				a_glow.anchorLocal = local.translate;
			} else {
				local.translate = a_glow.anchorLocal;
			}
			a_glow.root->local = local;

			SetGlowVisible(a_glow, true);
			if (a_glow.niLight) {
				a_glow.niLight->GetLightRuntimeData().fade = a_glow.lightTargetFade * fade;
			}

			TickGlowUVScroll(a_glow, a_deltaSeconds);
			TickGlowPulse(a_glow, a_deltaSeconds);
			TickGlowRingRotation(a_glow, a_deltaSeconds);

			RE::NiUpdateData updateData{};
			a_glow.root->Update(updateData);
			return true;
		}
	}

	RE::NiPoint3 GetGlowAnchorPosition(RE::NiAVObject* a_root)
	{
		if (!a_root) {
			return RE::NiPoint3{};
		}

		if (auto* goldNode = a_root->GetObjectByName(Constants::kWeaponHammerHeadNodeName)) {
			return AnchorFromGold(*goldNode);
		}

		return a_root->world.translate;
	}

	bool IsWeaponGlowNode(const RE::NiAVObject* a_object)
	{
		return a_object && std::string_view(a_object->name.c_str()) == Constants::kWeaponGlowRootNodeName;
	}

	bool StartWeaponGlow(RE::Actor& a_actor, bool a_checkSetting)
	{
		// Desactivable con [VFX] WeaponLight, salvo a_checkSetting=false (power attacks).
		if (a_checkSetting && !Settings::GetWeaponLight()) {
			return false;
		}

		if (g_active) {
			if (g_active->phase != GlowPhase::kFadingOut) {
				return false;
			}

			// El anterior aún se estaba apagando: su bucle lo suelta ya.
			g_active->closeNow = true;
			g_active.reset();
		}

		auto glow = std::make_shared<Glow>();
		glow->root = LoadGlowModel();
		if (!glow->root) {
			return false;
		}
		glow->root->name = RE::BSFixedString(Constants::kWeaponGlowRootNodeName);
		glow->actor = a_actor.GetHandle();
		ResolveShaders(*glow);
		CreateGlowLight(*glow);

		// Desde el fotograma siguiente: el primer tick lo cuelga de la mano a escala 0.
		(void)Physics::StartTickLoop(a_actor.GetHandle(), [glow](RE::TESObjectREFR&, float a_deltaSeconds) {
			return TickGlow(*glow, a_deltaSeconds);
		});

		g_active = std::move(glow);
		return true;
	}

	void RetargetWeaponGlowToReplica(RE::ObjectRefHandle a_handle)
	{
		if (!g_active) {
			return;
		}

		auto replica = a_handle.get();
		if (!replica || !replica->Get3D()) {
			logs::warn("Animation::RetargetWeaponGlowToReplica: réplica sin 3D todavía.");
			return;
		}

		// El siguiente tick lo cuelga de la réplica.
		g_active->replica = a_handle;
		g_active->followReplica = true;
	}

	void RetargetWeaponGlowToActor(RE::Actor& a_actor)
	{
		if (!g_active) {
			return;
		}

		if (!ActorUtils::GetWeaponBone(a_actor)) {
			logs::warn("Animation::RetargetWeaponGlowToActor: hueso \"{}\" no encontrado.", Constants::kWeaponNodeName);
			return;
		}

		// El siguiente tick lo cuelga de la mano.
		g_active->actor = a_actor.GetHandle();
		g_active->replica = {};
		g_active->followReplica = false;
	}

	void StopWeaponGlow()
	{
		if (!g_active || g_active->phase == GlowPhase::kFadingOut) {
			// Sin destello o ya apagándose.
			return;
		}

		// Fundido de salida; al terminar, su bucle lo suelta.
		g_active->phase = GlowPhase::kFadingOut;
		g_active->phaseElapsed = 0.0f;
	}

	void StopWeaponGlowNow()
	{
		if (g_active) {
			g_active->closeNow = true;
			g_active.reset();
		}
	}

	void RemoveStrayWeaponGlows()
	{
		auto* tes = RE::TES::GetSingleton();
		auto* form = Forms::weaponGlowActivator;
		if (!tes || !form) {
			return;
		}

		// Se apuntan durante el recorrido de las celdas y se borran después, fuera de él.
		std::vector<RE::ObjectRefHandle> strays;
		tes->ForEachReference([&](RE::TESObjectREFR* a_ref) {
			if (a_ref && !a_ref->IsDeleted() && a_ref->GetBaseObject() == form) {
				strays.emplace_back(a_ref);
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});

		for (const auto& handle : strays) {
			Physics::DestroyReference(handle);
		}

		if (!strays.empty()) {
			logs::info("Animation::RemoveStrayWeaponGlows: retirados {} destellos que quedaron guardados en la partida.", strays.size());
		}
	}
}
