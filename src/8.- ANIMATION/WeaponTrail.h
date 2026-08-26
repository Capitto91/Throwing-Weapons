// Estela visual del arma durante el vuelo (ida y vuelta).
// Reposiciona cada tick los segmentos ya modelados de un NIF de efecto
// (Constants::kTrailEffectPath) siguiendo el historial reciente de
// posiciones de la réplica en vuelo.
//
// Basado en el sistema de estelas de Precision (Ershin, MIT License,
// github.com/ersh1/Precision, src/AttackTrail.h/.cpp) -- reimplementado
// para seguir la posición de la réplica en vuelo en vez de un arma
// equipada en la mano de un actor, así que no necesita nada de la lógica
// de "qué NIF de estela usar según el arma/encantamiento" del original:
// siempre es la misma réplica única.
//
// Historia: hubo una primera versión de esto (v1.7.1) que se abandonó por
// completo en v1.9.8 ("a petición del usuario tras varias rondas de
// depuración sin resultado convincente", ver CHANGELOG.md) -- la
// arquitectura de fiabilidad (spawn único, reposicionar huesos ya
// existentes, tangente por historial de posiciones, parada por RAII) ya
// funcionaba correctamente en ese momento; el motivo real del abandono
// fue casi con toda seguridad estético (malla genérica de espada de
// Precision con un tinte azul plano, sin ninguna textura de rayo). Esta
// versión recupera esa misma arquitectura tal cual (código real
// recuperado de git, commit 43d7b1c) y al principio simplificaba dos
// cosas: el punto de anclaje ya no se desplazaba en el espacio del giro
// (Constants::kTrailAnchorLocalOffset, eliminado en su momento -- usaba
// directamente la posición del nodo raíz de la réplica), y el color/
// brillo ya no se sobreescribe por código (kTrailBaseColor/
// kTrailBaseColorScaleMult, eliminados -- se hornea en el shader del NIF
// final, mismo mecanismo que ThorMjolnirSparks.nif). La primera
// simplificación se deshizo el mismo día (2026-08-26): el nodo raíz no
// coincide con el centro visual del arma (cae en la base del mango, ver
// Constants::kTrailAnchorLocalOffset) y usarlo tal cual se notaba como
// "el inicio del trail falla" -- reintroducido, pero transformado por la
// rotación del nodo RAÍZ (constante en vuelo) en vez de la del nodo de
// giro (que sí gira, y era la causa original de que se retirase la
// primera vez).

