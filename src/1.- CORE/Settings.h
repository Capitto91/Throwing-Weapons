// Ajustes editables en juego ([Controls], [Throw], [Damage], [VFX], [Debug]) leídos y guardados en el INI.
// Los consultan los módulos en cada uso y los edita UI::ConfigMenu.

#pragma once

namespace Settings
{
	// Dispositivo y botón de lanzar / recuperar.
	struct ActionBinding
	{
		RE::INPUT_DEVICE device{ RE::INPUT_DEVICE::kKeyboard };
		std::uint32_t    keyCode{ 0 };
	};

	// Valores por defecto, usados si falta la clave en el INI y por ResetToDefaults. Tecla G (34).
	inline constexpr ActionBinding kDefaultActionBinding{ RE::INPUT_DEVICE::kKeyboard, 34 };
	inline constexpr float         kDefaultThrowSpeed = 5000.0f;  // u/s
	inline constexpr float         kDefaultThrowGravityMult = 0.35f;

	// Fracción del daño real del arma en el golpe de la ida y en cada golpe del regreso.
	inline constexpr float kDefaultThrowHitMult = 0.75f;
	inline constexpr float kDefaultReturnHitMult = 0.25f;

	// [Damage]: tambaleo en los golpes del regreso y descarga eléctrica (BGSHazard)
	// al clavarse en un actor o una superficie.
	inline constexpr bool kDefaultReturnStagger = true;
	inline constexpr bool kDefaultHazardOnActor = true;
	inline constexpr bool kDefaultHazardOnSurface = true;

	// Explosión en cada impacto de la ida (Animation::SpawnImpactVFX).
	inline constexpr bool kDefaultImpactExplosion = true;

	// [VFX]: estela, chispas, destello con luz y brillo de manos del lanzamiento.
	// Se leen al arrancar cada efecto.
	inline constexpr bool kDefaultTrail = true;
	inline constexpr bool kDefaultParticles = true;
	inline constexpr bool kDefaultWeaponLight = true;
	inline constexpr bool kDefaultHandEffect = true;

	// Chispas y destello durante los power attacks (Animation::PowerAttackVFX).
	inline constexpr bool kDefaultPowerAttackEffects = true;

	// Multiplicadores sobre el NIF de las chispas (Animation::WeaponVFX, también en los power attacks): partículas
	// por segundo y vida de cada partícula. Se aplican en vivo.
	inline constexpr float kDefaultParticleAmount = 1.0f;
	inline constexpr float kDefaultParticleLifetime = 1.0f;

	// Glow de la textura del martillo (Animation::GlowMapControl): modo, condición,
	// tipos de criatura, radio, intensidad y velocidad del pulso.
	enum class GlowMode : std::int32_t
	{
		kOff = 0,
		kConstant = 1,
		kPulse = 2
	};

	enum class GlowCondition : std::int32_t
	{
		kAlways = 0,
		kNearCreatures = 1
	};

	inline constexpr GlowMode      kDefaultGlowMode = GlowMode::kConstant;
	inline constexpr GlowCondition kDefaultGlowCondition = GlowCondition::kAlways;
	inline constexpr bool          kDefaultGlowNearDragons = true;
	inline constexpr bool          kDefaultGlowNearUndead = true;
	inline constexpr bool          kDefaultGlowNearDaedra = true;
	inline constexpr float         kDefaultGlowRadius = 2000.0f;   // unidades
	inline constexpr float         kDefaultGlowIntensity = 1.0f;
	inline constexpr float         kDefaultGlowPulseSpeed = 1.0f;  // Hz

	// Golpe de cámara al atrapar (Animation::CameraKick, desde WeaponManager::PerformCatchReequip): activado, ángulo
	// del primer impulso hacia arriba, tiempo hasta apagarse y rebotes por segundo. Se leen en cada atrape.
	inline constexpr bool  kDefaultCameraShake = true;
	inline constexpr float kDefaultCameraShakeAngle = 6.0f;      // grados
	inline constexpr float kDefaultCameraShakeDuration = 1.0f;   // s
	inline constexpr float kDefaultCameraShakeFrequency = 3.0f;  // rebotes/s

	// [Debug]: línea de rendimiento del plugin en el log (PerfMonitor). Apagada: no se mide nada.
	inline constexpr bool kDefaultPerformanceLog = false;

