// Implementación del sonido de atrape sincronizado en tiempo real.
// Ver CatchSound.h para el porqué de cada decisión.

#include "12.- AUDIO/CatchSound.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"
#include "RE/M/Misc.h"

#include <algorithm>

// mmsystem.h (arrastrado por Windows.h) define PlaySound como macro y
// reescribe RE::PlaySound a RE::PlaySoundA -- mismo problema que min/max,
// documentado en CLAUDE.md. Solo hace falta el guard en el punto de uso.
#ifdef PlaySound
#undef PlaySound
#endif

namespace Audio
{
	CatchSound::~CatchSound()
	{
		if (handle.IsValid()) {
			handle.Stop();
		}
		if (diagnosticHandle.IsValid()) {
			diagnosticHandle.Stop();
		}
	}

	void CatchSound::Update(const RE::NiPoint3& a_position, float a_currentDistance, float a_deltaSeconds)
	{
		if (previousDistance < 0.0f) {
			// Primera llamada: sin muestra anterior no hay forma de medir
			// una velocidad de cierre todavía.
			previousDistance = a_currentDistance;
			return;
		}

		const float closingSpeed = (previousDistance - a_currentDistance) / a_deltaSeconds;
		previousDistance = a_currentDistance;

		if (closingSpeed <= 0.0f) {
			// Parado o alejándose: sin estimación fiable de tiempo
			// restante todavía -- se reintenta el siguiente tick con
			// datos más recientes, en vez de asumir algo.
			return;
		}

		const float estimatedRemaining = a_currentDistance / closingSpeed;

		bool justStarted = false;
		if (!started) {
			if (estimatedRemaining > Constants::kCatchImpactSoundLeadTime) {
				return;
			}

			logs::info("Audio::CatchSound::Update: tiempo restante estimado {:.3f}s <= preámbulo {:.3f}s, intentando arrancar.", estimatedRemaining, Constants::kCatchImpactSoundLeadTime);

			auto* audioManager = RE::BSAudioManager::GetSingleton();
			auto* descriptor = audioManager ? ResolveSoundDescriptor(Constants::kCatchImpactSoundLocalFormID) : nullptr;
			if (!descriptor) {
				logs::warn("Audio::CatchSound::Update: no se pudo resolver el Sound Descriptor (FormID local 0x{:06X}).", Constants::kCatchImpactSoundLocalFormID);
				return;
			}

			const bool gotHandle = audioManager->GetSoundHandle(handle, descriptor, Constants::kFlightSoundHandleFlags);
			logs::info("Audio::CatchSound::Update: GetSoundHandle={}, IsValid={}.", gotHandle, handle.IsValid());
			if (!gotHandle || !handle.IsValid()) {
				return;
			}

			// Diagnóstico temporal: handle gemelo, aparte del que usa el
			// resto de la clase, sin SetPosition/SetVolume y arrancado con
			// FadeInPlay(0) en vez de Play() -- función distinta (otro
			// RELOCATION_ID, ver BSSoundHandle.cpp), para descartar que el
			// problema esté en algo específico del código interno de
			// Play() en concreto. Mantenido vivo (no se detiene aquí) para
			// poder seguir su IsPlaying()/GetDuration() tick a tick (ver
			// más abajo).
			if (audioManager->GetSoundHandle(diagnosticHandle, descriptor, Constants::kFlightSoundHandleFlags)) {
				const bool diagnosticPlayed = diagnosticHandle.FadeInPlay(0);
				logs::info("Audio::CatchSound::Update: [diagnóstico sin posición, FadeInPlay(0)] FadeInPlay()={}, IsPlaying()={}, GetDuration()={}.",
					diagnosticPlayed, diagnosticHandle.IsPlaying(), diagnosticHandle.GetDuration());
			}

			// Diagnóstico temporal: control en vivo -- RE::PlaySound es el
			// único camino que llegó a sonar en esta sesión (subsistema
			// distinto de BSAudioManager). Si hoy, con la configuración
			// actual del Sound Descriptor en CK, esto tampoco suena,
			// confirma que algo cambió en el propio descriptor (categoría/
			// output model) y no que BSAudioManager esté roto. Si esto SÍ
			// suena mientras handle/diagnosticHandle (arriba) reportan
			// IsPlaying()=true sin sonido, aísla el problema al output
			// model "SOMMono03000_verb" (sufijo _verb -> bus de reverb,
			// posiblemente sin contexto de Acoustic Space activo aquí).
			RE::PlaySound("CAP_ThorMjolnir_Sound_MjolnirCatch");
			logs::info("Audio::CatchSound::Update: [diagnóstico] RE::PlaySound(\"CAP_ThorMjolnir_Sound_MjolnirCatch\") lanzado.");

			started = true;
			justStarted = true;
			consumedClipTime = 0.0f;
		}

		// Cuánto preámbulo (en tiempo de clip) le queda al audio antes de
		// llegar al golpe grabado, frente a cuánto tiempo real queda hasta
		// el atrape -- la razón entre ambos es la velocidad de
		// reproducción que hace falta para que coincidan. Se recalcula
		// cada tick con la estimación más reciente, así que se reajusta
		// solo si el tiempo restante real cambia (el jugador se mueve).
		const float remainingClipTime = Constants::kCatchImpactSoundLeadTime - consumedClipTime;
		float       frequency = remainingClipTime > 0.0f ? remainingClipTime / estimatedRemaining : 1.0f;
		frequency = std::clamp(frequency, Constants::kCatchSoundMinFrequency, Constants::kCatchSoundMaxFrequency);

		// SetPosition/SetFrequency antes de Play() (nunca después): el
		// motor puede fijar la posición/atenuación en el instante de
		// Play() -- arrancarlo sin posicionar antes lo deja sonando desde
		// donde sea que un BSSoundHandle recién creado empiece por
		// defecto (probablemente el origen del mundo), inaudible a
		// cualquier distancia real del jugador aunque la propia llamada a
		// Play() devuelva éxito. Mismo orden que ya usa correctamente
		// Audio::FlightSound::Start (SetObjectToFollow antes de Play()).
		handle.SetFrequency(frequency);
		handle.SetPosition(a_position);
		consumedClipTime += frequency * a_deltaSeconds;

		if (justStarted) {
			// Volumen explícito, también antes de Play()/FadeInPlay() -- un
			// handle recién obtenido no tiene garantizado arrancar a
			// volumen audible por defecto (ver Constants::kSoundHandleVolume).
			// FadeInPlay(0) en vez de Play(), mismo motivo que en el handle
			// de diagnóstico de arriba -- función distinta, para descartar
			// que el problema esté en Play() en concreto.
			const bool volumeSet = handle.SetVolume(Constants::kSoundHandleVolume);
			const bool played = handle.FadeInPlay(0);
			logs::info("Audio::CatchSound::Update: SetVolume()={}, FadeInPlay()={}, IsPlaying()={}, GetDuration()={}.",
				volumeSet, played, handle.IsPlaying(), handle.GetDuration());
		} else {
			// Diagnóstico temporal: seguimiento tick a tick tras el
			// arranque -- si IsPlaying()/GetDuration() siguen en
			// false/0 varios ticks después (no solo en el instante de
			// Play()), descarta la hipótesis de "carga asíncrona todavía
			// no completada" y confirma que BSAudioManager nunca llega a
			// reproducir de verdad este descriptor.
			logs::info("Audio::CatchSound::Update: [seguimiento] handle IsPlaying()={}, GetDuration()={} -- diagnosticHandle IsPlaying()={}, GetDuration()={}.",
				handle.IsPlaying(), handle.GetDuration(), diagnosticHandle.IsPlaying(), diagnosticHandle.GetDuration());
		}
	}
}