#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace Animation
{
	class WeaponTrail
	{
	public:
		WeaponTrail() = default;

		// Fuerza al efecto a expirar de inmediato si seguía activo -- sin
		// esto, el NIF spawneado se queda visible y quieto en el sitio
		// hasta agotar su propio tiempo de vida (varios segundos), tanto
		// al clavarse como al cancelarse el vuelo desde fuera (botón de
		// recuperar a mitad de la ida). Comprobado en el juego (versión
		// anterior, v1.7.1).
		~WeaponTrail();

		// Solo movible: cada instancia posee un único efecto en la
		// escena. Copiarla duplicaría el NiPointer sin duplicar el
		// efecto, y la primera copia en destruirse lo apagaría para la
		// otra.
		WeaponTrail(const WeaponTrail&) = delete;
		WeaponTrail& operator=(const WeaponTrail&) = delete;
		WeaponTrail(WeaponTrail&&) = default;
		WeaponTrail& operator=(WeaponTrail&&) = default;

		// Crea el efecto de estela (Constants::kTrailEffectPath) en
		// a_cell, posicionado en a_initialPosition. Sin efecto (con aviso
		// en el log) si el NIF no llega a cargar -- las siguientes
		// llamadas a Update simplemente no harán nada.
		//
		// a_upReference (vector de MUNDO, no hace falta unitario ni
		// ortogonal a la dirección de vuelo) fija el PLANO de la cinta,
		// sin bancado -- el llamante lo captura de la normal real del
		// plano de la trayectoria (perpendicular a la dirección de vuelo
		// en todo instante, ver Throw::LaunchWeapon/Return::BeginReturnMovement),
		// no de Animation::TickSpin/GetSpinLocalRotation (el giro en vuelo
		// no debe mover la estela, ver comentario de cabecera del .cpp).
		//
		// a_roll (radianes) gira la cinta sobre su propio eje de avance,
		// un ángulo FIJO para todo el tramo -- el llamante lo calcula una
		// única vez comparando, en el instante de lanzar, la base sin
		// bancado (a_upReference) contra la que daría el eje real del arma
		// (ver Math::SetRotationFromForwardUp). Reintroduce el encaje con
		// el ángulo de salida del arma sin reintroducir bancado: al ser un
		// único ángulo fijo aplicado siempre igual, nunca se acumula.
		//
		// a_anchorWorldOffset (vector de MUNDO, ya transformado por el
		// llamante -- ver Constants::kTrailAnchorLocalOffset * rootWorld)
		// se suma a a_initialPosition aquí y a a_currentPosition en cada
		// Update() -- compensa que el nodo raíz de la réplica no coincide
		// con el centro visual del arma (cae en la base del mango). Fijo
		// para todo el tramo, igual que a_upReference/a_roll: el llamante
		// lo calcula una única vez con la rotación del nodo RAÍZ (que no
		// gira en vuelo), nunca con la del nodo de giro.
		void Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset);

		// Sustituye el roll fijado en Start() -- pensado para el
		// enderezado del regreso (punto 10, segunda mitad,
		// Animation::TickSpinStraighten): el arma funde su orientación
		// hacia la de la mano en la ventana final antes de llegar
		// (Constants::kSpinStraightenLeadTime), pero el roll de la estela
		// se quedaba fijo con el valor de todo el vuelo -- desalineados
		// justo en el tramo más visible. El llamante (Return::BeginReturnMovement)
		// recalcula el roll objetivo cada tick durante esa misma ventana
		// (mismo blend que TickSpinStraighten) y lo aplica aquí -- solo
		// afecta a los PRÓXIMOS segmentos que se añadan, los ya colocados
		// no se retocan (mismo criterio que el resto de la estela: nunca
		// se reescribe la posición/orientación de un segmento ya fijado
		// salvo por reciclado).
		void SetRoll(float a_roll);

		// Añade a_currentPosition al historial y reposiciona los
		// segmentos de la cadena Constants::kTrailRootNodeName según la
		// distancia recorrida desde el tick anterior (no el tiempo -- ver
		// Constants::kTrailLength/kTrailSegmentSpacing). Sin efecto si
		// Start no llegó a crear el efecto, o si su NIF no tiene esa
		// cadena de segmentos. a_deltaSeconds ya no participa en el
		// espaciado/reciclado de segmentos, solo en el log de diagnóstico.
		void Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds);

	private:
		RE::NiPointer<RE::BSTempEffectParticle> particle;
		std::vector<RE::NiPoint3>               history;

		// Referencia de "hacia arriba" y ángulo de roll, capturados una
		// única vez en Start() (ver su doc comment) -- fijan el plano y el
		// giro sobre el eje de avance durante todo el tramo, constantes
		// aunque el giro visual del arma siga avanzando.
		RE::NiPoint3 upReference{ 0.0f, 0.0f, 1.0f };
		float        roll{ 0.0f };
		RE::NiPoint3 anchorWorldOffset{ 0.0f, 0.0f, 0.0f };

		// Distancia recorrida acumulada desde Start() (no tiempo -- ver
		// Constants::kTrailLength/kTrailSegmentSpacing, 2026-08-26).
		// segmentDistances guarda, por cada segmento activo, el valor de
		// totalDistance que tenía en el instante en que se colocó -- un
		// segmento se recicla cuando totalDistance ya lo ha dejado
		// kTrailLength unidades atrás (antes: cuando pasaba un tiempo fijo,
		// ver Constants.h para el porqué del cambio).
		std::deque<float> segmentDistances;
		float             totalDistance{ 0.0f };

		std::uint32_t currentBoneIdx{ 0 };
		float         currentTime{ 0.0f };  // solo para espaciar el log de diagnóstico
		float         segmentsToAddRemainder{ 0.0f };

		// Diagnóstico temporal (2026-08-26): el log de una sesión de prueba
		// real no tenía ni una sola línea de WeaponTrail (ni las de éxito ni
		// las de warn ya existentes), pese a varios ciclos completos de
		// lanzamiento/regreso -- indica que el sistema no llega a correr en
		// absoluto, no que corra con datos equivocados. Estos flags evitan
		// inundar el log (una vez por Start()) mientras se localiza en cuál
		// de los tres puntos silenciosos posibles se está cayendo. Quitar en
		// cuanto se identifique la causa real.
		bool diagLoggedTrailRootResolved{ false };

		// Segundo hallazgo (mismo diagnóstico, 2026-08-26): el sistema sí
		// corre (nodo resuelto, segmentos colocados), pero el primer
		// segmento del regreso quedaba a ~1 unidad del arma, contra ~105 en
		// el lanzamiento -- puede ser un bug real, o puede ser que el
		// regreso arranca a velocidad 0 (Return::ComputeReturnAcceleration)
		// y a los ~66ms del primer segmento apenas se ha movido nada,
		// comportamiento físicamente correcto. Sustituye el log de "primer
		// segmento" (una sola muestra, no distingue los dos casos) por
		// muestreo periódico durante toda la vida del efecto, para ver si
		// el retraso crece con la velocidad real o se queda plano. Quitar
		// junto con diagLoggedTrailRootResolved en cuanto se
		// diagnostique/arregle.
		float diagLastLogTime{ -1.0f };
	};
}
