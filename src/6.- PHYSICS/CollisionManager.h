// Gestiona las colisiones del arma.
// Detecta impactos contra enemigos, paredes y otras superficies del mundo.

#pragma once

// Colisión propia por raycast: sin RE::Projectile no hay ImpactResult ni
// TESHitEvent gratis, así que el impacto en vuelo (y el punto real de la
// mirilla al apuntar) se resuelven a mano con RE::TES::Pick — ver
// CLAUDE.md, "APIs verificadas para reconstruir lo que daba gratis
// RE::Projectile".

namespace Collision
{
	struct HitResult
	{
		bool               hit{ false };
		RE::NiPoint3       point{};
		RE::TESObjectREFR* target{ nullptr };            // referencia golpeada, si se pudo resolver (nullptr si no).
		RE::COL_LAYER      layer{ RE::COL_LAYER::kUnidentified };  // capa de colisión de lo golpeado, solo diagnóstico.
		float              fraction{ 0.0f };              // 0-1 a lo largo de a_from->a_to; ver SweepRaycast.
	};

	// Lanza un rayo desde a_from hasta a_to (unidades de juego) y devuelve
	// dónde impacta, si impacta. Resuelve el rootCollidable golpeado a una
	// referencia con RE::TESHavokUtilities::FindCollidableRef; si esa
	// referencia coincide con a_ignore1 o a_ignore2, se descarta como si no
	// hubiera golpeado nada — CFilter codifica una sola capa de colisión,
	// no permite excluir referencias concretas de la consulta (ver
	// CLAUDE.md), así que el filtrado por puntero se hace aquí a mano.
	// Pensado para ignorar al lanzador y a la propia réplica en vuelo.
	// a_rayLayer es la capa que el propio rayo declara tener: determina,
	// según la matriz de interacción de capas del motor (no expuesta,
	// fuera de nuestro control), contra qué otras capas puede siquiera
	// generar un candidato de impacto — antes de que este código llegue a
	// filtrar nada. kProjectile (igual que las flechas) es el valor por
	// defecto.
	HitResult Raycast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::TESObjectREFR* a_ignore1 = nullptr, RE::TESObjectREFR* a_ignore2 = nullptr, RE::COL_LAYER a_rayLayer = RE::COL_LAYER::kProjectile);

	// Un punto del recorrido, probado contra dos capas (kProjectile, la de
	// las flechas; y si no detecta nada, kLineOfSight, la que usa el
	// propio juego para comprobar si hay algo sólido en medio) — se queda
	// con la primera que detecte algo. Confirmado en el juego: sin
	// kLineOfSight, cierto entorno (paredes gruesas) no registraba ningún
	// impacto con kProjectile, sea cual sea el rayo usado.
	HitResult RaycastSolid(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::TESObjectREFR* a_ignore1 = nullptr, RE::TESObjectREFR* a_ignore2 = nullptr);

	// Colisión en vuelo: varios RaycastSolid en cruz alrededor de la línea
	// central, separados a_radius unidades, quedándose con el que impacta
	// primero (menor fracción a lo largo del recorrido) entre todos.
	// Aproxima el volumen real del arma (un único rayo puede pasar de
	// largo junto a geometría irregular o un actor en movimiento) y,
	// comprobado en el juego, también encuentra un punto de contacto más
	// superficial que un único rayo — varias muestras cercanas entre sí
	// tienden a dar con la cara visible de la superficie en vez de siempre
	// acabar en una capa de colisión más profunda, reduciendo cuánto se
	// hunde visualmente el arma clavada. El punto devuelto se recalcula
	// siempre sobre la línea central (a_from->a_to), no sobre el rayo
	// desviado que detectó el impacto — de lo contrario el arma se clava
	// hasta a_radius unidades a un lado de la trayectoria real, en vez de
	// en el punto exacto por el que pasaba (comprobado en el juego).
	HitResult SweepRaycast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_radius, RE::TESObjectREFR* a_ignore1 = nullptr, RE::TESObjectREFR* a_ignore2 = nullptr);
}
