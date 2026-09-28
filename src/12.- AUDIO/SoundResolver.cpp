// Sonidos por archivo -- ver SoundResolver.h.

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
		handle.FadeInPlay(0);
	}

	void WarmUpAll()
	{
		PlayFileOneShot(RE::NiPoint3{}, Constants::kThrowLaunchSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCatchStartSoundFilePath, 0.0f);

		// El chasquido de Llamada y "catch end" necesitan dos usos.
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCallReleaseSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCallReleaseSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCatchEndSoundFilePath, 0.0f);
		PlayFileOneShot(RE::NiPoint3{}, Constants::kCatchEndSoundFilePath, 0.0f);

	}
}
