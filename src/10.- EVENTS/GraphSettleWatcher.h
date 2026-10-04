// Sigue en los grafos de animación del jugador si hay un desenvainado en curso (BeginWeaponDraw .. WeapEquip_Out)
// y si un attackStop nuestro sigue sin procesar. Lo consulta Lightning Dash antes del golpe en salto.
#pragma once

namespace Events::GraphSettleWatcher
{
	// Engancha el vigilante a todos los grafos del gestor de animación activo del jugador (sin duplicar).
	void Track(RE::Actor& a_player);

	// Avisa de que viene un desenvainado aunque sus eventos aún no hayan llegado (p. ej. tras desequipar el arma).
	void NoteDrawExpected();

	// Marca (a_pending) un attackStop a punto de enviarse, pendiente hasta su evento attackStop; se llama antes de
	// enviarlo y, con false, después si el grafo lo rechazó.
	void NoteAttackStopSent(bool a_pending);

	// Sin desenvainado en curso ni attackStop pendiente.
	[[nodiscard]] bool IsSettled();

	// Olvida lo pendiente: lo llama quien agota su red de seguridad, para no arrastrar un estado que no se cerró.
	void Reset();
}
