// Configuración del plugin editable en tiempo de ejecución.
// Punto único de lectura/escritura del INI (Constants::kInputConfigPath) para
// los ajustes que puede cambiar el usuario desde el menú del juego (SKSE Menu
// Framework): el resto del plugin consulta aquí el valor en uso en cada
// momento, en vez de leer el INI una sola vez al arrancar, así que un cambio
// desde el menú se aplica al instante.
//
// Cubre [Controls], [Throw], [Damage] y [VFX].

#pragma once

namespace Settings
{
	// Dispositivo y código de botón de la acción de lanzar / recuperar el
	// arma (un toque: pulsar y soltar).
	struct ActionBinding
	{
		RE::INPUT_DEVICE device{ RE::INPUT_DEVICE::kKeyboard };
		std::uint32_t    keyCode{ 0 };
	};

	// Valores por defecto, usados si falta la clave en el INI (o no existe
	// el archivo) y por ResetToDefaults. Tecla G (DIK_G = 34), la misma que
	// trae el INI distribuido -- antes el código caía a la R (0x13) si
	// faltaba la clave, sin coincidir con el INI.
	inline constexpr ActionBinding kDefaultActionBinding{ RE::INPUT_DEVICE::kKeyboard, 34 };
	inline constexpr float         kDefaultThrowSpeed = 5000.0f;  // u/s
	inline constexpr float         kDefaultThrowGravityMult = 0.35f;

	// Fracción del daño real del arma (HitData::Populate, ver
	// Combat::ApplyWeaponHit) aplicada en el golpe inicial de la ida y en
	// cada golpe del regreso (punto 9). Decisión del usuario 2026-09-27:
	// 75% / 25%.
	inline constexpr float kDefaultThrowHitMult = 0.75f;
	inline constexpr float kDefaultReturnHitMult = 0.25f;

	// Interruptores de [Damage] (2026-09-27, a petición del usuario):
	// - ReturnStagger: el golpe del regreso tambalea al objetivo
	//   (Combat::ApplyReturnHit). El golpe inicial de la ida no tambalea
	//   nunca -- el objetivo queda paralizado (ver Combat::ApplyWeaponHit).
	// - HazardOnActor / HazardOnSurface: descarga eléctrica (BGSHazard) al
	//   impactar contra un actor / una superficie en la ida
	//   (Combat::SpawnActorHazard / SpawnSurfaceHazard). Ojo: contra un
	//   actor, el hazard ES el daño continuo mientras el arma sigue
	//   clavada; desactivarlo deja solo el golpe inicial.
	inline constexpr bool kDefaultReturnStagger = true;
	inline constexpr bool kDefaultHazardOnActor = true;
	inline constexpr bool kDefaultHazardOnSurface = true;

	// - ImpactExplosion: explosión en cada impacto de la ida, contra actor o
	//   superficie (Animation::SpawnImpactVFX, BGSExplosion propio
	//   Constants::kImpactExplosionLocalFormID). Incluye lo que tenga
	//   configurado ese BGSExplosion en la Creation Kit (luz, sonido, y
	//   daño/empuje si los tuviera). 2026-09-28, a petición del usuario.
	inline constexpr bool kDefaultImpactExplosion = true;

	// [VFX] (2026-09-28, a petición del usuario): efectos puramente
	// visuales que se pueden desactivar. Se consultan al ARRANCAR cada
	// efecto, así que un cambio con el arma ya en vuelo se nota en el
	// siguiente tramo (lanzamiento o regreso), no en el que está en curso.
	// - Trail: estela de rayo en vuelo (Animation::WeaponTrailGroup).
	// - Particles: chispas mientras el arma se mueve (Animation::WeaponVFX).
	// - WeaponLight: destello ThorMjolnirLight.nif (Animation::WeaponGlow).
	// - HandEffect: brillo de manos al lanzar (Animation::TriggerHandGlow).
	inline constexpr bool kDefaultTrail = true;
	inline constexpr bool kDefaultParticles = true;
	inline constexpr bool kDefaultWeaponLight = true;
	inline constexpr bool kDefaultHandEffect = true;

	// - PowerAttackEffects: chispas + destello con luz durante cada power
	//   attack cuerpo a cuerpo con el arma en la mano
	//   (Animation::PowerAttackVFX). Independiente de Particles/WeaponLight,
	//   que solo afectan al lanzamiento. Se consulta al empezar cada golpe.
	inline constexpr bool kDefaultPowerAttackEffects = true;

	// Glow de la textura del propio martillo (glow map del .nif del arma,
	// Animation::GlowMapControl), 2026-09-28 a petición del usuario. Por
	// defecto Constant + Always al 100%: el aspecto de siempre.
	// - GlowMode: apagado, constante o en pulso.
	// - GlowCondition: siempre, o solo con criaturas de los tipos marcados
	//   (keywords de raza ActorTypeDragon/Undead/Daedra) vivas dentro de
	//   GlowRadius.
	// - GlowIntensity: multiplicador sobre el valor del .nif (constante, y
	//   máximo del pulso). GlowPulseSpeed: pulsos por segundo.
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
	inline constexpr float         kDefaultGlowRadius = 2000.0f;  // unidades
	inline constexpr float         kDefaultGlowIntensity = 1.0f;
	inline constexpr float         kDefaultGlowPulseSpeed = 1.0f;  // Hz

	// Rangos válidos: Load recorta a ellos lo que venga del INI, y el menú
	// los usa como límites de sus controles. Placeholders razonables, sin
	// ninguna referencia del motor detrás.
	inline constexpr float kThrowSpeedMin = 1000.0f;
	inline constexpr float kThrowSpeedMax = 15000.0f;
	inline constexpr float kThrowGravityMultMin = 0.0f;
	inline constexpr float kThrowGravityMultMax = 2.0f;
	inline constexpr float kHitMultMin = 0.0f;
	inline constexpr float kHitMultMax = 2.0f;
	inline constexpr float kGlowRadiusMin = 500.0f;
	inline constexpr float kGlowRadiusMax = 8000.0f;
	inline constexpr float kGlowIntensityMin = 0.0f;
	inline constexpr float kGlowIntensityMax = 3.0f;
	inline constexpr float kGlowPulseSpeedMin = 0.1f;
	inline constexpr float kGlowPulseSpeedMax = 5.0f;

	// Lee [Controls], [Throw], [Damage] y [VFX] del INI. Claves ausentes (o archivo
	// inexistente) -> valor por defecto. Llamar una vez al cargar el plugin,
	// antes de Input::InputManager::Init.
	void Load();

	// Escribe los valores actuales de [Controls], [Throw], [Damage] y [VFX] en el INI,
	// conservando el resto del archivo (otras secciones, comentarios).
	// Devuelve false si no se pudo guardar.
	bool Save();

	// Vuelve a los valores por defecto (sin guardar: eso lo decide Save).
	void ResetToDefaults();

	// Accesores seguros entre hilos (el menú se dibuja fuera del bucle de
	// juego, y los valores se leen desde el hilo principal).
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

	// Nombre del dispositivo tal como se escribe en el INI
	// ("Keyboard"/"Mouse"/"Gamepad").
	[[nodiscard]] const char* DeviceToString(RE::INPUT_DEVICE a_device);
}
