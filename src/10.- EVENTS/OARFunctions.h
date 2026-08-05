// Integración con la API de Functions de Open Animation Replacer.
// Detecta las anotaciones de liberación de Lanzar/Llamada/Atrape -- ver
// CHANGELOG.md 2026-08-05 y CLAUDE.md para el porqué frente a otros
// mecanismos descartados.
//
// Los headers de "13.- EXTERNAL/OpenAnimationReplacer" son una copia literal
// (sin modificar) de src/API/ en github.com/ersh1/OpenAnimationReplacer --
// el propio header lo pide así ("Copy this file into your own project if
// you wish to use this API"). No editar esos archivos directamente; si hace
// falta actualizarlos, sustituirlos enteros por la versión nueva.
//
// A diferencia del mecanismo anterior (OAR dispara NotifyAnimationGraph vía
// su función nativa SendAnimEvent, que resultó no propagarse a ningún
// BSTEventSink externo -- confirmado en el juego: el propio Animation Event
// Log de OAR mostraba el evento disparándose, pero nuestro sink nunca lo
// recibía), esta vía registra funciones custom propias directamente en OAR
// -- OAR nos llama a nosotros por función (Run/RunImpl), en el mismo
// proceso, sin ningún evento de por medio.
#pragma once

namespace Events::OARFunctions
{
	// Debe llamarse una única vez, dentro de SKSE::MessagingInterface::kPostLoad
	// (o antes) -- después de ese punto OAR ya ha cerrado su mapa de
	// fábricas de funciones y el registro no tendría efecto (documentado en
	// el propio header de la API). Si Open Animation Replacer no está
	// instalado o es una versión sin esta API, GetAPI() devuelve null y esta
	// función se limita a avisar por log -- el ciclo sigue funcionando con
	// la red de seguridad por tiempo de cada Begin*Animation, solo se pierde
	// la sincronía fina.
	void RegisterAll();
}
