// Implementación de la reproducción de sonidos sueltos por archivo.
// Ver SoundResolver.h para el porqué de cada decisión.

#include "12.- AUDIO/SoundResolver.h"

#include "1.- CORE/Constants.h"

namespace Audio
{
	void PlayFileOneShot(const RE::NiPoint3& a_position, const char* a_filePath, float a_volume)
	{
		auto* audioManager = RE::BSAudioManager::GetSingleton();
		if (!audioManager) {
			return;
		}

		RE::BSResource::ID fileID;
		fileID.GenerateFromPath(a_filePath);

		RE::BSSoundHandle handle;
		audioManager->GetSoundHandleByFile(handle, fileID, Constants::kSoundHandleFlags, Constants::kFileSoundPriority);
		handle.SetPosition(a_position);
		handle.SetVolume(a_volume);
		const bool played = handle.FadeInPlay(0);
		logs::info("Audio::PlayFileOneShot: \"{}\" -- FadeInPlay()={}.", a_filePath, played);
	}

	void WarmUpAll()
	{
		PlayFileOneShot(RE::NiPoint3{}, Constants::kThrowLaunchSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCatchStartSoundFilePath, 0.0f);

		// El chasquido de Llamada y "catch end" necesitan dos usos reales
		// antes de estabilizarse, no uno como los otros 2 (comprobado en el
		// juego para "catch end" con el mecanismo antiguo por Sound
		// Descriptor, 2026-09-23, ver CHANGELOG.md v1.19.24; el chasquido se
		// suma aquí como prueba tras fallar con un único calentamiento en
		// v1.19.27, sin confirmar todavía si dos bastan). Sin explicación
		// firme de por qué estos dos en concreto lo necesitan y los otros 2
		// no.
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCallReleaseSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCallReleaseSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCatchEndSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCatchEndSoundFilePath, 0.0f);

		logs::info("Audio::WarmUpAll: gastado el primer intento de los 4 sonidos del arma (dos veces para chasquido y catch end).");
	}
}