	// Rangos válidos: Load recorta el INI a ellos y el menú los usa como límites.
	inline constexpr float kThrowSpeedMin = 1000.0f;
	inline constexpr float kThrowSpeedMax = 15000.0f;
	inline constexpr float kThrowGravityMultMin = 0.0f;
	inline constexpr float kThrowGravityMultMax = 2.0f;
	inline constexpr float kHitMultMin = 0.0f;
	inline constexpr float kHitMultMax = 2.0f;
	inline constexpr float kParticleAmountMin = 0.1f;
	inline constexpr float kParticleAmountMax = 2.0f;
	inline constexpr float kParticleLifetimeMin = 0.25f;
	inline constexpr float kParticleLifetimeMax = 2.0f;
	inline constexpr float kGlowRadiusMin = 500.0f;
	inline constexpr float kGlowRadiusMax = 8000.0f;
	inline constexpr float kGlowIntensityMin = 0.0f;
	inline constexpr float kGlowIntensityMax = 3.0f;
	inline constexpr float kGlowPulseSpeedMin = 0.1f;
	inline constexpr float kGlowPulseSpeedMax = 5.0f;
	inline constexpr float kCameraShakeAngleMin = 1.0f;
	inline constexpr float kCameraShakeAngleMax = 20.0f;
	inline constexpr float kCameraShakeDurationMin = 0.2f;
	inline constexpr float kCameraShakeDurationMax = 3.0f;
	inline constexpr float kCameraShakeFrequencyMin = 1.0f;
	inline constexpr float kCameraShakeFrequencyMax = 8.0f;

	// Lee el INI (valor por defecto si falta la clave). Lo llama Plugin::Init al arrancar.
	void Load();

	// Guarda los ajustes en el INI conservando el resto del archivo. false si falla.
	// Lo llama UI::ConfigMenu.
	bool Save();

	// Vuelve a los valores por defecto (sin guardar).
	void ResetToDefaults();

	// Accesores seguros entre hilos (menú y hilo principal).
	[[nodiscard]] ActionBinding GetActionBinding();
	void                        SetActionBinding(const ActionBinding& a_binding);

	[[nodiscard]] float GetThrowSpeed();
	void                SetThrowSpeed(float a_speed);

	[[nodiscard]] float GetThrowGravityMult();
	void                SetThrowGravityMult(float a_mult);

	[[nodiscard]] float GetThrowHitMult();
	void                SetThrowHitMult(float a_mult);

	[[nodiscard]] float GetReturnHitMult();
	void                SetReturnHitMult(float a_mult);

	[[nodiscard]] bool GetReturnStagger();
	void               SetReturnStagger(bool a_enabled);

	[[nodiscard]] bool GetHazardOnActor();
	void               SetHazardOnActor(bool a_enabled);

	[[nodiscard]] bool GetHazardOnSurface();
	void               SetHazardOnSurface(bool a_enabled);

	[[nodiscard]] bool GetImpactExplosion();
	void               SetImpactExplosion(bool a_enabled);

	[[nodiscard]] bool GetTrail();
	void               SetTrail(bool a_enabled);

	[[nodiscard]] bool GetParticles();
	void               SetParticles(bool a_enabled);

	[[nodiscard]] bool GetWeaponLight();
	void               SetWeaponLight(bool a_enabled);

	[[nodiscard]] bool GetHandEffect();
	void               SetHandEffect(bool a_enabled);

	[[nodiscard]] bool GetPowerAttackEffects();
	void               SetPowerAttackEffects(bool a_enabled);

	[[nodiscard]] float GetParticleAmount();
	void                SetParticleAmount(float a_mult);

	[[nodiscard]] float GetParticleLifetime();
	void                SetParticleLifetime(float a_mult);

	[[nodiscard]] GlowMode GetGlowMode();
	void                   SetGlowMode(GlowMode a_mode);

	[[nodiscard]] GlowCondition GetGlowCondition();
	void                        SetGlowCondition(GlowCondition a_condition);

	[[nodiscard]] bool GetGlowNearDragons();
	void               SetGlowNearDragons(bool a_enabled);

	[[nodiscard]] bool GetGlowNearUndead();
	void               SetGlowNearUndead(bool a_enabled);

	[[nodiscard]] bool GetGlowNearDaedra();
	void               SetGlowNearDaedra(bool a_enabled);

	[[nodiscard]] float GetGlowRadius();
	void                SetGlowRadius(float a_radius);

	[[nodiscard]] float GetGlowIntensity();
	void                SetGlowIntensity(float a_intensity);

	[[nodiscard]] float GetGlowPulseSpeed();
	void                SetGlowPulseSpeed(float a_speed);

	[[nodiscard]] bool GetCameraShake();
	void               SetCameraShake(bool a_enabled);

	[[nodiscard]] float GetCameraShakeAngle();
	void                SetCameraShakeAngle(float a_degrees);

	[[nodiscard]] float GetCameraShakeDuration();
	void                SetCameraShakeDuration(float a_seconds);

	[[nodiscard]] float GetCameraShakeFrequency();
	void                SetCameraShakeFrequency(float a_bouncesPerSecond);

	[[nodiscard]] bool GetPerformanceLog();
	void               SetPerformanceLog(bool a_enabled);

	// Nombre del dispositivo en el INI ("Keyboard"/"Mouse"/"Gamepad").
	[[nodiscard]] const char* DeviceToString(RE::INPUT_DEVICE a_device);
}
