// Implementación de los dos sonidos de atrape (arranque + golpe final).
// Ver CatchSound.h para el porqué de cada decisión.

#include "12.- AUDIO/CatchSound.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"
#include "RE/M/Misc.h"

// mmsystem.h (arrastrado por Windows.h) define PlaySound como macro y
// reescribe RE::PlaySound a RE::PlaySoundA -- mismo problema que min/max,
// documentado en CLAUDE.md. Solo hace falta el guard en el punto de uso.
#ifdef PlaySound
#undef PlaySound
#endif

namespace Audio
{
	namespace
	{
		// Reproducción fiable de un sonido suelto (ver
		// Constants::kFlightSoundHandleFlags para el porqué de cada paso,
		// confirmado en el juego): un RE::BSSoundHandle de cebado sin
		// posición dejado sonar por su cuenta, RE::PlaySound(a_editorID)
		// en el mismo instante, y el RE::BSSoundHandle real posicionado y
		// arrancado con FadeInPlay(0). Ninguno de los tres handles llama a
		// Stop() -- RE::BSSoundHandle no se autolibera (~BSSoundHandle() =
		// default), y el motor sigue reproduciéndolos por su cuenta aunque
		// el handle local se destruya al salir de esta función.
		void PlayReliableOneShot(const RE::NiPoint3& a_position, RE::FormID a_localFormID, const char* a_editorID)
		{
			auto* audioManager = RE::BSAudioManager::GetSingleton();
			auto* descriptor = audioManager ? ResolveSoundDescriptor(a_localFormID) : nullptr;
			if (!descriptor) {
				logs::warn("Audio::PlayReliableOneShot: no se pudo resolver el Sound Descriptor (FormID local 0x{:06X}).", a_localFormID);
				return;
			}

			RE::BSSoundHandle primingHandle;
			if (audioManager->GetSoundHandle(primingHandle, descriptor, Constants::kFlightSoundHandleFlags)) {
				primingHandle.FadeInPlay(0);
			}

			RE::PlaySound(a_editorID);

			RE::BSSoundHandle handle;
			if (audioManager->GetSoundHandle(handle, descriptor, Constants::kFlightSoundHandleFlags)) {
				handle.SetPosition(a_position);
				handle.SetVolume(Constants::kSoundHandleVolume);
				const bool played = handle.FadeInPlay(0);
				logs::info("Audio::PlayReliableOneShot: FormID local 0x{:06X} -- FadeInPlay()={}.", a_localFormID, played);
			}
		}
	}

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
