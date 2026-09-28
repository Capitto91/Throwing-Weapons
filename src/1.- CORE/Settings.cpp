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
		constexpr const char* kDamageSection = "Damage";
		constexpr const char* kVfxSection = "VFX";

		// Nombres de clave de [Controls] sin cambiar (AimDevice/AimKeyCode)
		// aunque ya no haya fase de apuntado: renombrarlas rompería los INI
		// que ya tengan los usuarios.
		constexpr const char* kDeviceKey = "AimDevice";
		constexpr const char* kKeyCodeKey = "AimKeyCode";
		constexpr const char* kSpeedKey = "Speed";
		constexpr const char* kGravityMultKey = "GravityMultiplier";
		constexpr const char* kThrowHitMultKey = "ThrowHitMultiplier";
		constexpr const char* kReturnHitMultKey = "ReturnHitMultiplier";
		constexpr const char* kReturnStaggerKey = "ReturnStagger";
		constexpr const char* kHazardOnActorKey = "HazardOnActor";
		constexpr const char* kHazardOnSurfaceKey = "HazardOnSurface";
		constexpr const char* kImpactExplosionKey = "ImpactExplosion";
		constexpr const char* kTrailKey = "Trail";
		constexpr const char* kParticlesKey = "Particles";
		constexpr const char* kWeaponLightKey = "WeaponLight";
		constexpr const char* kHandEffectKey = "HandEffect";
		constexpr const char* kPowerAttackEffectsKey = "PowerAttackEffects";
		constexpr const char* kGlowModeKey = "GlowMode";
		constexpr const char* kGlowConditionKey = "GlowCondition";
		constexpr const char* kGlowNearDragonsKey = "GlowNearDragons";
		constexpr const char* kGlowNearUndeadKey = "GlowNearUndead";
		constexpr const char* kGlowNearDaedraKey = "GlowNearDaedra";
		constexpr const char* kGlowRadiusKey = "GlowRadius";
		constexpr const char* kGlowIntensityKey = "GlowIntensity";
		constexpr const char* kGlowPulseSpeedKey = "GlowPulseSpeed";

		struct Values
		{
			ActionBinding binding{ kDefaultActionBinding };
			float         throwSpeed{ kDefaultThrowSpeed };
			float         throwGravityMult{ kDefaultThrowGravityMult };
			float         throwHitMult{ kDefaultThrowHitMult };
			float         returnHitMult{ kDefaultReturnHitMult };
			bool          returnStagger{ kDefaultReturnStagger };
			bool          hazardOnActor{ kDefaultHazardOnActor };
			bool          hazardOnSurface{ kDefaultHazardOnSurface };
			bool          impactExplosion{ kDefaultImpactExplosion };
			bool          trail{ kDefaultTrail };
			bool          particles{ kDefaultParticles };
			bool          weaponLight{ kDefaultWeaponLight };
			bool          handEffect{ kDefaultHandEffect };
			bool          powerAttackEffects{ kDefaultPowerAttackEffects };
			GlowMode      glowMode{ kDefaultGlowMode };
			GlowCondition glowCondition{ kDefaultGlowCondition };
			bool          glowNearDragons{ kDefaultGlowNearDragons };
			bool          glowNearUndead{ kDefaultGlowNearUndead };
			bool          glowNearDaedra{ kDefaultGlowNearDaedra };
			float         glowRadius{ kDefaultGlowRadius };
			float         glowIntensity{ kDefaultGlowIntensity };
			float         glowPulseSpeed{ kDefaultGlowPulseSpeed };
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

		// Valores del INI en texto ("Off"/"Constant"/"Pulse",
		// "Always"/"NearCreatures"), igual que AimDevice.
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

		GlowMode ParseGlowMode(std::string_view a_value)
		{
			if (a_value == "Off") {
				return GlowMode::kOff;
			}
			if (a_value == "Pulse") {
				return GlowMode::kPulse;
			}
			if (a_value == "Constant") {
				return GlowMode::kConstant;
			}
			return kDefaultGlowMode;
		}

		const char* GlowConditionToString(GlowCondition a_condition)
		{
			return a_condition == GlowCondition::kNearCreatures ? "NearCreatures" : "Always";
		}

		GlowCondition ParseGlowCondition(std::string_view a_value)
		{
			return a_value == "NearCreatures" ? GlowCondition::kNearCreatures : GlowCondition::kAlways;
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
			loaded.throwHitMult = Clamp(static_cast<float>(ini.GetDoubleValue(kDamageSection, kThrowHitMultKey, kDefaultThrowHitMult)), kHitMultMin, kHitMultMax);
			loaded.returnHitMult = Clamp(static_cast<float>(ini.GetDoubleValue(kDamageSection, kReturnHitMultKey, kDefaultReturnHitMult)), kHitMultMin, kHitMultMax);
			loaded.returnStagger = ini.GetBoolValue(kDamageSection, kReturnStaggerKey, kDefaultReturnStagger);
			loaded.hazardOnActor = ini.GetBoolValue(kDamageSection, kHazardOnActorKey, kDefaultHazardOnActor);
			loaded.hazardOnSurface = ini.GetBoolValue(kDamageSection, kHazardOnSurfaceKey, kDefaultHazardOnSurface);
			loaded.impactExplosion = ini.GetBoolValue(kDamageSection, kImpactExplosionKey, kDefaultImpactExplosion);
			loaded.trail = ini.GetBoolValue(kVfxSection, kTrailKey, kDefaultTrail);
			loaded.particles = ini.GetBoolValue(kVfxSection, kParticlesKey, kDefaultParticles);
			loaded.weaponLight = ini.GetBoolValue(kVfxSection, kWeaponLightKey, kDefaultWeaponLight);
			loaded.handEffect = ini.GetBoolValue(kVfxSection, kHandEffectKey, kDefaultHandEffect);
			loaded.powerAttackEffects = ini.GetBoolValue(kVfxSection, kPowerAttackEffectsKey, kDefaultPowerAttackEffects);
			loaded.glowMode = ParseGlowMode(ini.GetValue(kVfxSection, kGlowModeKey, GlowModeToString(kDefaultGlowMode)));
			loaded.glowCondition = ParseGlowCondition(ini.GetValue(kVfxSection, kGlowConditionKey, GlowConditionToString(kDefaultGlowCondition)));
			loaded.glowNearDragons = ini.GetBoolValue(kVfxSection, kGlowNearDragonsKey, kDefaultGlowNearDragons);
			loaded.glowNearUndead = ini.GetBoolValue(kVfxSection, kGlowNearUndeadKey, kDefaultGlowNearUndead);
			loaded.glowNearDaedra = ini.GetBoolValue(kVfxSection, kGlowNearDaedraKey, kDefaultGlowNearDaedra);
			loaded.glowRadius = Clamp(static_cast<float>(ini.GetDoubleValue(kVfxSection, kGlowRadiusKey, kDefaultGlowRadius)), kGlowRadiusMin, kGlowRadiusMax);
			loaded.glowIntensity = Clamp(static_cast<float>(ini.GetDoubleValue(kVfxSection, kGlowIntensityKey, kDefaultGlowIntensity)), kGlowIntensityMin, kGlowIntensityMax);
			loaded.glowPulseSpeed = Clamp(static_cast<float>(ini.GetDoubleValue(kVfxSection, kGlowPulseSpeedKey, kDefaultGlowPulseSpeed)), kGlowPulseSpeedMin, kGlowPulseSpeedMax);
		}

		{
			std::scoped_lock lock(g_mutex);
			g_values = loaded;
		}

		logs::info("Settings::Load: tecla {} {} | velocidad {:.0f} u/s | multiplicador de gravedad {:.2f} | daño ida {:.2f} / regreso {:.2f} | stagger regreso {} | hazard actor {} / superficie {} | explosión {} | VFX trail {} / partículas {} / luz {} / manos {} / power attack {}.",
			DeviceToString(loaded.binding.device), loaded.binding.keyCode, loaded.throwSpeed, loaded.throwGravityMult, loaded.throwHitMult, loaded.returnHitMult,
			loaded.returnStagger, loaded.hazardOnActor, loaded.hazardOnSurface, loaded.impactExplosion,
			loaded.trail, loaded.particles, loaded.weaponLight, loaded.handEffect, loaded.powerAttackEffects);
		logs::info("Settings::Load: glow del arma {} / {} | dragones {} / no muertos {} / daedra {} | radio {:.0f} | intensidad {:.2f} | pulso {:.2f} Hz.",
			GlowModeToString(loaded.glowMode), GlowConditionToString(loaded.glowCondition),
			loaded.glowNearDragons, loaded.glowNearUndead, loaded.glowNearDaedra,
			loaded.glowRadius, loaded.glowIntensity, loaded.glowPulseSpeed);
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
		ini.SetDoubleValue(kDamageSection, kThrowHitMultKey, current.throwHitMult);
		ini.SetDoubleValue(kDamageSection, kReturnHitMultKey, current.returnHitMult);
		ini.SetBoolValue(kDamageSection, kReturnStaggerKey, current.returnStagger);
		ini.SetBoolValue(kDamageSection, kHazardOnActorKey, current.hazardOnActor);
		ini.SetBoolValue(kDamageSection, kHazardOnSurfaceKey, current.hazardOnSurface);
		ini.SetBoolValue(kDamageSection, kImpactExplosionKey, current.impactExplosion);
		ini.SetBoolValue(kVfxSection, kTrailKey, current.trail);
		ini.SetBoolValue(kVfxSection, kParticlesKey, current.particles);
		ini.SetBoolValue(kVfxSection, kWeaponLightKey, current.weaponLight);
		ini.SetBoolValue(kVfxSection, kHandEffectKey, current.handEffect);
		ini.SetBoolValue(kVfxSection, kPowerAttackEffectsKey, current.powerAttackEffects);
		ini.SetValue(kVfxSection, kGlowModeKey, GlowModeToString(current.glowMode));
		ini.SetValue(kVfxSection, kGlowConditionKey, GlowConditionToString(current.glowCondition));
		ini.SetBoolValue(kVfxSection, kGlowNearDragonsKey, current.glowNearDragons);
		ini.SetBoolValue(kVfxSection, kGlowNearUndeadKey, current.glowNearUndead);
		ini.SetBoolValue(kVfxSection, kGlowNearDaedraKey, current.glowNearDaedra);
		ini.SetDoubleValue(kVfxSection, kGlowRadiusKey, current.glowRadius);
		ini.SetDoubleValue(kVfxSection, kGlowIntensityKey, current.glowIntensity);
		ini.SetDoubleValue(kVfxSection, kGlowPulseSpeedKey, current.glowPulseSpeed);

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

	float GetThrowHitMult()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.throwHitMult;
	}

	void SetThrowHitMult(float a_mult)
	{
		std::scoped_lock lock(g_mutex);
		g_values.throwHitMult = Clamp(a_mult, kHitMultMin, kHitMultMax);
	}

	float GetReturnHitMult()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.returnHitMult;
	}

	void SetReturnHitMult(float a_mult)
	{
		std::scoped_lock lock(g_mutex);
		g_values.returnHitMult = Clamp(a_mult, kHitMultMin, kHitMultMax);
	}

	bool GetReturnStagger()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.returnStagger;
	}

	void SetReturnStagger(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.returnStagger = a_enabled;
	}

	bool GetHazardOnActor()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.hazardOnActor;
	}

	void SetHazardOnActor(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.hazardOnActor = a_enabled;
	}

	bool GetHazardOnSurface()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.hazardOnSurface;
	}

	void SetHazardOnSurface(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.hazardOnSurface = a_enabled;
	}

	bool GetImpactExplosion()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.impactExplosion;
	}

	void SetImpactExplosion(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.impactExplosion = a_enabled;
	}

	bool GetTrail()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.trail;
	}

	void SetTrail(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.trail = a_enabled;
	}

	bool GetParticles()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.particles;
	}

	void SetParticles(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.particles = a_enabled;
	}

	bool GetWeaponLight()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.weaponLight;
	}

	void SetWeaponLight(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.weaponLight = a_enabled;
	}

	bool GetHandEffect()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.handEffect;
	}

	void SetHandEffect(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.handEffect = a_enabled;
	}

	bool GetPowerAttackEffects()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.powerAttackEffects;
	}

	void SetPowerAttackEffects(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.powerAttackEffects = a_enabled;
	}

	GlowMode GetGlowMode()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowMode;
	}

	void SetGlowMode(GlowMode a_mode)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowMode = a_mode;
	}

	GlowCondition GetGlowCondition()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowCondition;
	}

	void SetGlowCondition(GlowCondition a_condition)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowCondition = a_condition;
	}

	bool GetGlowNearDragons()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowNearDragons;
	}

	void SetGlowNearDragons(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowNearDragons = a_enabled;
	}

	bool GetGlowNearUndead()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowNearUndead;
	}

	void SetGlowNearUndead(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowNearUndead = a_enabled;
	}

	bool GetGlowNearDaedra()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowNearDaedra;
	}

	void SetGlowNearDaedra(bool a_enabled)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowNearDaedra = a_enabled;
	}

	float GetGlowRadius()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowRadius;
	}

	void SetGlowRadius(float a_radius)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowRadius = Clamp(a_radius, kGlowRadiusMin, kGlowRadiusMax);
	}

	float GetGlowIntensity()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowIntensity;
	}

	void SetGlowIntensity(float a_intensity)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowIntensity = Clamp(a_intensity, kGlowIntensityMin, kGlowIntensityMax);
	}

	float GetGlowPulseSpeed()
	{
		std::scoped_lock lock(g_mutex);
		return g_values.glowPulseSpeed;
	}

	void SetGlowPulseSpeed(float a_speed)
	{
		std::scoped_lock lock(g_mutex);
		g_values.glowPulseSpeed = Clamp(a_speed, kGlowPulseSpeedMin, kGlowPulseSpeedMax);
	}
}
