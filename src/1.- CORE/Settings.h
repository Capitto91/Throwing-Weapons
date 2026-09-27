// Configuración del plugin editable en tiempo de ejecución.
// Punto único de lectura/escritura del INI (Constants::kInputConfigPath) para
// los ajustes que puede cambiar el usuario desde el menú del juego (SKSE Menu
// Framework): el resto del plugin consulta aquí el valor en uso en cada
// momento, en vez de leer el INI una sola vez al arrancar, así que un cambio
// desde el menú se aplica al instante.
//
// De momento cubre [Controls] y [Throw]. [Damage] sigue leyéndose en
// Combat::Init (7.- COMBAT/DamageManager.cpp) -- pendiente de pasar aquí.

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

	// Rangos válidos: Load recorta a ellos lo que venga del INI, y el menú
	// los usa como límites de sus controles. Placeholders razonables, sin
	// ninguna referencia del motor detrás.
	inline constexpr float kThrowSpeedMin = 1000.0f;
	inline constexpr float kThrowSpeedMax = 15000.0f;
	inline constexpr float kThrowGravityMultMin = 0.0f;
	inline constexpr float kThrowGravityMultMax = 2.0f;

	// Lee [Controls] y [Throw] del INI. Claves ausentes (o archivo
	// inexistente) -> valor por defecto. Llamar una vez al cargar el plugin,
	// antes de Input::InputManager::Init.
	void Load();

	// Escribe los valores actuales de [Controls] y [Throw] en el INI,
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

	// Nombre del dispositivo tal como se escribe en el INI
	// ("Keyboard"/"Mouse"/"Gamepad").
	[[nodiscard]] const char* DeviceToString(RE::INPUT_DEVICE a_device);
}
