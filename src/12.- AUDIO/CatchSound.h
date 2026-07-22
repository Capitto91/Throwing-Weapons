// Los dos sonidos del atrape (arranque anticipado + golpe final
// garantizado) -- ver Constants.h ("Sonido de atrape, en dos partes") para
// el porqué del diseño en dos partes. No hay ninguna mecánica del
// documento de diseño que cubra esto, es pulido audiovisual puro.
//
// Reemplaza por completo el diseño anterior de un único sonido con ajuste
// continuo de velocidad de reproducción (RE::BSSoundHandle::SetFrequency
// cada tick, ver CHANGELOG.md) -- a petición del usuario, se acepta una
// pequeña desincronización del arranque a cambio de un diseño mucho más
// simple: el golpe final ya no depende de ningún cálculo de velocidad de
// cierre ni de que el arranque haya sonado, se dispara siempre, tal cual,
// exactamente en el instante real de la llegada.

#pragma once

namespace Audio
{
	// Sin RE::BSSoundHandle como miembro: cada sonido se dispara una sola
	// vez y se deja sonar por su cuenta (igual que Audio::PlaySoundOneShot
	// -- el motor sigue reproduciéndolo aunque el handle local que lo
	// arrancó se destruya al momento), así que esta clase no gestiona
	// ningún recurso que liberar, no hace falta RAII ni restringir copia/
	// movimiento.
	class CatchCue
	{
	public:
		// a_startDelay: segundos desde este mismo instante (el de la
		// propia construcción, que coincide con el principio de
		// Return::BeginReturn -- antes incluso del temblor de
		// desprendimiento si lo hay) hasta que debe sonar el arranque. Ver
		// Return::BeginReturn para el cálculo.
		explicit CatchCue(float a_startDelay) noexcept :
			startDelay(a_startDelay)
		{}

		// Llamar cada tick, tanto durante el temblor de desprendimiento
		// (Return::BeginReturn) como durante el movimiento
		// (Return::BeginReturnMovement) -- acumula su propio reloj interno
		// desde que se creó esta instancia, independiente de en cuál de
		// los dos bucles de tick se llame, así que el retardo cuenta igual
		// si el arma estaba clavada o no. Dispara el sonido de arranque
		// una sola vez, en cuanto ese reloj alcanza el retardo; no hace
		// nada más en las llamadas siguientes.
		void UpdateStart(const RE::NiPoint3& a_position, float a_deltaSeconds);

		// Llamar exactamente una vez, al detectar la llegada real a la
		// mano (Return::BeginReturnMovement) -- dispara el golpe grabado
		// siempre, sin condición, haya sonado ya el arranque o no.
		static void PlayEnd(const RE::NiPoint3& a_position);

	private:
		float startDelay;
		float elapsed{ 0.0f };
		bool  startFired{ false };
	};
}
