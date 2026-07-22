// Implementación de la resolución compartida de Sound Descriptor.
// Ver SoundResolver.h para el porqué de cada decisión.

#include "12.- AUDIO/SoundResolver.h"

#include "1.- CORE/Constants.h"

namespace Audio
{
	namespace
	{
		// Un único intento de resolución (Sound Descriptor directo, o
		// Sound Marker usando su campo "Sound") sobre un FormID local
		// concreto -- separado para poder probar más de un candidato de
		// FormID sin duplicar esta lógica (ver ResolveSoundDescriptor).
		RE::BGSSoundDescriptorForm* TryResolve(RE::TESDataHandler& a_dataHandler, RE::FormID a_localFormID)
		{
			if (auto* descriptor = a_dataHandler.LookupForm<RE::BGSSoundDescriptorForm>(a_localFormID, Constants::kSoundPluginName)) {
				return descriptor;
			}

			if (auto* markSound = a_dataHandler.LookupForm<RE::TESSound>(a_localFormID, Constants::kSoundPluginName)) {
				if (!markSound->descriptor) {
					logs::warn("Audio::ResolveSoundDescriptor: el Sound Marker 0x{:06X} de \"{}\" no tiene ningún Sound Descriptor asignado en su campo \"Sound\".", a_localFormID, Constants::kSoundPluginName);
					return nullptr;
				}
				return markSound->descriptor;
			}

			return nullptr;
		}
	}

	RE::BGSSoundDescriptorForm* ResolveSoundDescriptor(RE::FormID a_localFormID)
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return nullptr;
		}

		if (auto* descriptor = TryResolve(*dataHandler, a_localFormID)) {
			return descriptor;
		}

		// Constants.h guarda el FormID local "en bruto" del registro (el
		// que se ve en la Creation Kit, hasta 24 bits) -- pero este plugin
		// tiene el flag ESL activo (comprobado leyendo el header del propio
		// .esp), y el direccionamiento comprimido de un plugin ligero solo
		// usa 12 bits para la parte local. Si el FormID en bruto no
		// resuelve y excede esos 12 bits, se prueba también enmascarado a
		// ellos -- por si el valor que de verdad usa el motor en tiempo de
		// ejecución es ese subconjunto, no el bruto.
		if (a_localFormID > 0x00000FFF) {
			const RE::FormID masked = a_localFormID & 0x00000FFF;
			if (auto* descriptor = TryResolve(*dataHandler, masked)) {
				logs::info("Audio::ResolveSoundDescriptor: FormID local 0x{:06X} no resolvió, pero su versión de 12 bits (plugin ESL) 0x{:03X} sí -- usar ese valor directamente en Constants.h.", a_localFormID, masked);
				return descriptor;
			}
		}

		logs::warn("Audio::ResolveSoundDescriptor: no se encontró ningún Sound Descriptor ni Sound Marker con FormID local 0x{:06X} (ni su variante de 12 bits) en \"{}\".", a_localFormID, Constants::kSoundPluginName);
		return nullptr;
	}

	void PrecacheAll()
	{
		auto* audioManager = RE::BSAudioManager::GetSingleton();
		if (!audioManager) {
			return;
		}

		for (const auto localFormID : { Constants::kThrowLaunchSoundLocalFormID, Constants::kFlightLoopSoundLocalFormID, Constants::kCatchStartSoundLocalFormID, Constants::kCatchEndSoundLocalFormID }) {
			if (auto* descriptor = ResolveSoundDescriptor(localFormID)) {
				audioManager->PrecacheDescriptor(descriptor, 0);
				logs::info("Audio::PrecacheAll: precacheado FormID local 0x{:06X}.", localFormID);
			}
		}
	}
}
