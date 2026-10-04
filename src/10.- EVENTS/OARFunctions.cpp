// Funciones de OAR -- ver OARFunctions.h.

#include "10.- EVENTS/OARFunctions.h"

#include "13.- EXTERNAL/OpenAnimationReplacer/OpenAnimationReplacerAPI-Functions.h"
#include "3.- WEAPON/LightningDash.h"
#include "3.- WEAPON/WeaponManager.h"

namespace Events::OARFunctions
{
	namespace
	{
		// Mínimo que pide OAR: nombre, descripción, versión requerida y RunImpl.
		// OAR llama a RunImpl desde un hilo de animación: el aviso a WeaponManager se encola al hilo principal.
		class ThrowReleaseFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = "ThorMjolnirThrowRelease"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara WeaponManager::OnThrowReleaseAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] { Weapon::WeaponManager::GetSingleton()->OnThrowReleaseAnimationEvent(); });
				return true;
			}
		};

		class CallReleaseFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = "ThorMjolnirCallRelease"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara WeaponManager::OnCallReleaseAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] { Weapon::WeaponManager::GetSingleton()->OnCallReleaseAnimationEvent(); });
				return true;
			}
		};

		class CatchReleaseFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = "ThorMjolnirCatchRelease"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara WeaponManager::OnCatchReleaseAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] { Weapon::WeaponManager::GetSingleton()->OnCatchReleaseAnimationEvent(true); });
				return true;
			}
		};

		class SlamImpactFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = "ThorMjolnirSlamImpact"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara LightningDash::OnSlamImpactAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] { Weapon::LightningDash::OnSlamImpactAnimationEvent(true); });
				return true;
			}
		};

		template <typename T>
		void Register()
		{
			switch (OAR_API::Functions::AddCustomFunction<T>()) {
				using enum OAR_API::Functions::APIResult;
			case OK:
				logs::info("Events::OARFunctions: '{}' registrada.", T::FUNCTION_NAME);
				break;
			case AlreadyRegistered:
				logs::warn("Events::OARFunctions: '{}' ya estaba registrada.", T::FUNCTION_NAME);
				break;
			case Invalid:
				logs::error("Events::OARFunctions: '{}' inválida, no registrada.", T::FUNCTION_NAME);
				break;
			case Failed:
				logs::error("Events::OARFunctions: fallo al registrar '{}'.", T::FUNCTION_NAME);
				break;
			}
		}
	}

	void RegisterAll()
	{
		OAR_API::Functions::GetAPI(OAR_API::Functions::InterfaceVersion::Latest);
		if (!g_oarFunctionsInterface) {
			logs::warn("Events::OARFunctions::RegisterAll: no se pudo obtener la API de Functions de Open Animation Replacer -- ¿está instalado/actualizado? El ciclo seguirá funcionando vía la red de seguridad por tiempo de cada Begin*Animation, sin sincronía fina.");
			return;
		}

		Register<ThrowReleaseFunction>();
		Register<CallReleaseFunction>();
		Register<CatchReleaseFunction>();
		Register<SlamImpactFunction>();
	}
}
