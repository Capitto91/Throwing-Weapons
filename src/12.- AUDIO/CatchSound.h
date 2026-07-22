// Sonido de atrape, sincronizado en tiempo real con la llegada real del
// arma a la mano -- ver Constants.h ("Sonido de lanzamiento/vuelo/atrape")
// y el plan de rediseño para el porqué de cada decisión. No hay ninguna
// mecánica del documento de diseño que cubra esto, es pulido audiovisual
// puro.

#pragma once

namespace Audio
{
	// A diferencia de PlaySoundOneShot (dispara y olvida), este sonido se
	// arranca por adelantado y se reajusta cada tick mientras suena, para
	// que el golpe grabado en el propio audio
	// (Constants::kCatchImpactSoundLeadTime segundos dentro del clip)
	// caiga siempre justo en el instante real de la llegada -- sea cual
	// sea la duración real del regreso (vuelos muy cortos) o si el
	// jugador se mueve durante el trayecto (cambia el tiempo restante real
	// respecto a cualquier predicción hecha al principio). En vez de
	// predecir una vez, mide la velocidad de cierre real tick a tick a
	// partir de la misma distancia arma-mano que ya decide la llegada
	// (ver Return::BeginReturnMovement) y ajusta la velocidad de
	// reproducción (RE::BSSoundHandle::SetFrequency) en consecuencia.
	//
	// RAII: RE::BSSoundHandle no se autolibera (~BSSoundHandle() =
	// default) -- mismo motivo que Audio::FlightSound.
	class CatchSound
	{
	public:
		CatchSound() = default;
		~CatchSound();

		// Solo movible, mismo motivo que Audio::FlightSound: un único
		// BSSoundHandle en vuelo por instancia.
		CatchSound(const CatchSound&) = delete;
		CatchSound& operator=(const CatchSound&) = delete;
		CatchSound(CatchSound&&) = default;
		CatchSound& operator=(CatchSound&&) = default;

		// Debe llamarse cada tick del regreso, con a_position = la
		// posición actual de la réplica y a_currentDistance = distancia
		// actual hasta la mano (la misma que ya usa
		// Return::BeginReturnMovement para decidir la llegada). No hace
		// nada visible hasta que el tiempo restante estimado cae por
		// debajo de Constants::kCatchImpactSoundLeadTime; a partir de ahí
		// arranca el sonido y sigue ajustando su velocidad de
		// reproducción y posición cada tick hasta que deja de llamarse.
		void Update(const RE::NiPoint3& a_position, float a_currentDistance, float a_deltaSeconds);

		// true si el sonido ya ha arrancado (ver Update) -- Return::BeginReturnMovement
		// lo usa como red de seguridad: si nunca llegó a arrancar (caso
		// degenerado, velocidad de cierre nunca positiva) y la réplica ya
		// llegó a la mano, dispara un sonido suelto sin sincronizar en vez
		// de dejar el atrape mudo.
		[[nodiscard]] bool HasStarted() const noexcept { return started; }

	private:
		RE::BSSoundHandle handle;
		// Cebado empírico: un segundo BSSoundHandle, sin posición, mantenido
		// vivo (sin Stop() hasta el destructor) -- en las pruebas en el
		// juego, "handle" solo llegó a sonar en la configuración que
		// también tenía este segundo handle vivo en paralelo. Pararlo
		// inmediatamente después de arrancarlo (probado) volvió a dejar
		// "handle" mudo. Sin explicación confirmada todavía -- ver
		// CHANGELOG.md para el historial de depuración.
		RE::BSSoundHandle primingHandle;
		bool              started{ false };
		float             consumedClipTime{ 0.0f };

		// Distancia de la muestra anterior, para medir la velocidad de
		// cierre por diferencia -- valor negativo usado como centinela de
		// "todavía no hay muestra anterior" (una distancia real nunca es
		// negativa).
		float previousDistance{ -1.0f };
	};
}
