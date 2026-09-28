// Comprobación de requisitos -- ver Requirements.h.

#include "1.- CORE/Requirements.h"

#include "1.- CORE/Constants.h"

#include <filesystem>
#include <optional>

namespace Requirements
{
	namespace
	{
		const SKSE::LoadInterface* g_skse = nullptr;

		// Plugin SKSE que usa el mod: nombre con que se registra en SKSE (el de su DLL),
		// si es obligatorio, versión mínima (vacía = cualquiera) y versión con la que se probó.
		struct PluginRequirement
		{
			const char*                 skseName;
			const char*                 displayName;
			bool                        required;
			std::optional<REL::Version> minimum;
			REL::Version                tested;
			const char*                 purpose;
		};

		const PluginRequirement kPlugins[] = {
			{ "OpenAnimationReplacer", "Open Animation Replacer", true, REL::Version{ 3, 0, 0 }, REL::Version{ 3, 2, 0 },
				"animaciones de Lanzar, Llamada y Atrape" },
			{ "SkipEquipAnimation", "Skip Equip Animation", true, std::nullopt, REL::Version{ 1, 0, 9 },
				"reequipar el arma sin animación de desenvainar" },
			{ "TrueDirectionalMovement", "True Directional Movement", false, std::nullopt, REL::Version{ 2, 3, 1 },
				"apuntar al objetivo fijado (target lock)" },
			{ "SKSEMenuFramework", "SKSE Menu Framework", false, REL::Version{ 3, 0, 0 }, REL::Version{ 3, 13, 0 },
				"menú de configuración en el juego" },
		};

		// Versión mínima del juego para el rango de FormID extendido del .esp (ESL con cabecera 1.71).
		constexpr REL::Version kExtendedEslGameVersion{ 1, 6, 1130 };

		// Fichero de Address Library que corresponde a la versión del juego en ejecución.
		std::filesystem::path AddressLibraryPath(const REL::Version& a_game)
		{
			const auto name = REL::Module::IsVR() ? "version-" + a_game.string() + ".csv" :
			                  REL::Module::IsAE() ? "versionlib-" + a_game.string() + ".bin" :
			                                        "version-" + a_game.string() + ".bin";
			return std::filesystem::path("Data/SKSE/Plugins") / name;
		}
	}

	void Init(const SKSE::LoadInterface* a_skse)
	{
		g_skse = a_skse;

		const auto game = REL::Module::get().version();
		const auto runtime = REL::Module::IsVR() ? "VR" : (REL::Module::IsAE() ? "AE" : "SE");
		logs::info("==== Requisitos de ThorMjolnir ====");
		logs::info("[OK] Skyrim {} ({}).", game.string("."), runtime);
		if (REL::Module::IsVR()) {
			logs::warn("[AVISO] Skyrim VR: el daño usa un camino de respaldo, sin reacciones de golpe completas.");
		}

		if (a_skse) {
			logs::info("[OK] SKSE {}.", REL::Version::unpack(a_skse->SKSEVersion()).string("."));
		}

		std::error_code ec;
		const auto      addressLibrary = AddressLibraryPath(game);
		if (std::filesystem::exists(addressLibrary, ec)) {
			logs::info("[OK] Address Library ({}).", addressLibrary.filename().string());
		} else {
			logs::error("[FALTA] Address Library para Skyrim {}: no se encuentra {}. Instala \"Address Library for SKSE Plugins\".",
				game.string("."), addressLibrary.string());
		}
	}

	void CheckPlugins()
	{
		if (!g_skse) {
			return;
		}

		for (const auto& plugin : kPlugins) {
			const auto* info = g_skse->GetPluginInfo(plugin.skseName);
			if (!info) {
				if (plugin.required) {
					logs::error("[FALTA] {} (obligatorio: {}).", plugin.displayName, plugin.purpose);
				} else {
					logs::info("[--] {} no instalado (opcional: {}).", plugin.displayName, plugin.purpose);
				}
				continue;
			}

			const auto version = REL::Version::unpack(info->version);
			if (plugin.minimum && version < *plugin.minimum) {
				logs::error("[VERSIÓN] {} {} es demasiado antiguo: mínimo {} (probado con {}).",
					plugin.displayName, version.string("."), plugin.minimum->string("."), plugin.tested.string("."));
			} else if (version != plugin.tested) {
				logs::info("[OK] {} {} (probado con {}).", plugin.displayName, version.string("."), plugin.tested.string("."));
			} else {
				logs::info("[OK] {} {}.", plugin.displayName, version.string("."));
			}
		}
	}

	void CheckPluginFile()
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return;
		}

		const auto pluginName = Constants::kSoundPluginName;
		if (!dataHandler->LookupModByName(pluginName)) {
			logs::error("[FALTA] {} no está activo en el orden de carga.", pluginName);
			return;
		}

		// Un formulario propio conocido: si el .esp está pero no se resuelve, el juego no admite su rango de FormID.
		if (!dataHandler->LookupForm<RE::TESObjectACTI>(Constants::kWeaponGlowActivatorLocalFormID, pluginName)) {
			logs::error(
				"[ERROR] {} está activo pero sus formularios no se resuelven: hace falta Skyrim {} o posterior, "
				"o \"Backported Extended ESL Support\" en versiones anteriores.",
				pluginName, kExtendedEslGameVersion.string("."));
			return;
		}

		logs::info("[OK] {} cargado.", pluginName);
		logs::info("==================================");
	}
}
