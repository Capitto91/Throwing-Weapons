// Sonido posicional del vuelo del arma (ida y vuelta) y sonidos sueltos de
// lanzamiento/atrape. Ver CLAUDE.md -- no hay ninguna mecánica del
// documento de diseño que cubra esto, es pulido audiovisual puro.

#pragma once

namespace Audio
{
	// Sonido en bucle que sigue a la réplica mientras vuela de verdad (fase
	// parabólica de la ida, curva del regreso) -- nunca durante el temblor
	// de desprendimiento ni mientras sigue clavada en un actor.
	//
	// RAII: RE::BSSoundHandle no se autolibera (~BSSoundHandle() = default,
	// verificado en BSSoundHandle.h) -- sin este destructor, cualquier
	// salida del ciclo que no llame a Stop() a mano dejaría el bucle
	// sonando para siempre.
	class FlightSound
	{
	public:
		FlightSound() = default;
		~FlightSound();

		// Solo movible: un único BSSoundHandle en vuelo por instancia:
		// copiarla podría parar el mismo sonido dos veces o dejarlo sin
		// parar si la copia "equivocada" es la que se destruye.
		FlightSound(const FlightSound&) = delete;
		FlightSound& operator=(const FlightSound&) = delete;
		FlightSound(FlightSound&&) = default;
		FlightSound& operator=(FlightSound&&) = default;

		// Arranca el Sound Descriptor/Marker identificado por
		// a_localFormID (ver Audio::ResolveSoundDescriptor) en bucle,
		// siguiendo a a_node cada fotograma (BSSoundHandle::SetObjectToFollow
		// -- no hace falta llamar a nada por tick). Sin efecto (con aviso en
		// el log) si no resuelve, o si a_node es nulo.
		void Start(RE::NiAVObject* a_node, RE::FormID a_localFormID);

	private:
		RE::BSSoundHandle handle;
	};

	// Sonido suelto (lanzamiento): se reproduce una vez en a_position y no
	// hace falta retenerlo -- el propio motor sigue reproduciéndolo por su
	// cuenta aunque el RE::BSSoundHandle local se destruya al momento
	// (mismo patrón que BSAudioManager::Play(BSISoundDescriptor*), que crea
	// un handle local de usar y tirar, ver BSAudioManager.cpp). Sin efecto
	// (con aviso en el log) si a_localFormID no resuelve.
	void PlaySoundOneShot(const RE::NiPoint3& a_position, RE::FormID a_localFormID);
}
