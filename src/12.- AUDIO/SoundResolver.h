// Resolución compartida de Sound Descriptor por FormID -- ver Constants.h
// ("Sonido de lanzamiento/atrape") para el porqué de resolver por FormID
// en vez de por EditorID.

#pragma once

namespace Audio
{
	// Resuelve el RE::BGSSoundDescriptorForm identificado por
	// a_localFormID dentro del plugin a_modName -- usado tanto por
	// Audio::PlayReliableOneShot como por Audio::CatchCue.
	//
	// Prueba primero RE::TESDataHandler::LookupForm<RE::BGSSoundDescriptorForm>
	// directamente; si a_localFormID resulta ser el de un Sound Marker
	// (RE::TESSound) en vez de un Sound Descriptor, se resuelve como tal y
	// se usa su campo "Sound" (RE::TESSound::descriptor) -- así el
	// llamante no necesita saber cuál de los dos tipos de registro creó el
	// usuario en la Creation Kit para un FormID dado. Devuelve nullptr (con
	// aviso en el log) si no resuelve como ninguno de los dos.
	RE::BGSSoundDescriptorForm* ResolveSoundDescriptor(RE::FormID a_localFormID);

	// Reproducción fiable de un sonido suelto en a_position, identificado por
	// a_localFormID (ver ResolveSoundDescriptor) y a_editorID (para el
	// RE::PlaySound de refuerzo). Mecanismo confirmado en el juego para los
	// sonidos de atrape (12.- AUDIO/CatchSound.cpp, ver Constants.h "Sonido
	// de atrape, en dos partes"): un RE::BSSoundHandle de cebado sin
	// posición, RE::PlaySound(a_editorID) en paralelo, y un RE::BSSoundHandle
	// real posicionado con FadeInPlay(0) -- las tres cosas a la vez, ninguna
	// sola basta (comprobado repetidas veces). Movida aquí desde
	// CatchSound.cpp para compartirla con cualquier otro sonido suelto que
	// necesite la misma fiabilidad (p. ej. Audio::CallSound).
	void PlayReliableOneShot(const RE::NiPoint3& a_position, RE::FormID a_localFormID, const char* a_editorID);

	// Precarga en caché los cuatro Sound Descriptor del arma (lanzamiento,
	// vuelo, arranque y golpe final del atrape, ver Constants.h) -- llamar
	// una vez en Events::OnSKSEMessage(kDataLoaded), antes del primer
	// lanzamiento.
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
