// Reproducción de los sonidos sueltos del arma, por archivo directo.
// Ver Constants.h ("Sonido de lanzamiento"/"Sonido de atrape") para el
// porqué de referenciar el .wav directamente en vez de un Sound
// Descriptor de la Creation Kit.

#pragma once

namespace Audio
{
	// Reproduce a_filePath (ruta relativa a Data, p. ej.
	// "Sound/FX/ThorMjolnir/MjolnirCall02_End.wav") en a_position, vía
	// RE::BSAudioManager::GetSoundHandleByFile (RE::BSResource::ID::
	// GenerateFromPath) -- sin pasar por ningún Sound Descriptor ni
	// EditorID del juego. Mismo patrón ya usado en el proyecto para el
	// .nif de la estela (Constants::kTrailEffectPath): referenciar el
	// archivo directamente en vez de un registro del juego.
	//
	// Sustituye por completo (2026-09-23, a petición del usuario, ver
	// CHANGELOG.md v1.19.26-v1.19.27) al mecanismo anterior por Sound
	// Descriptor (FormID local + EditorID, con un cebado y un
	// RE::PlaySound de refuerzo además del RE::BSSoundHandle real) --
	// "arranque de Atrape" dejó de sonar de forma fiable con ese
	// mecanismo, sin ningún cambio de código de por medio, coincidiendo
	// con una actualización de Skyrim; usado como control fiable durante
	// toda la investigación anterior, así que la causa apunta al entorno
	// (versión del juego/CommonLibSSE-NG), no a la lógica de la
	// aplicación. Sin explicación firme de la causa exacta -- el usuario
	// no puede revertir la versión del juego para una prueba de control.
	void PlayFileOneShot(const RE::NiPoint3& a_position, const char* a_filePath, float a_volume);

	// Gasta, al cargar partida (Events::OnSKSEMessage(kDataLoaded)), el
	// "primer intento perdido" de cada uno de los cuatro sonidos del arma
	// (lanzamiento, llamada, arranque y golpe final del atrape, ver
	// Constants.h) -- mismo mecanismo exacto que PlayFileOneShot, con
	// volumen 0.
	//
	// Bug de "no suena en el primer intento de la partida" (investigado a
	// fondo 2026-09-22, ver CHANGELOG.md v1.19.3-v1.19.11, entonces sobre
	// el mecanismo por Sound Descriptor ya retirado): no es un problema de
	// caché de archivo (Audio::PrecacheDescriptor confirmado irrelevante)
	// -- hipótesis más fundamentada, sin confirmar con certeza:
	// RE::BSAudioManager es un sistema de colas de mensajes con hilo
	// propio (ver RE/B/BSSoundMessage.h), y la primera vez que se pide una
	// identidad nueva hace falta que el hilo de audio procese un Init/
	// LoadForPlayback antes de que un Play/FadeIn inmediatamente
	// posterior tenga efecto. "Catch end" en concreto necesitaba dos usos
	// reales antes de estabilizarse, no uno como los otros 3 (v1.19.24) --
	// sin datos todavía de si eso sigue aplicando a este mecanismo nuevo,
	// se mantiene el doble calentamiento por precaución.
	void WarmUpAll();
}
