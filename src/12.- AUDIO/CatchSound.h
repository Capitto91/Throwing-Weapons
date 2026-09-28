// Sonidos del atrape: arranque anticipado durante el regreso y golpe final al cerrar la mano.

#pragma once

namespace Audio
{
	// Temporizador del sonido de arranque; cada sonido se dispara una vez y suena solo.
	class CatchCue
	{
	public:
		// a_startDelay: segundos desde ahora hasta el sonido de arranque.
		// Lo crea Return::BeginReturn, que calcula el retardo.
		explicit CatchCue(float a_startDelay) noexcept :
			startDelay(a_startDelay)
		{}

		// Suma a_deltaSeconds y suena una vez al alcanzar el retardo.
		// Lo llaman cada tick los bucles de Return (temblor y movimiento).
		void UpdateStart(const RE::NiPoint3& a_position, float a_deltaSeconds);

		// Golpe final del atrape en a_position.
		// Lo llama WeaponManager::OnCatchReleaseAnimationEvent cuando la mano se cierra.
		static void PlayEnd(const RE::NiPoint3& a_position);

	private:
		float startDelay;
		float elapsed{ 0.0f };
		bool  startFired{ false };
	};
}
