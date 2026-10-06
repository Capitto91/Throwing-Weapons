// Ajustes editables en juego -- ver Settings.h.

#include "1.- CORE/Settings.h"

#include "1.- CORE/Constants.h"

#include <SimpleIni.h>

#include <array>
#include <mutex>
#include <variant>

namespace Settings
{
	namespace
	{
		constexpr const char* kControlsSection = "Controls";
		constexpr const char* kThrowSection = "Throw";
		constexpr const char* kDamageSection = "Damage";
		constexpr const char* kVfxSection = "VFX";
		constexpr const char* kDebugSection = "Debug";

		struct Values
		{
			RE::INPUT_DEVICE device{ kDefaultActionBinding.device };
			std::uint32_t    keyCode{ kDefaultActionBinding.keyCode };
			float            throwSpeed{ kDefaultThrowSpeed };
			float            throwGravityMult{ kDefaultThrowGravityMult };
			float            throwHitMult{ kDefaultThrowHitMult };
			float            returnHitMult{ kDefaultReturnHitMult };
			bool             returnStagger{ kDefaultReturnStagger };
			bool             hazardOnActor{ kDefaultHazardOnActor };
			bool             hazardOnSurface{ kDefaultHazardOnSurface };
			bool             impactExplosion{ kDefaultImpactExplosion };
			bool             trail{ kDefaultTrail };
			bool             particles{ kDefaultParticles };
			bool             weaponLight{ kDefaultWeaponLight };
			bool             handEffect{ kDefaultHandEffect };
			bool             powerAttackEffects{ kDefaultPowerAttackEffects };
			float            particleAmount{ kDefaultParticleAmount };
			float            particleLifetime{ kDefaultParticleLifetime };
			GlowMode         glowMode{ kDefaultGlowMode };
			GlowCondition    glowCondition{ kDefaultGlowCondition };
			bool             glowNearDragons{ kDefaultGlowNearDragons };
			bool             glowNearUndead{ kDefaultGlowNearUndead };
			bool             glowNearDaedra{ kDefaultGlowNearDaedra };
			float            glowRadius{ kDefaultGlowRadius };
			float            glowIntensity{ kDefaultGlowIntensity };
			float            glowPulseSpeed{ kDefaultGlowPulseSpeed };
			bool             cameraShake{ kDefaultCameraShake };
			float            cameraShakeAngle{ kDefaultCameraShakeAngle };
			float            cameraShakeDuration{ kDefaultCameraShakeDuration };
			float            cameraShakeFrequency{ kDefaultCameraShakeFrequency };
			bool             performanceLog{ kDefaultPerformanceLog };
		};

		std::mutex g_mutex;
		Values     g_values;

		// Valores de texto del INI para el modo y la condición del glow.
		const char* GlowModeToString(GlowMode a_mode)
		{
			switch (a_mode) {
			case GlowMode::kOff:
				return "Off";
			case GlowMode::kPulse:
				return "Pulse";
			default:
				return "Constant";
			}
		}

		const char* GlowConditionToString(GlowCondition a_condition)
		{
			return a_condition == GlowCondition::kNearCreatures ? "NearCreatures" : "Always";
		}

		// Campos de texto de Values: escritura al INI y lectura desde él (un texto desconocido da el valor por defecto).
		const char* WriteDevice(const Values& a_values) { return DeviceToString(a_values.device); }
		const char* WriteGlowMode(const Values& a_values) { return GlowModeToString(a_values.glowMode); }
		const char* WriteGlowCondition(const Values& a_values) { return GlowConditionToString(a_values.glowCondition); }

		void ReadDevice(Values& a_values, std::string_view a_text)
		{
			if (a_text == "Mouse") {
				a_values.device = RE::INPUT_DEVICE::kMouse;
			} else if (a_text == "Gamepad") {
				a_values.device = RE::INPUT_DEVICE::kGamepad;
			} else {
				a_values.device = RE::INPUT_DEVICE::kKeyboard;
			}
		}

