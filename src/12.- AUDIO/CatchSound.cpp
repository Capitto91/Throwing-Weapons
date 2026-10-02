// Sonidos de atrape -- ver CatchSound.h.

#include "12.- AUDIO/CatchSound.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"

namespace Audio
{
	void CatchCue::UpdateStart(const RE::NiPoint3& a_position, float a_secondsToArrival)
	{
		if (startFired || a_secondsToArrival > Constants::kCatchStartSoundLeadTime) {
			return;
		}

		startFired = true;
		PlayFileOneShot(a_position, Constants::kCatchStartSoundFilePath, Constants::kSoundHandleVolume);
	}

	void CatchCue::PlayEnd(const RE::NiPoint3& a_position)
	{
		PlayFileOneShot(a_position, Constants::kCatchEndSoundFilePath, Constants::kSoundHandleVolume);
	}
}
