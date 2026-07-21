// Resolución compartida de Sound Descriptor por FormID -- ver Constants.h
// ("Sonido de lanzamiento/vuelo/atrape") para el porqué de resolver por
// FormID en vez de por EditorID.

#pragma once

namespace Audio
{
	// Resuelve el RE::BGSSoundDescriptorForm identificado por
	// a_localFormID dentro del plugin a_modName -- usado tanto por
	// Audio::FlightSound/PlaySoundOneShot como por Audio::CatchSound.
	//
	// Prueba primero RE::TESDataHandler::LookupForm<RE::BGSSoundDescriptorForm>
	// directamente; si a_localFormID resulta ser el de un Sound Marker
	// (RE::TESSound) en vez de un Sound Descriptor, se resuelve como tal y
	// se usa su campo "Sound" (RE::TESSound::descriptor) -- así el
	// llamante no necesita saber cuál de los dos tipos de registro creó el
	// usuario en la Creation Kit para un FormID dado. Devuelve nullptr (con
	// aviso en el log) si no resuelve como ninguno de los dos.
	RE::BGSSoundDescriptorForm* ResolveSoundDescriptor(RE::FormID a_localFormID);

	// Precarga en caché los tres Sound Descriptor del arma (lanzamiento/
	// vuelo/atrape, ver Constants.h) -- llamar una vez en
	// Events::OnSKSEMessage(kDataLoaded), antes del primer lanzamiento.
	//
	// Motivo (comprobado en el juego): el primer acceso a un recurso de
	// audio nunca antes solicitado tarda en cargar de forma asíncrona --
	// mismo patrón ya documentado para el 3D de una réplica recién creada
	// (ver CLAUDE.md) -- y sin precarga, BSAudioManager::GetSoundHandle +
	// Play() reportan éxito (IsPlaying()=true, GetDuration() con un valor
	// real) pero no se oye nada durante los primeros lanzamientos, hasta
	// que el recurso queda cacheado. RE::BSAudioManager::PrecacheDescriptor
	// existe justo para esto (RE/B/BSAudioManager.h) -- a_flags sin
	// documentar en commonlibsse-ng, se usa 0 sin ninguna base más que ser
	// el valor neutro.
	void PrecacheAll();
}
