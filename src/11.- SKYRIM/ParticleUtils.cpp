// Ajuste de sistemas de partículas -- ver ParticleUtils.h.

#include "11.- SKYRIM/ParticleUtils.h"

#include "1.- CORE/Constants.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <format>
#include <unordered_map>

namespace ParticleUtils
{
	namespace
	{
		// Campos de NiPSysEmitter y NiPSysVolumeEmitter tras la base NiPSysModifier; CommonLibSSE-NG no los declara.
		// De 0x34 a 0x68, orden de nif.xml leído en memoria; FindVolumeEmitter comprueba emitterObject antes de usarlos.
		struct VolumeEmitterFields
		{
			float           unk30;                 // 30 (fuera del NIF; cambia en el juego)
			float           speed;                 // 34
			float           speedVariation;        // 38
			float           declination;           // 3C
			float           declinationVariation;  // 40
			float           planarAngle;           // 44
			float           planarAngleVariation;  // 48
			RE::NiColorA    initialColor;          // 4C
			float           initialRadius;         // 5C
			float           radiusVariation;       // 60
			float           lifeSpan;              // 64
			float           lifeSpanVariation;     // 68
			float           unk6C;                 // 6C (fuera del NIF; 1.0 en el juego)
			RE::NiAVObject* emitterObject;         // 70 (NiPSysVolumeEmitter)
		};
		static_assert(sizeof(RE::NiPSysModifier) == 0x30);
		static_assert(offsetof(VolumeEmitterFields, lifeSpan) == 0x64 - sizeof(RE::NiPSysModifier));
		static_assert(offsetof(VolumeEmitterFields, emitterObject) == 0x70 - sizeof(RE::NiPSysModifier));

		// Palabras de 4 bytes que se vuelcan al log si la disposición no cuadra (de 0x30 a 0x7C).
		constexpr std::size_t kDumpWordCount = 20;

		// Valores del NIF por "ruta|sistema|campo", guardados la primera vez que se ven en la sesión: las instancias
		// de un NIF comparten el interpolador del ritmo de nacimiento, así que una nueva ya trae lo escrito en otra.
		std::unordered_map<std::string, float> g_originals;

		float RememberOriginal(std::string_view a_nifPath, std::string_view a_systemName, std::string_view a_field, float a_current)
		{
			return g_originals.try_emplace(std::format("{}|{}|{}", a_nifPath, a_systemName, a_field), a_current).first->second;
		}

		VolumeEmitterFields& GetVolumeEmitterFields(RE::NiPSysModifier* a_emitter)
		{
			return *reinterpret_cast<VolumeEmitterFields*>(reinterpret_cast<std::byte*>(a_emitter) + sizeof(RE::NiPSysModifier));
		}

		// true si a_object es de la clase con NiRTTI a_rtti o deriva de ella.
		bool IsKindOf(const RE::NiObject* a_object, const RE::NiRTTI* a_rtti)
		{
			const auto* objectRTTI = a_object ? a_object->GetRTTI() : nullptr;
			return objectRTTI && objectRTTI->IsKindOf(a_rtti);
		}

		// true si a_candidate es un objeto del árbol de a_root; compara la dirección sin leer lo que apunta.
		bool ContainsObject(RE::NiAVObject* a_root, const void* a_candidate)
		{
			bool found = false;
			RE::BSVisit::TraverseScenegraphObjects(a_root, [&](RE::NiAVObject* a_object) {
				found = a_object == a_candidate;
				return found ? RE::BSVisit::BSVisitControl::kStop : RE::BSVisit::BSVisitControl::kContinue;
			});
			return found;
		}

