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
		// Volumen explícito antes de Play() -- ver Constants::kSoundHandleVolume.
		const bool volumeSet = handle.SetVolume(Constants::kSoundHandleVolume);
		const bool played = handle.Play();
		logs::info("Audio::FlightSound::Start: SetVolume()={}, Play()={}.", volumeSet, played);
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
		// Volumen explícito antes de Play() -- ver Constants::kSoundHandleVolume.
		const bool volumeSet = handle.SetVolume(Constants::kSoundHandleVolume);
		const bool played = handle.Play();
		logs::info("Audio::PlaySoundOneShot: SetVolume()={}, Play()={}.", volumeSet, played);
	}
}
