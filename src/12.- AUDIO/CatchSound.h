// Sonidos del atrape: arranque anticipado durante el regreso y golpe final al cerrar la mano.

#pragma once

namespace Audio
{
	// Disparo del sonido de arranque; cada sonido se dispara una vez y suena solo.
	class CatchCue
	{
	public:
		// Suena una vez cuando a_secondsToArrival baja a Constants::kCatchStartSoundLeadTime.
		// Lo llaman cada tick los bucles de Return (temblor y vuelo) con su llegada prevista.
		void UpdateStart(const RE::NiPoint3& a_position, float a_secondsToArrival);

		// Golpe final del atrape en a_position.
		// Lo llama WeaponManager::OnCatchReleaseAnimationEvent cuando la mano se cierra.
		static void PlayEnd(const RE::NiPoint3& a_position);

	private:
		bool startFired{ false };
	};
}