		// Campos de a_emitter tras NiPSysModifier en hexadecimal y como float, para rehacer la disposición.
		std::string DumpEmitterWords(RE::NiPSysModifier* a_emitter)
		{
			const auto* bytes = reinterpret_cast<const std::byte*>(a_emitter) + sizeof(RE::NiPSysModifier);

			std::string dump;
			for (std::size_t i = 0; i < kDumpWordCount; ++i) {
				std::uint32_t word = 0;
				float         asFloat = 0.0f;
				std::memcpy(&word, bytes + i * 4, 4);
				std::memcpy(&asFloat, bytes + i * 4, 4);
				dump += std::format(" {:X}={:08X}({:g})", sizeof(RE::NiPSysModifier) + i * 4, word, asFloat);
			}
			return dump;
		}

		// Interpolador "BirthRate" del NiPSysEmitterCtlr de a_system si es un valor fijo; si no, avisa y nullptr.
		RE::NiFloatInterpolator* FindBirthRate(RE::NiParticleSystem& a_system, std::string_view a_systemName)
		{
			for (auto* controller = a_system.GetControllers(); controller; controller = controller->GetNext()) {
				auto* interpController = IsEmitterController(controller) ? netimmerse_cast<RE::NiInterpController*>(controller) : nullptr;
				if (!interpController) {
					continue;
				}

				for (std::uint16_t i = 0; i < interpController->GetInterpolatorCount(); ++i) {
					const char* id = interpController->GetInterpolatorID(i);
					if (!id || id != Constants::kBirthRateInterpolatorID) {
						continue;
					}

					auto* interpolator = netimmerse_cast<RE::NiFloatInterpolator*>(interpController->GetInterpolator(i));
					if (!interpolator || interpolator->floatData) {
						logs::warn("ParticleUtils: el ritmo de nacimiento de '{}' no es un valor fijo (NiFloatInterpolator sin claves), no se ajusta.", a_systemName);
						return nullptr;
					}
					return interpolator;
				}
			}

			logs::warn("ParticleUtils: '{}' no tiene interpolador \"{}\" en un NiPSysEmitterCtlr, no se ajusta el ritmo de nacimiento.", a_systemName, Constants::kBirthRateInterpolatorID);
			return nullptr;
		}

		// Primer emisor de volumen de a_system cuya disposición cuadra: su objeto emisor es un nodo de a_root y su
		// vida es positiva. Si ninguno cuadra, avisa con los campos leídos y nullptr.
		RE::NiPSysModifier* FindVolumeEmitter(RE::NiParticleSystem& a_system, RE::NiAVObject* a_root, std::string_view a_systemName)
		{
			static REL::Relocation<const RE::NiRTTI*> volumeEmitterRTTI{ RE::NiRTTI_NiPSysVolumeEmitter };

			for (const auto& modifier : a_system.GetParticleSystemRuntimeData().modifierList) {
				if (!IsKindOf(modifier.get(), volumeEmitterRTTI.get())) {
					continue;
				}

				const auto& fields = GetVolumeEmitterFields(modifier.get());
				const bool  valid = ContainsObject(a_root, fields.emitterObject) &&
				                   std::isfinite(fields.lifeSpan) && fields.lifeSpan > 0.0f &&
				                   std::isfinite(fields.lifeSpanVariation) && fields.lifeSpanVariation >= 0.0f;
				if (valid) {
					return modifier.get();
				}

				logs::warn("ParticleUtils: el emisor '{}' de '{}' no cuadra con la disposición esperada, no se ajusta la vida. Campos:{}",
					modifier->name.c_str(), a_systemName, DumpEmitterWords(modifier.get()));
			}

			logs::warn("ParticleUtils: '{}' sin emisor de volumen ajustable, no se ajusta la vida de sus partículas.", a_systemName);
			return nullptr;
		}
	}

	bool IsEmitterController(const RE::NiTimeController* a_controller)
	{
		// Nombre exacto de la clase: solo el emisor, nunca el NiPSysUpdateCtlr de la misma cadena.
		const auto* rtti = a_controller ? a_controller->GetRTTI() : nullptr;
		return rtti && rtti->GetName() && rtti->GetName() == Constants::kEmitterControllerRTTIName;
	}

