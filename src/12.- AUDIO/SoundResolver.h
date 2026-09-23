// Resolución compartida de Sound Descriptor por FormID -- ver Constants.h
// ("Sonido de lanzamiento/atrape") para el porqué de resolver por FormID
// en vez de por EditorID.

#pragma once

namespace Audio
{
	// Resuelve el RE::BGSSoundDescriptorForm identificado por
	// a_localFormID dentro de ThorMjolnirOAR.esp -- usado tanto por
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
	//
	// Bug de "no suena en el primer intento de la partida" (investigado a
	// fondo 2026-09-22, ver CHANGELOG.md v1.19.3-v1.19.11): afecta a
	// cualquier Sound Descriptor -- propio o vanilla -- la primerísima vez
	// que este mecanismo se llama sobre él en la sesión, sea cual sea el
	// tiempo real transcurrido desde la carga de la partida. No es un
	// problema de caché de archivo (Audio::PrecacheDescriptor confirmado
	// irrelevante, ver Audio::WarmUpAll más abajo) ni de qué función de
	// RE::BSAudioManager se use para obtener el handle (GetSoundHandle por
	// puntero a descriptor y GetSoundHandleByName por nombre se comportan
	// igual). Mitigado por Audio::WarmUpAll, que gasta ese primer intento
	// perdido al cargar partida en vez de en el primer uso real del jugador
	// -- no es un arreglo de la causa raíz, que sigue sin identificarse con
	// certeza (hipótesis más fundamentada: RE::BSAudioManager es un sistema
	// de colas de mensajes con hilo propio, ver RE/B/BSSoundMessage.h, y la
	// primera vez que se pide una identidad nueva hace falta que el hilo de
	// audio procese un Init/LoadForPlayback antes de que un Play/FadeIn
	// inmediatamente posterior tenga efecto -- sin confirmar, sin
	// desensamblador disponible para verificarlo contra el código nativo
	// real).
	void PlayReliableOneShot(const RE::NiPoint3& a_position, RE::FormID a_localFormID, const char* a_editorID);

	// Gasta, al cargar partida (Events::OnSKSEMessage(kDataLoaded)), el
	// "primer intento perdido" de cada uno de los cuatro Sound Descriptor
	// del arma (lanzamiento, llamada, arranque y golpe final del atrape,
	// ver Constants.h) -- mismo mecanismo exacto que PlayReliableOneShot
	// (mismo RE::PlaySound incluido, a petición del usuario, para
	// calentar exactamente lo mismo que calienta un uso real), pero con
	// volumen 0 en el RE::BSSoundHandle real. Sin forma de silenciar la
	// pata RE::PlaySound (no tiene parámetro de volumen) -- consecuencia
	// aceptada: cada uno de los 4 sonidos se oye una vez, de verdad, justo
	// al cargar partida. Decisión explícita del usuario (2026-09-22, ver
	// CHANGELOG.md v1.19.11) para maximizar la fiabilidad mientras se
	// evalúa si hace falta silenciarlo del todo más adelante.
	//
	// Sustituye a Audio::PrecacheAll/RE::BSAudioManager::PrecacheDescriptor,
	// confirmado irrelevante para este bug (ver el comentario de
	// PlayReliableOneShot) tras varias rondas de pruebas -- no comprueba
	// nada, no calienta nada que el propio bug necesite.
	void WarmUpAll();
}
