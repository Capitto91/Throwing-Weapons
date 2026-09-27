// Implementación de la configuración editable en tiempo de ejecución.

#include "1.- CORE/Settings.h"

#include "1.- CORE/Constants.h"

#include <SimpleIni.h>

#include <mutex>

namespace Settings
{
	namespace
	{
		constexpr const char* kControlsSection = "Controls";
		constexpr const char* kThrowSection = "Throw";

		// Nombres de clave de [Controls] sin cambiar (AimDevice/AimKeyCode)
		// aunque ya no haya fase de apuntado: renombrarlas rompería los INI
		// que ya tengan los usuarios.
		constexpr const char* kDeviceKey = "AimDevice";
		constexpr const char* kKeyCodeKey = "AimKeyCode";
		constexpr const char* kSpeedKey = "Speed";
		constexpr const char* kGravityMultKey = "GravityMultiplier";

		struct Values
		{
			ActionBinding binding{ kDefaultActionBinding };
			float         throwSpeed{ kDefaultThrowSpeed };
			float         throwGravityMult{ kDefaultThrowGravityMult };
		};

		std::mutex g_mutex;
		Values     g_values;

		RE::INPUT_DEVICE ParseDevice(std::string_view a_value)
		{
			if (a_value == "Mouse") {
				return RE::INPUT_DEVICE::kMouse;
			}
			if (a_value == "Gamepad") {
				return RE::INPUT_DEVICE::kGamepad;
			}
			return RE::INPUT_DEVICE::kKeyboard;
		}

		// std::clamp evitado a propósito: Windows.h define min/max como
		// macros (mismo problema ya documentado en el proyecto, ver
		// Return::BeginReturn).
		float Clamp(float a_value, float a_min, float a_max)
		{
			return a_value < a_min ? a_min : (a_value > a_max ? a_max : a_value);
		}
	}

	const char* DeviceToString(RE::INPUT_DEVICE a_device)
	{
		switch (a_device) {
		case RE::INPUT_DEVICE::kMouse:
			return "Mouse";
		case RE::INPUT_DEVICE::kGamepad:
			return "Gamepad";
		default:
			return "Keyboard";
		}
	}

	void Load()
	{
		Values loaded;

		CSimpleIniA ini;
		ini.SetUnicode();

		if (ini.LoadFile(Constants::kInputConfigPath) < 0) {
			logs::warn("Settings::Load: no se encontró {}, se usan los valores por defecto.", Constants::kInputConfigPath);
		} else {
			loaded.binding.device = ParseDevice(ini.GetValue(kControlsSection, kDeviceKey, DeviceToString(kDefaultActionBinding.device)));
			loaded.binding.keyCode = static_cast<std::uint32_t>(ini.GetLongValue(kControlsSection, kKeyCodeKey, static_cast<long>(kDefaultActionBinding.keyCode)));
			loaded.throwSpeed = Clamp(static_cast<float>(ini.GetDoubleValue(kThrowSection, kSpeedKey, kDefaultThrowSpeed)), kThrowSpeedMin, kThrowSpeedMax);
			loaded.throwGravityMult = Clamp(static_cast<float>(ini.GetDoubleValue(kThrowSection, kGravityMultKey, kDefaultThrowGravityMult)), kThrowGravityMultMin, kThrowGravityMultMax);
		}

		{
			std::scoped_lock lock(g_mutex);
			g_values = loaded;
		}

		logs::info("Settings::Load: tecla {} {} | velocidad {:.0f} u/s | multiplicador de gravedad {:.2f}.",
			DeviceToString(loaded.binding.device), loaded.binding.keyCode, loaded.throwSpeed, loaded.throwGravityMult);
	}

	bool Save()
	{
		Values current;
		{
			std::scoped_lock lock(g_mutex);
			current = g_values;
		}

		// Se carga el archivo existente (si lo hay) para conservar todo lo
		// que no es nuestro: otras secciones, comentarios, orden.
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.LoadFile(Constants::kInputConfigPath);

		ini.SetValue(kControlsSection, kDeviceKey, DeviceToString(current.binding.device));
		ini.SetLongValue(kControlsSection, kKeyCodeKey, static_cast<long>(current.binding.keyCode));
		ini.SetDoubleValue(kThrowSection, kSpeedKey, current.throwSpeed);
		ini.SetDoubleValue(kThrowSection, kGravityMultKey, current.throwGravityMult);

		// Sin firma BOM: el INI distribuido no la lleva.
		if (ini.SaveFile(Constants::kInputConfigPath, false) < 0) {
			logs::error("Settings::Save: no se pudo escribir {}.", Constants::kInputConfigPath);
			return false;
		}

		logs::info("Settings::Save: configuración guardada en {}.", Constants::kInputConfigPath);
		return true;
	}

	void ResetToDefaults()
	{
		std::scoped_lock lock(g_mutex);
		g_values = Values{};
	}

	ActionBinding GetActionBinding()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.binding;
	}

	void SetActionBinding(const ActionBinding& a_binding)
	{
		std::scoped_lock lock(g_mutex);
		g_values.binding = a_binding;
	}

	float GetThrowSpeed()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.throwSpeed;
	}

	void SetThrowSpeed(float a_speed)
	{
		std::scoped_lock lock(g_mutex);
		g_values.throwSpeed = Clamp(a_speed, kThrowSpeedMin, kThrowSpeedMax);
	}

	float GetThrowGravityMult()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.throwGravityMult;
	}

	void SetThrowGravityMult(float a_mult)
	{
		std::scoped_lock lock(g_mutex);
		g_values.throwGravityMult = Clamp(a_mult, kThrowGravityMultMin, kThrowGravityMultMax);
	}
}