	std::vector<EmitterTuning> CaptureEmitters(RE::NiAVObject* a_root, std::string_view a_nifPath)
	{
		std::vector<EmitterTuning> emitters;
		if (!a_root) {
			return emitters;
		}

		RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
			auto* system = netimmerse_cast<RE::NiParticleSystem*>(a_geometry);
			if (!system) {
				return RE::BSVisit::BSVisitControl::kContinue;
			}

			const std::string_view systemName{ system->name.c_str() };
			EmitterTuning          tuning;

			// Cada valor sale "del NIF" y "en la instancia": si difieren, la instancia comparte el objeto con otra.
			std::string birthRateText{ "no ajustable" };
			if (auto* birthRate = FindBirthRate(*system, systemName)) {
				tuning.birthRate.reset(birthRate);
				tuning.originalBirthRate = RememberOriginal(a_nifPath, systemName, "BirthRate", birthRate->floatValue);
				birthRateText = std::format("{:.1f}/s del NIF ({:.1f} en la instancia)", tuning.originalBirthRate, birthRate->floatValue);
			}

			std::string emitterText{ "no ajustable" };
			if (auto* emitter = FindVolumeEmitter(*system, a_root, systemName)) {
				const auto& fields = GetVolumeEmitterFields(emitter);
				tuning.emitter.reset(emitter);
				tuning.originalLifeSpan = RememberOriginal(a_nifPath, systemName, "LifeSpan", fields.lifeSpan);
				tuning.originalLifeSpanVariation = RememberOriginal(a_nifPath, systemName, "LifeSpanVariation", fields.lifeSpanVariation);
				emitterText = std::format("'{}': Life Span {:.2f} ± {:.2f} s del NIF ({:.2f} ± {:.2f} en la instancia), Speed {:.2f} ± {:.2f}, Initial Radius {:.2f} ± {:.2f}",
					emitter->name.c_str(), tuning.originalLifeSpan, tuning.originalLifeSpanVariation, fields.lifeSpan, fields.lifeSpanVariation,
					fields.speed, fields.speedVariation, fields.initialRadius, fields.radiusVariation);
			}

			if (const auto& data = system->GetParticlesRuntimeData().particleData) {
				tuning.maxParticles = data->GetParticlesRuntimeData().maxNumVertices;
			}

			// Speed e Initial Radius solo se leen aquí, para contrastar la disposición con los valores del NIF.
			logs::info("ParticleUtils: '{}' -- ritmo de nacimiento {} | emisor {} | tope {} partículas.", systemName, birthRateText, emitterText, tuning.maxParticles);

			emitters.push_back(std::move(tuning));
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return emitters;
	}

	void ApplyMultipliers(const std::vector<EmitterTuning>& a_emitters, float a_birthRateMult, float a_lifeSpanMult)
	{
		for (const auto& tuning : a_emitters) {
			if (tuning.emitter) {
				auto& fields = GetVolumeEmitterFields(tuning.emitter.get());
				fields.lifeSpan = tuning.originalLifeSpan * a_lifeSpanMult;
				fields.lifeSpanVariation = tuning.originalLifeSpanVariation * a_lifeSpanMult;
			}

			if (!tuning.birthRate) {
				continue;
			}

			// Con más partículas vivas que el tope, el emisor deja de crear hasta que mueren: salen a ráfagas.
			float       birthRate = tuning.originalBirthRate * a_birthRateMult;
			const float maxLifeSpan = (tuning.originalLifeSpan + tuning.originalLifeSpanVariation) * a_lifeSpanMult;
			if (tuning.emitter && tuning.maxParticles > 0 && maxLifeSpan > 0.0f) {
				birthRate = (std::min)(birthRate, static_cast<float>(tuning.maxParticles) / maxLifeSpan);
			}
			tuning.birthRate->floatValue = birthRate;
		}
	}
}
