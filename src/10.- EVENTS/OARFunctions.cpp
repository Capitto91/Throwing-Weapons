// Funciones de OAR -- ver OARFunctions.h.

#include "10.- EVENTS/OARFunctions.h"

#include "13.- EXTERNAL/OpenAnimationReplacer/OpenAnimationReplacerAPI-Functions.h"
#include "3.- WEAPON/LightningDash.h"
#include "3.- WEAPON/WeaponManager.h"
#include "11.- SKYRIM/FirstPersonDiag.h"

namespace Events::OARFunctions
{
	namespace
	{
		// Mínimo que pide OAR (nombre, descripción, versión, RunImpl). FirstPerson: variante con sufijo 1P que llaman los
		// submods de _1stperson. OAR llama a RunImpl desde un hilo de animación: el aviso se encola al hilo principal.
		template <bool FirstPerson>
		class ThrowReleaseFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = FirstPerson ? "ThorMjolnirThrowRelease1P"sv : "ThorMjolnirThrowRelease"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara WeaponManager::OnThrowReleaseAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] {
					Diag::DumpHands(FirstPerson ? "OAR Throw 1P" : "OAR Throw");
					Weapon::WeaponManager::GetSingleton()->OnThrowReleaseAnimationEvent();
				});
				return true;
			}
		};

		template <bool FirstPerson>
		class CallReleaseFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = FirstPerson ? "ThorMjolnirCallRelease1P"sv : "ThorMjolnirCallRelease"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara WeaponManager::OnCallReleaseAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] {
					Diag::DumpHands(FirstPerson ? "OAR Call 1P" : "OAR Call");
					Weapon::WeaponManager::GetSingleton()->OnCallReleaseAnimationEvent();
				});
				return true;
			}
		};

		template <bool FirstPerson>
		class CatchReleaseFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = FirstPerson ? "ThorMjolnirCatchRelease1P"sv : "ThorMjolnirCatchRelease"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara WeaponManager::OnCatchReleaseAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] {
					Diag::DumpHands(FirstPerson ? "OAR Catch 1P" : "OAR Catch");
					Weapon::WeaponManager::GetSingleton()->OnCatchReleaseAnimationEvent(true);
				});
				return true;
			}
		};

		template <bool FirstPerson>
		class SlamImpactFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = FirstPerson ? "ThorMjolnirSlamImpact1P"sv : "ThorMjolnirSlamImpact"sv;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return "Dispara LightningDash::OnSlamImpactAnimationEvent (ThorMjolnir)."sv.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] {
					Diag::DumpHands(FirstPerson ? "OAR Slam 1P" : "OAR Slam");
					Weapon::LightningDash::OnSlamImpactAnimationEvent(true);
				});
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

		Register<ThrowReleaseFunction<false>>();
		Register<ThrowReleaseFunction<true>>();
		Register<CallReleaseFunction<false>>();
		Register<CallReleaseFunction<true>>();
		Register<CatchReleaseFunction<false>>();
		Register<CatchReleaseFunction<true>>();
		Register<SlamImpactFunction<false>>();
		Register<SlamImpactFunction<true>>();
	}
}
