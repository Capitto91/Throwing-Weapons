// Ritmo de nacimiento y vida de los sistemas de partículas de un NIF instanciado: guarda los valores del NIF una vez
// por sesión y escribe valor del NIF × multiplicador, sin acumular. Lo usa Animation::WeaponVFX.

#pragma once

namespace ParticleUtils
{
	// Emisor de un NiParticleSystem con sus valores originales del NIF. Un puntero nulo es un valor que no se ajusta.
	struct EmitterTuning
	{
		RE::NiPointer<RE::NiFloatInterpolator> birthRate;  // interpolador "BirthRate" del NiPSysEmitterCtlr, sin claves
		float                                  originalBirthRate{ 0.0f };
		RE::NiPointer<RE::NiPSysModifier>      emitter;    // emisor de volumen con la disposición de campos comprobada
		float                                  originalLifeSpan{ 0.0f };
		float                                  originalLifeSpanVariation{ 0.0f };
		std::uint16_t                          maxParticles{ 0 };  // tope de partículas vivas del NiPSysData
	};

	// true si a_controller es el NiPSysEmitterCtlr que hace nacer las partículas de su sistema.
	[[nodiscard]] bool IsEmitterController(const RE::NiTimeController* a_controller);

	// Localiza el ritmo de nacimiento y la vida de cada sistema bajo a_root (instancia de a_nifPath, con el 3D cargado).
	// Los valores del NIF son los de la primera instancia de la sesión; lo no ajustable se avisa en el log y queda nulo.
	[[nodiscard]] std::vector<EmitterTuning> CaptureEmitters(RE::NiAVObject* a_root, std::string_view a_nifPath);

	// Escribe valor del NIF × multiplicador; el ritmo se limita para no superar maxParticles vivas.
	// La vida solo cambia en las partículas que nazcan después. Lo llama el bucle por fotograma del efecto.
	void ApplyMultipliers(const std::vector<EmitterTuning>& a_emitters, float a_birthRateMult, float a_lifeSpanMult);
}