		void ReadGlowMode(Values& a_values, std::string_view a_text)
		{
			if (a_text == "Off") {
				a_values.glowMode = GlowMode::kOff;
			} else if (a_text == "Pulse") {
				a_values.glowMode = GlowMode::kPulse;
			} else if (a_text == "Constant") {
				a_values.glowMode = GlowMode::kConstant;
			} else {
				a_values.glowMode = kDefaultGlowMode;
			}
		}

		void ReadGlowCondition(Values& a_values, std::string_view a_text)
		{
			a_values.glowCondition = a_text == "NearCreatures" ? GlowCondition::kNearCreatures : GlowCondition::kAlways;
		}

		// Tipos de clave del INI: número con rango, sí/no, entero sin signo y texto con su conversión.
		struct FloatKey
		{
			float Values::*member;
			float          min;
			float          max;
		};

		struct BoolKey
		{
			bool Values::*member;
		};

		struct UIntKey
		{
			std::uint32_t Values::*member;
		};

		struct TextKey
		{
			const char* (*write)(const Values&);
			void (*read)(Values&, std::string_view);
		};

		// Clave del INI: sección, nombre y campo de Values.
		struct Key
		{
			const char*                                       section;
			const char*                                       name;
			std::variant<FloatKey, BoolKey, UIntKey, TextKey> field;
		};

		// Todas las claves, en el orden en que Load y Save las recorren (el de un INI nuevo). [Controls] conserva los
		// nombres antiguos (AimDevice/AimKeyCode) para no romper los INI existentes.
		constexpr std::array kKeys{
			Key{ kControlsSection, "AimDevice", TextKey{ WriteDevice, ReadDevice } },
			Key{ kControlsSection, "AimKeyCode", UIntKey{ &Values::keyCode } },
			Key{ kThrowSection, "Speed", FloatKey{ &Values::throwSpeed, kThrowSpeedMin, kThrowSpeedMax } },
			Key{ kThrowSection, "GravityMultiplier", FloatKey{ &Values::throwGravityMult, kThrowGravityMultMin, kThrowGravityMultMax } },
			Key{ kDamageSection, "ThrowHitMultiplier", FloatKey{ &Values::throwHitMult, kHitMultMin, kHitMultMax } },
			Key{ kDamageSection, "ReturnHitMultiplier", FloatKey{ &Values::returnHitMult, kHitMultMin, kHitMultMax } },
			Key{ kDamageSection, "ReturnStagger", BoolKey{ &Values::returnStagger } },
			Key{ kDamageSection, "HazardOnActor", BoolKey{ &Values::hazardOnActor } },
			Key{ kDamageSection, "HazardOnSurface", BoolKey{ &Values::hazardOnSurface } },
			Key{ kDamageSection, "ImpactExplosion", BoolKey{ &Values::impactExplosion } },
			Key{ kVfxSection, "Trail", BoolKey{ &Values::trail } },
			Key{ kVfxSection, "Particles", BoolKey{ &Values::particles } },
			Key{ kVfxSection, "WeaponLight", BoolKey{ &Values::weaponLight } },
			Key{ kVfxSection, "HandEffect", BoolKey{ &Values::handEffect } },
			Key{ kVfxSection, "PowerAttackEffects", BoolKey{ &Values::powerAttackEffects } },
			Key{ kVfxSection, "ParticleAmount", FloatKey{ &Values::particleAmount, kParticleAmountMin, kParticleAmountMax } },
			Key{ kVfxSection, "ParticleLifetime", FloatKey{ &Values::particleLifetime, kParticleLifetimeMin, kParticleLifetimeMax } },
			Key{ kVfxSection, "GlowMode", TextKey{ WriteGlowMode, ReadGlowMode } },
			Key{ kVfxSection, "GlowCondition", TextKey{ WriteGlowCondition, ReadGlowCondition } },
			Key{ kVfxSection, "GlowNearDragons", BoolKey{ &Values::glowNearDragons } },
			Key{ kVfxSection, "GlowNearUndead", BoolKey{ &Values::glowNearUndead } },
			Key{ kVfxSection, "GlowNearDaedra", BoolKey{ &Values::glowNearDaedra } },
			Key{ kVfxSection, "GlowRadius", FloatKey{ &Values::glowRadius, kGlowRadiusMin, kGlowRadiusMax } },
			Key{ kVfxSection, "GlowIntensity", FloatKey{ &Values::glowIntensity, kGlowIntensityMin, kGlowIntensityMax } },
			Key{ kVfxSection, "GlowPulseSpeed", FloatKey{ &Values::glowPulseSpeed, kGlowPulseSpeedMin, kGlowPulseSpeedMax } },
			Key{ kVfxSection, "CameraShake", BoolKey{ &Values::cameraShake } },
			Key{ kVfxSection, "CameraShakeAngle", FloatKey{ &Values::cameraShakeAngle, kCameraShakeAngleMin, kCameraShakeAngleMax } },
			Key{ kVfxSection, "CameraShakeDuration", FloatKey{ &Values::cameraShakeDuration, kCameraShakeDurationMin, kCameraShakeDurationMax } },
			Key{ kVfxSection, "CameraShakeFrequency", FloatKey{ &Values::cameraShakeFrequency, kCameraShakeFrequencyMin, kCameraShakeFrequencyMax } },
			Key{ kDebugSection, "PerformanceLog", BoolKey{ &Values::performanceLog } },
		};

