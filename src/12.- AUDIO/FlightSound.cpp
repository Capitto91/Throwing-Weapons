// Implementación del sonido de vuelo/lanzamiento.
// Ver FlightSound.h y SoundResolver.h para el porqué de cada decisión.

#include "12.- AUDIO/FlightSound.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"

namespace Audio
{
	FlightSound::~FlightSound()
	{
		if (handle.IsValid()) {
			handle.Stop();
		}
	}

	void FlightSound::Start(RE::NiAVObject* a_node, RE::FormID a_localFormID)
	{
		if (!a_node) {
			return;
		}

		auto* audioManager = RE::BSAudioManager::GetSingleton();
		auto* descriptor = audioManager ? ResolveSoundDescriptor(a_localFormID) : nullptr;
		if (!descriptor) {
			return;
		}

		const bool gotHandle = audioManager->GetSoundHandle(handle, descriptor, Constants::kFlightSoundHandleFlags);
		logs::info("Audio::FlightSound::Start: FormID local 0x{:06X} -- GetSoundHandle={}, IsValid={}.", a_localFormID, gotHandle, handle.IsValid());
		if (!gotHandle || !handle.IsValid()) {
			return;
		}

		handle.SetObjectToFollow(a_node);
		// Volumen explícito antes de FadeInPlay() -- ver
		// Constants::kSoundHandleVolume. FadeInPlay(0) en vez de Play():
		// con Play() el BSSoundHandle nunca llegaba a sonar en el juego
		// pese a reportar éxito (ver Audio::CatchSound::Update, mismo
		// problema comprobado repetidamente ahí) -- FadeInPlay(0) sí lo
		// hizo sonar.
		const bool volumeSet = handle.SetVolume(Constants::kSoundHandleVolume);
		const bool played = handle.FadeInPlay(0);
		logs::info("Audio::FlightSound::Start: SetVolume()={}, FadeInPlay()={}.", volumeSet, played);
	}

	void PlaySoundOneShot(const RE::NiPoint3& a_position, RE::FormID a_localFormID)
	{
		auto* audioManager = RE::BSAudioManager::GetSingleton();
		auto* descriptor = audioManager ? ResolveSoundDescriptor(a_localFormID) : nullptr;
		if (!descriptor) {
			return;
		}

		RE::BSSoundHandle handle;
		const bool         gotHandle = audioManager->GetSoundHandle(handle, descriptor, Constants::kFlightSoundHandleFlags);
		logs::info("Audio::PlaySoundOneShot: FormID local 0x{:06X} -- GetSoundHandle={}, IsValid={}.", a_localFormID, gotHandle, handle.IsValid());
		if (!gotHandle || !handle.IsValid()) {
			return;
		}

		handle.SetPosition(a_position);
		// Volumen explícito antes de FadeInPlay() -- ver
		// Constants::kSoundHandleVolume. FadeInPlay(0) en vez de Play(),
		// mismo motivo que en FlightSound::Start más arriba.
		const bool volumeSet = handle.SetVolume(Constants::kSoundHandleVolume);
		const bool played = handle.FadeInPlay(0);
		logs::info("Audio::PlaySoundOneShot: SetVolume()={}, FadeInPlay()={}.", volumeSet, played);
	}
}
