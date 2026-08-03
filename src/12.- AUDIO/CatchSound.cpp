// Implementación de los dos sonidos de atrape (arranque + golpe final).
// Ver CatchSound.h para el porqué de cada decisión.

#include "12.- AUDIO/CatchSound.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"

namespace Audio
{
	void CatchCue::UpdateStart(const RE::NiPoint3& a_position, float a_deltaSeconds)
	{
		if (startFired) {
			return;
		}

		elapsed += a_deltaSeconds;
		if (elapsed < startDelay) {
			return;
		}

		startFired = true;
		PlayReliableOneShot(a_position, Constants::kCatchStartSoundLocalFormID, Constants::kCatchStartSoundEditorID);
	}

	void CatchCue::PlayEnd(const RE::NiPoint3& a_position)
	{
		PlayReliableOneShot(a_position, Constants::kCatchEndSoundLocalFormID, Constants::kCatchEndSoundEditorID);
	}
}