		// Junta varias lambdas en un solo visitante de std::visit.
		template <class... Ts>
		struct Overloaded : Ts...
		{
			using Ts::operator()...;
		};

		// Sin std::clamp: Windows.h define min/max como macros.
		float Clamp(float a_value, float a_min, float a_max)
		{
			return a_value < a_min ? a_min : (a_value > a_max ? a_max : a_value);
		}

		// Recorta a_value al rango de a_member en kKeys.
		float ClampToKeyRange(float Values::*a_member, float a_value)
		{
			for (const auto& key : kKeys) {
				if (const auto* floatKey = std::get_if<FloatKey>(&key.field); floatKey && floatKey->member == a_member) {
					return Clamp(a_value, floatKey->min, floatKey->max);
				}
			}
			return a_value;
		}

		// Lectura y escritura de un campo con el mutex tomado; los números se recortan a su rango.
		template <class T>
		T Read(T Values::*a_member)
		{
			std::scoped_lock lock(g_mutex);
			return g_values.*a_member;
		}

		template <class T>
		void Write(T Values::*a_member, T a_value)
		{
			std::scoped_lock lock(g_mutex);
			g_values.*a_member = a_value;
		}

		void Write(float Values::*a_member, float a_value)
		{
			const float      clamped = ClampToKeyRange(a_member, a_value);
			std::scoped_lock lock(g_mutex);
			g_values.*a_member = clamped;
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
			// El valor por defecto de cada clave es el que ya tiene loaded.
			for (const auto& key : kKeys) {
				std::visit(Overloaded{
							   [&](const FloatKey& a_key) { loaded.*a_key.member = Clamp(static_cast<float>(ini.GetDoubleValue(key.section, key.name, loaded.*a_key.member)), a_key.min, a_key.max); },
							   [&](const BoolKey& a_key) { loaded.*a_key.member = ini.GetBoolValue(key.section, key.name, loaded.*a_key.member); },
							   [&](const UIntKey& a_key) { loaded.*a_key.member = static_cast<std::uint32_t>(ini.GetLongValue(key.section, key.name, static_cast<long>(loaded.*a_key.member))); },
							   [&](const TextKey& a_key) { a_key.read(loaded, ini.GetValue(key.section, key.name, a_key.write(loaded))); } },
					key.field);
			}
		}

		{
			std::scoped_lock lock(g_mutex);
			g_values = loaded;
		}

