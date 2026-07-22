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
		if (primingHandle.IsValid()) {
			primingHandle.Stop();
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

			// Cebado empírico (ver CatchSound.h): segundo handle, sin
			// posición, mantenido vivo -- no se llama Stop() aquí, solo en
			// el destructor.
			if (audioManager->GetSoundHandle(primingHandle, descriptor, Constants::kFlightSoundHandleFlags)) {
				primingHandle.FadeInPlay(0);
			}

			// Confirmado en el juego como necesario (no solo diagnóstico):
			// sin esta llamada -- aun con primingHandle -- "handle" se
			// queda mudo. No hay explicación firme de por qué (a_flags y
			// el pool de voces de BSAudioManager no están documentados en
			// commonlibsse-ng); tratarlo como una dependencia real hasta
			// que se entienda mejor, no como código de prueba a retirar.
			RE::PlaySound("CAP_ThorMjolnir_Sound_MjolnirCatch");

			const bool gotHandle = audioManager->GetSoundHandle(handle, descriptor, Constants::kFlightSoundHandleFlags);
			logs::info("Audio::CatchSound::Update: GetSoundHandle={}, IsValid={}.", gotHandle, handle.IsValid());
			if (!gotHandle || !handle.IsValid()) {
				return;
			}

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

		// SetPosition/SetFrequency antes de FadeInPlay() (nunca después):
		// el motor puede fijar la posición/atenuación en el instante de
		// arranque -- arrancarlo sin posicionar antes lo deja sonando
		// desde donde sea que un BSSoundHandle recién creado empiece por
		// defecto, inaudible a cualquier distancia real del jugador aunque
		// la propia llamada devuelva éxito. Mismo orden que ya usa
		// correctamente Audio::FlightSound::Start (SetObjectToFollow antes
		// de Play()).
		handle.SetFrequency(frequency);
		handle.SetPosition(a_position);
		consumedClipTime += frequency * a_deltaSeconds;

		if (justStarted) {
			// Volumen explícito, también antes de FadeInPlay() -- un
			// handle recién obtenido no tiene garantizado arrancar a
			// volumen audible por defecto (ver Constants::kSoundHandleVolume).
			// FadeInPlay(0) en vez de Play(): con Play() el handle nunca
			// llegaba a sonar en el juego (comprobado repetidamente,
			// IsPlaying()/GetDuration() reportaban reproducción activa sin
			// nada audible); FadeInPlay(0) -- fundido de entrada de 0ms,
			// función distinta con su propio RELOCATION_ID -- sí lo hizo
			// sonar en las pruebas.
			const bool volumeSet = handle.SetVolume(Constants::kSoundHandleVolume);
			const bool played = handle.FadeInPlay(0);
			logs::info("Audio::CatchSound::Update: SetVolume()={}, FadeInPlay()={}, IsPlaying()={}, GetDuration()={}.",
				volumeSet, played, handle.IsPlaying(), handle.GetDuration());
		}
	}
}
