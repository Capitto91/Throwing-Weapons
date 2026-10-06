// Funciones de OAR -- ver OARFunctions.h.

#include "10.- EVENTS/OARFunctions.h"

#include "13.- EXTERNAL/OpenAnimationReplacer/OpenAnimationReplacerAPI-Functions.h"
#include "3.- WEAPON/LightningDash.h"
#include "3.- WEAPON/WeaponManager.h"

namespace Events::OARFunctions
{
	namespace
	{
		// Anotación de cada clip: nombres que usan los config.json (tercera / primera persona), descripción para el
		// editor de OAR y aviso que se ejecuta en el hilo principal.
		struct ThrowRelease
		{
			static constexpr std::string_view kName = "ThorMjolnirThrowRelease"sv;
			static constexpr std::string_view kName1P = "ThorMjolnirThrowRelease1P"sv;
			static constexpr std::string_view kDescription = "Dispara WeaponManager::OnThrowReleaseAnimationEvent (ThorMjolnir)."sv;
			static void                       Run() { Weapon::WeaponManager::GetSingleton()->OnThrowReleaseAnimationEvent(); }
		};

		struct CallRelease
		{
			static constexpr std::string_view kName = "ThorMjolnirCallRelease"sv;
			static constexpr std::string_view kName1P = "ThorMjolnirCallRelease1P"sv;
			static constexpr std::string_view kDescription = "Dispara WeaponManager::OnCallReleaseAnimationEvent (ThorMjolnir)."sv;
			static void                       Run() { Weapon::WeaponManager::GetSingleton()->OnCallReleaseAnimationEvent(); }
		};

		struct CatchRelease
		{
			static constexpr std::string_view kName = "ThorMjolnirCatchRelease"sv;
			static constexpr std::string_view kName1P = "ThorMjolnirCatchRelease1P"sv;
			static constexpr std::string_view kDescription = "Dispara WeaponManager::OnCatchReleaseAnimationEvent (ThorMjolnir)."sv;
			static void                       Run() { Weapon::WeaponManager::GetSingleton()->OnCatchReleaseAnimationEvent(true); }
		};

		struct SlamImpact
		{
			static constexpr std::string_view kName = "ThorMjolnirSlamImpact"sv;
			static constexpr std::string_view kName1P = "ThorMjolnirSlamImpact1P"sv;
			static constexpr std::string_view kDescription = "Dispara LightningDash::OnSlamImpactAnimationEvent (ThorMjolnir)."sv;
			static void                       Run() { Weapon::LightningDash::OnSlamImpactAnimationEvent(true); }
		};

		// Mínimo que pide OAR (nombre, descripción, versión, RunImpl) para la anotación Gesture; FirstPerson elige el
		// nombre con sufijo 1P. OAR llama a RunImpl desde un hilo de animación: el aviso se encola al hilo principal.
		template <class Gesture, bool FirstPerson>
		class AnnotationFunction final : public Functions::CustomFunction
		{
		public:
			constexpr static inline std::string_view FUNCTION_NAME = FirstPerson ? Gesture::kName1P : Gesture::kName;

			RE::BSString GetName() const override { return FUNCTION_NAME.data(); }
			RE::BSString GetDescription() const override { return Gesture::kDescription.data(); }
			REL::Version GetRequiredVersion() const override { return { 1, 0, 0 }; }

		protected:
			bool RunImpl(RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, Functions::Trigger*) const override
			{
				SKSE::GetTaskInterface()->AddTask([] { Gesture::Run(); });
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

		// Registra las dos variantes de Gesture: tercera persona y primera (sufijo 1P).
		template <class Gesture>
		void RegisterBothViews()
		{
			Register<AnnotationFunction<Gesture, false>>();
			Register<AnnotationFunction<Gesture, true>>();
		}
	}

	void RegisterAll()
	{
		OAR_API::Functions::GetAPI(OAR_API::Functions::InterfaceVersion::Latest);
		if (!g_oarFunctionsInterface) {
			logs::warn("Events::OARFunctions::RegisterAll: no se pudo obtener la API de Functions de Open Animation Replacer -- ¿está instalado/actualizado? El ciclo seguirá funcionando vía la red de seguridad por tiempo de cada Begin*Animation, sin sincronía fina.");
			return;
		}

		RegisterBothViews<ThrowRelease>();
		RegisterBothViews<CallRelease>();
		RegisterBothViews<CatchRelease>();
		RegisterBothViews<SlamImpact>();
	}
}