		logs::info("Settings::Load: tecla {} {} | velocidad {:.0f} u/s | multiplicador de gravedad {:.2f} | daño ida {:.2f} / regreso {:.2f} | stagger regreso {} | hazard actor {} / superficie {} | explosión {} | VFX trail {} / partículas {} (cantidad {:.2f}, vida {:.2f}) / luz {} / manos {} / power attack {}.",
			DeviceToString(loaded.device), loaded.keyCode, loaded.throwSpeed, loaded.throwGravityMult, loaded.throwHitMult, loaded.returnHitMult,
			loaded.returnStagger, loaded.hazardOnActor, loaded.hazardOnSurface, loaded.impactExplosion,
			loaded.trail, loaded.particles, loaded.particleAmount, loaded.particleLifetime, loaded.weaponLight, loaded.handEffect, loaded.powerAttackEffects);
		logs::info("Settings::Load: glow del arma {} / {} | dragones {} / no muertos {} / daedra {} | radio {:.0f} | intensidad {:.2f} | pulso {:.2f} Hz | golpe de cámara {} ({:.1f}°, {:.2f} s, {:.1f} rebotes/s) | registro de rendimiento {}.",
			GlowModeToString(loaded.glowMode), GlowConditionToString(loaded.glowCondition),
			loaded.glowNearDragons, loaded.glowNearUndead, loaded.glowNearDaedra,
			loaded.glowRadius, loaded.glowIntensity, loaded.glowPulseSpeed,
			loaded.cameraShake, loaded.cameraShakeAngle, loaded.cameraShakeDuration, loaded.cameraShakeFrequency, loaded.performanceLog);
	}

	bool Save()
	{
		Values current;
		{
			std::scoped_lock lock(g_mutex);
			current = g_values;
		}

		// Carga el archivo existente para conservar lo que no es nuestro.
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.LoadFile(Constants::kInputConfigPath);

		for (const auto& key : kKeys) {
			std::visit(Overloaded{
						   [&](const FloatKey& a_key) { ini.SetDoubleValue(key.section, key.name, current.*a_key.member); },
						   [&](const BoolKey& a_key) { ini.SetBoolValue(key.section, key.name, current.*a_key.member); },
						   [&](const UIntKey& a_key) { ini.SetLongValue(key.section, key.name, static_cast<long>(current.*a_key.member)); },
						   [&](const TextKey& a_key) { ini.SetValue(key.section, key.name, a_key.write(current)); } },
				key.field);
		}

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
		return { g_values.device, g_values.keyCode };
	}

	void SetActionBinding(const ActionBinding& a_binding)
	{
		std::scoped_lock lock(g_mutex);
		g_values.device = a_binding.device;
		g_values.keyCode = a_binding.keyCode;
	}

	float GetThrowSpeed() { return Read(&Values::throwSpeed); }
	void  SetThrowSpeed(float a_speed) { Write(&Values::throwSpeed, a_speed); }

	float GetThrowGravityMult() { return Read(&Values::throwGravityMult); }
	void  SetThrowGravityMult(float a_mult) { Write(&Values::throwGravityMult, a_mult); }

	float GetThrowHitMult() { return Read(&Values::throwHitMult); }
	void  SetThrowHitMult(float a_mult) { Write(&Values::throwHitMult, a_mult); }

	float GetReturnHitMult() { return Read(&Values::returnHitMult); }
	void  SetReturnHitMult(float a_mult) { Write(&Values::returnHitMult, a_mult); }

	bool GetReturnStagger() { return Read(&Values::returnStagger); }
	void SetReturnStagger(bool a_enabled) { Write(&Values::returnStagger, a_enabled); }

	bool GetHazardOnActor() { return Read(&Values::hazardOnActor); }
	void SetHazardOnActor(bool a_enabled) { Write(&Values::hazardOnActor, a_enabled); }

	bool GetHazardOnSurface() { return Read(&Values::hazardOnSurface); }
	void SetHazardOnSurface(bool a_enabled) { Write(&Values::hazardOnSurface, a_enabled); }

	bool GetImpactExplosion() { return Read(&Values::impactExplosion); }
	void SetImpactExplosion(bool a_enabled) { Write(&Values::impactExplosion, a_enabled); }

	bool GetTrail() { return Read(&Values::trail); }
	void SetTrail(bool a_enabled) { Write(&Values::trail, a_enabled); }

	bool GetParticles() { return Read(&Values::particles); }
	void SetParticles(bool a_enabled) { Write(&Values::particles, a_enabled); }

	bool GetWeaponLight() { return Read(&Values::weaponLight); }
	void SetWeaponLight(bool a_enabled) { Write(&Values::weaponLight, a_enabled); }

	bool GetHandEffect() { return Read(&Values::handEffect); }
	void SetHandEffect(bool a_enabled) { Write(&Values::handEffect, a_enabled); }

	bool GetPowerAttackEffects() { return Read(&Values::powerAttackEffects); }
	void SetPowerAttackEffects(bool a_enabled) { Write(&Values::powerAttackEffects, a_enabled); }

	float GetParticleAmount() { return Read(&Values::particleAmount); }
	void  SetParticleAmount(float a_mult) { Write(&Values::particleAmount, a_mult); }

	float GetParticleLifetime() { return Read(&Values::particleLifetime); }
	void  SetParticleLifetime(float a_mult) { Write(&Values::particleLifetime, a_mult); }

	GlowMode GetGlowMode() { return Read(&Values::glowMode); }
	void     SetGlowMode(GlowMode a_mode) { Write(&Values::glowMode, a_mode); }

	GlowCondition GetGlowCondition() { return Read(&Values::glowCondition); }
	void          SetGlowCondition(GlowCondition a_condition) { Write(&Values::glowCondition, a_condition); }

	bool GetGlowNearDragons() { return Read(&Values::glowNearDragons); }
	void SetGlowNearDragons(bool a_enabled) { Write(&Values::glowNearDragons, a_enabled); }

	bool GetGlowNearUndead() { return Read(&Values::glowNearUndead); }
	void SetGlowNearUndead(bool a_enabled) { Write(&Values::glowNearUndead, a_enabled); }

	bool GetGlowNearDaedra() { return Read(&Values::glowNearDaedra); }
	void SetGlowNearDaedra(bool a_enabled) { Write(&Values::glowNearDaedra, a_enabled); }

	float GetGlowRadius() { return Read(&Values::glowRadius); }
	void  SetGlowRadius(float a_radius) { Write(&Values::glowRadius, a_radius); }

	float GetGlowIntensity() { return Read(&Values::glowIntensity); }
	void  SetGlowIntensity(float a_intensity) { Write(&Values::glowIntensity, a_intensity); }

	float GetGlowPulseSpeed() { return Read(&Values::glowPulseSpeed); }
	void  SetGlowPulseSpeed(float a_speed) { Write(&Values::glowPulseSpeed, a_speed); }

	bool GetCameraShake() { return Read(&Values::cameraShake); }
	void SetCameraShake(bool a_enabled) { Write(&Values::cameraShake, a_enabled); }

	float GetCameraShakeAngle() { return Read(&Values::cameraShakeAngle); }
	void  SetCameraShakeAngle(float a_degrees) { Write(&Values::cameraShakeAngle, a_degrees); }

	float GetCameraShakeDuration() { return Read(&Values::cameraShakeDuration); }
	void  SetCameraShakeDuration(float a_seconds) { Write(&Values::cameraShakeDuration, a_seconds); }

	float GetCameraShakeFrequency() { return Read(&Values::cameraShakeFrequency); }
	void  SetCameraShakeFrequency(float a_bouncesPerSecond) { Write(&Values::cameraShakeFrequency, a_bouncesPerSecond); }

	bool GetPerformanceLog() { return Read(&Values::performanceLog); }
	void SetPerformanceLog(bool a_enabled) { Write(&Values::performanceLog, a_enabled); }
}
