// Implementación de la estela visual del arma.
// Reposiciona los segmentos ya modelados del NIF de efecto siguiendo el
// historial de posiciones de la réplica -- ver WeaponTrail.h para el
// diseño completo y el historial de la versión anterior.
//
// Portado de AttackTrail::Update de Precision (Ershin, MIT License,
// github.com/ersh1/Precision, src/AttackTrail.cpp), quitando todo lo que
// no aplica a nuestro caso de uso (GetTrailDefinition -- siempre es la
// misma réplica, no hay que elegir NIF según arma/encantamiento -- y el
// escalado según la longitud del arma equipada).
//
// La dirección de cada segmento se calcula distinto al original: Precision
// deduce la tangente comparando dos interpolaciones Catmull-Rom (posición e
// "hacia dónde mira" la propia réplica en cada muestra), lo que asume que
// la rotación de la réplica cambia de una muestra a otra -- válido para su
// caso (el arma gira de verdad durante un mandoble), no para el nuestro:
// Throw::LaunchWeapon nunca actualiza el ángulo de la réplica en vuelo
// (Physics::SyncHavok se llama con a_refr.GetAngle() sin cambios cada
// tick), así que esa técnica degenera en una dirección constante e
// incorrecta (Catmull-Rom es afín, CR(p+C,t) = CR(p,t)+C, así que con
// rotación constante la resta de las dos interpolaciones da siempre el
// mismo vector fijo). En su lugar, la tangente sale directamente del
// propio historial de posiciones (Math::CatmullRomTangent sobre las 4
// muestras usadas para interpolar), que sí refleja el recorrido real sin
// depender de ningún ángulo de la réplica.
//
// Ese "hacia dónde mira la réplica" de Precision no solo daba la
// dirección de avance -- también fijaba el PLANO de la cinta (el giro
// alrededor del propio eje de avance, un tercer grado de libertad que un
// vector de dirección por sí solo no puede fijar). Al descartarlo, la
// primera versión de este archivo dejaba ese plano en manos de una
// referencia de mundo implícita sin relación con el arma -- diagnosticado
// en el juego 2026-08-26 como estela "en un plano distinto" al del arma.
// Arreglado con upReference (ver WeaponTrail.h): una única captura de un
// eje real del arma en el instante de Start(), no una lectura continua --
// fija el plano de la cinta sin que la estela llegue a rotar con el giro
// visual en vuelo (Animation::TickSpin), que debe quedar fuera de esto.

#include "8.- ANIMATION/WeaponTrail.h"

#include "1.- CORE/Constants.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"

namespace Animation
{
	namespace
	{
		// Tiempo de vida del propio efecto (no confundir con
		// Constants::kTrailLength, que es el alcance por distancia de cada
		// segmento individual): margen amplio para cubrir cualquier vuelo,
		// mismo valor que Precision.
		constexpr float kParticleLifetime = 10.0f;

		// Transformación local de a_node a partir de su transformación
		// mundial ya calculada y la de su padre actual. Portado de
		// Precision (Utils::GetLocalTransform).
		RE::NiTransform GetLocalTransform(RE::NiAVObject* a_node, const RE::NiTransform& a_worldTransform)
		{
			if (auto* parent = a_node->parent) {
				return parent->world.Invert() * a_worldTransform;
			}

			return a_worldTransform;
		}

		// Aparca todos los segmentos de a_trailRootNode en a_parkedTransform
		// (escala 0, invisibles) -- la orientación no importa a escala 0.
		// Usada mientras no hay historial suficiente para interpolar una
		// dirección real, para que los segmentos nunca lleguen a
		// renderizarse en la pose de fábrica que trae el NIF.
		void ParkAllSegments(RE::NiNode& a_trailRootNode, const RE::NiTransform& a_parkedTransform)
		{
			auto&      segments = a_trailRootNode.GetChildren();
			const auto segmentCount = static_cast<std::uint32_t>(segments.size());
			for (std::uint32_t i = 0; i < segmentCount; ++i) {
				if (auto& segmentBone = segments[static_cast<std::uint16_t>(i)]) {
					segmentBone->local = GetLocalTransform(segmentBone.get(), a_parkedTransform);
					segmentBone->world = a_parkedTransform;
				}
			}
		}
	}

	WeaponTrail::~WeaponTrail()
	{
		if (particle) {
			// No hay ningún "Stop" nativo expuesto para un
			// BSTempEffectParticle a medio vivir -- en vez de eso, se
			// fuerza age >= lifetime para que el propio motor lo detecte y
			// lo retire (Detach) en su siguiente actualización interna,
			// igual de rápido que si hubiera agotado su vida útil de
			// verdad. Mismo truco que Precision
			// (AttackTrail::Update: trailParticle->age += trailParticle->lifetime).
			particle->age += particle->lifetime;
		}
	}

	void WeaponTrail::Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset)
	{
		diagLoggedTrailRootResolved = false;
		diagLastLogTime = -1.0f;
		upReference = a_upReference;
		roll = a_roll;
		anchorWorldOffset = a_anchorWorldOffset;

		if (!a_cell) {
			// Diagnóstico temporal (ver WeaponTrail.h): este return era
			// silencioso -- si a_cell es nulo (p. ej. TESObjectREFR::GetParentCell()
			// sin resolver todavía justo tras spawnear la réplica), el
			// sistema entero no llega a arrancar y no quedaba ningún rastro
			// en el log.
			logs::warn("Animation::WeaponTrail::Start: a_cell es nulo, no se crea el efecto.");
			return;
		}

		const RE::NiPoint3 anchoredInitialPosition = a_initialPosition + anchorWorldOffset;
		logs::info("Animation::WeaponTrail::Start: creando efecto '{}' en ({:.1f},{:.1f},{:.1f}).", Constants::kTrailEffectPath, anchoredInitialPosition.x, anchoredInitialPosition.y, anchoredInitialPosition.z);

		// Nace a escala 0 (invisible) a propósito -- ver Update, que la
		// restaura a 1 en su primera llamada exitosa. BSTempEffectParticle
		// carga su 3D en segundo plano de forma asíncrona (igual que
		// cualquier 3D nuevo), así que intentar "cazar" el instante justo
		// en que está listo para aparcar los segmentos es una carrera que
		// a veces se gana y a veces no (causa del defecto intermitente
		// reportado en la versión anterior, ver CHANGELOG.md v1.7.11).
		// Naciendo a escala 0 no hay ninguna carrera que ganar: el efecto
		// es invisible desde el primer fotograma en que llega a
		// renderizarse, sea cual sea el momento exacto en que su 3D
		// termine de cargar. La rotación inicial no importa a escala 0 --
		// se pasa identidad.
		particle = RE::NiPointer<RE::BSTempEffectParticle>(
			RE::BSTempEffectParticle::Spawn(a_cell, kParticleLifetime, Constants::kTrailEffectPath, RE::NiMatrix3{}, anchoredInitialPosition, 0.0f, 7, nullptr));

		if (!particle) {
			logs::warn("Animation::WeaponTrail::Start: no se pudo crear el efecto '{}'.", Constants::kTrailEffectPath);
		} else {
			logs::info("Animation::WeaponTrail::Start: BSTempEffectParticle creado correctamente (particleObject todavía puede tardar en cargar su 3D).");
		}
	}

	void WeaponTrail::SetRoll(float a_roll)
	{
		roll = a_roll;
	}

	void WeaponTrail::Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds)
	{
		if (!particle || !particle->particleObject) {
			// Diagnóstico temporal (ver WeaponTrail.h): este return también
			// era silencioso -- si particleObject nunca llega a resolver
			// (carga asíncrona que nunca termina, o particle nulo por el
			// caso de arriba), Update() no hacía nada tick tras tick sin
			// dejar ningún rastro. history tampoco se llena en este caso.
			currentTime += a_deltaSeconds;
			return;
		}

		// a_currentPosition es la posición LÓGICA del arma (nodo raíz de
		// la réplica) -- se ancla aquí, no en cada llamante, para que
		// todo lo demás en esta función (historial, log de diagnóstico)
		// use siempre el punto ya compensado. Ver WeaponTrail.h,
		// a_anchorWorldOffset.
		const RE::NiPoint3 anchoredPosition = a_currentPosition + anchorWorldOffset;

		const float distanceThisTick = history.empty() ? 0.0f : (anchoredPosition - history.back()).Length();
		history.emplace_back(anchoredPosition);
		totalDistance += distanceThisTick;

		auto* fadeNode = particle->particleObject->AsFadeNode();
		if (!fadeNode) {
			return;
		}

		fadeNode->GetRuntimeData().currentFade = 1.0f;

		// Restaura la escala del nodo raíz del efecto a 1 (nace a 0, ver
		// Start) -- a partir de aquí, cada segmento individual controla su
		// propia visibilidad con su propia escala (0 mientras se aparca,
		// más abajo). particleObject no tiene padre activo (no es hijo de
		// ningún nodo que el motor recalcule cada fotograma), así que basta
		// con escribir local/world directamente, mismo patrón que ya usa
		// el resto de esta función con los segmentos.
		particle->particleObject->local.scale = 1.0f;
		particle->particleObject->world.scale = 1.0f;

		auto* trailRoot = fadeNode->GetObjectByName(Constants::kTrailRootNodeName);
		auto* trailRootNode = trailRoot ? trailRoot->AsNode() : nullptr;
		if (!trailRootNode) {
			logs::warn("Animation::WeaponTrail::Update: el efecto '{}' no tiene el nodo '{}' (NIF sin la convención de estela esperada).", Constants::kTrailEffectPath, Constants::kTrailRootNodeName);
			currentTime += a_deltaSeconds;
			return;
		}

		auto&      segments = trailRootNode->GetChildren();
		const auto segmentCount = static_cast<std::uint32_t>(segments.size());
		if (segmentCount == 0) {
			currentTime += a_deltaSeconds;
			return;
		}

		if (!diagLoggedTrailRootResolved) {
			logs::info("Animation::WeaponTrail::Update: nodo '{}' resuelto con {} segmentos hijos.", Constants::kTrailRootNodeName, segmentCount);
			diagLoggedTrailRootResolved = true;
		}

		// Catmull-Rom necesita 4 muestras para interpolar una dirección real
		// (ver más abajo) -- hasta entonces no hay suficiente historial. Se
		// aparcan a escala 0 en la posición actual -- la orientación no
		// importa a escala 0.
		if (history.size() < 4) {
			RE::NiTransform parkedTransform;
			parkedTransform.translate = history.back();
			parkedTransform.scale = 0.0f;
			ParkAllSegments(*trailRootNode, parkedTransform);
			currentTime += a_deltaSeconds;
			return;
		}

		// Puntos de control de Catmull-Rom (las 4 últimas muestras del
		// historial) -- se calculan una única vez y se reutilizan tanto
		// para la posición como para la orientación de cada segmento, y
		// para el tramo "aparcado" de más abajo. No depende de ningún
		// ángulo de la réplica, a diferencia del original de Precision
		// (ver comentario al inicio del archivo).
		auto p3It = history.rbegin();
		auto p2It = p3It + 1;
		auto p1It = p2It + 1;
		auto p0It = p1It + 1;

		const auto& ip0 = *p0It;
		const auto& ip1 = *p1It;
		const auto& ip2 = *p2It;
		const auto& ip3 = *p3It;

		// Dirección de avance (eje Y local, ver Math::SetRotationFromForwardUp)
		// de un segmento en el punto a_t de la curva (0 = ip1, 1 = ip2, ver
		// Math::CatmullRom) -- tangente real de la curva
		// (Math::CatmullRomTangent) evaluada en el punto exacto de CADA
		// segmento, en vez de una única dirección compartida por todos los
		// segmentos de un mismo tick (2026-08-26, arreglo de "poca
		// resolución"/facetado visible cuando la trayectoria curva rápido
		// dentro de un solo tick -- las posiciones ya se interpolaban
		// suaves, la orientación no). El eje Y del segmento va de tenue
		// (origen) a intenso (creciente) en el NIF, pero en el juego el
		// resultado salía al revés (confirmado por el usuario), así que se
		// niega antes de devolverlo.
		const auto segmentAxisAt = [&ip0, &ip1, &ip2, &ip3](float a_t) {
			RE::NiPoint3 tangent = Math::CatmullRomTangent(ip0, ip1, ip2, ip3, a_t);
			const float  length = tangent.Length();
			tangent = length > 0.0f ? tangent / length : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
			return -tangent;
		};

		float      segmentsToAdd = segmentsToAddRemainder + distanceThisTick / Constants::kTrailSegmentSpacing;
		const auto segmentsToAddTrunc = static_cast<std::uint32_t>(segmentsToAdd);
		segmentsToAddRemainder = segmentsToAdd - static_cast<float>(segmentsToAddTrunc);

		// Recicla segmentos que ya han quedado kTrailLength unidades por
		// detrás de la posición actual (o fuerza el hueco necesario si no
		// caben todos los nuevos) antes de añadir ninguno.
		if (!segmentDistances.empty()) {
			std::uint32_t segmentsToMove = 0;
			for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
				if (i < segmentDistances.size() && totalDistance - segmentDistances[i] > Constants::kTrailLength) {
					++segmentsToMove;
				} else {
					break;
				}
			}

			const std::uint32_t totalSegments = currentBoneIdx + segmentsToAddTrunc - segmentsToMove;
			if (totalSegments >= segmentCount) {
				segmentsToMove += totalSegments - (segmentCount - 1);
			}
			// (evitar std::min: Windows.h define una macro min() que lo
			// rompe si algún header transitivo la deja sin NOMINMAX)
			const auto distanceCount = static_cast<std::uint32_t>(segmentDistances.size());
			if (segmentsToMove > distanceCount) {
				segmentsToMove = distanceCount;
			}

			if (segmentsToMove > 0) {
				segmentDistances.erase(segmentDistances.begin(), segmentDistances.begin() + segmentsToMove);

				for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
					if (segmentCount > i + segmentsToMove) {
						// local Y world, las dos juntas -- igual que en
						// cualquier otro sitio de este proyecto donde se
						// reposiciona un nodo a mano (SetPosition/SetAngle
						// del arma, TickSpin, los propios segmentos nuevos
						// más abajo). Copiar solo local dejaba el mundial
						// del hueco de destino congelado en lo que tuviera
						// de antes -- con el buffer casi siempre lleno
						// (kTrailLength/kTrailSegmentSpacing ==
						// segmentCount), esto pasaba casi cada tick, así
						// que la mayoría de los segmentos activos se
						// quedaban con su posición mundial fija desde la
						// última vez que se escribieron de verdad (al
						// añadirse), en vez de seguir la del hueso cuyo
						// local acababan de heredar -- reportado por el
						// usuario como "se actualiza arriba/abajo pero no
						// a los lados, mantiene una posición curva todo el
						// rato" (2026-08-26), presente desde el principio.
						auto& destSegment = segments[static_cast<std::uint16_t>(i)];
						auto& srcSegment = segments[static_cast<std::uint16_t>(i + segmentsToMove)];
						destSegment->local = srcSegment->local;
						destSegment->world = srcSegment->world;
					}
				}

				currentBoneIdx -= segmentsToMove;
			}
		}

		// Añade los segmentos nuevos, interpolados con Catmull-Rom entre
		// las 4 últimas muestras del historial -- cada uno con su propia
		// orientación (segmentAxisAt(t)), no una compartida por tick.
		if (segmentsToAdd > 0.0f) {
			for (std::uint32_t i = 0; i < segmentsToAddTrunc; ++i) {
				if (segmentCount <= currentBoneIdx) {
					break;
				}

				auto& segmentBone = segments[static_cast<std::uint16_t>(currentBoneIdx)];
				if (!segmentBone) {
					continue;
				}

				const float t = (static_cast<float>(i) + 1.0f) / segmentsToAdd;
				const auto  interpolatedPos = Math::CatmullRom(ip0, ip1, ip2, ip3, t);
				const auto  segmentAxis = segmentAxisAt(t);

				RE::NiTransform newTransform = segmentBone->world;
				Math::SetRotationFromForwardUp(newTransform.rotate, segmentAxis, upReference, roll);
				newTransform.translate = interpolatedPos;
				newTransform.scale = Constants::kTrailSegmentScale;

				segmentBone->local = GetLocalTransform(segmentBone.get(), newTransform);
				segmentBone->world = newTransform;

				if (diagLastLogTime < 0.0f || currentTime - diagLastLogTime >= 0.15f) {
					const float lag = (anchoredPosition - interpolatedPos).Length();
					logs::info(
						"Animation::WeaponTrail::Update: t={:.2f}s dist={:.1f}u segmento#{} a {:.1f}u del arma (segmento=({:.1f},{:.1f},{:.1f}) arma=({:.1f},{:.1f},{:.1f})).",
						currentTime, totalDistance, currentBoneIdx, lag,
						interpolatedPos.x, interpolatedPos.y, interpolatedPos.z,
						anchoredPosition.x, anchoredPosition.y, anchoredPosition.z);
					diagLastLogTime = currentTime;
				}

				segmentDistances.emplace_back(totalDistance - distanceThisTick * (1.0f - t));
				++currentBoneIdx;
			}
		}

		// Estrecha cada segmento activo según cuánta distancia se ha
		// recorrido desde que se colocó (tamaño completo recién añadido,
		// hacia 0 según se acerca a kTrailLength unidades atrás) -- forma
		// un cono real que se va cerrando hacia la cola. Recorre solo el
		// tramo activo [0, currentBoneIdx) -- los aparcados más abajo no
		// tienen distancia real todavía.
		for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
			if (i >= segmentDistances.size()) {
				break;
			}

			auto& segmentBone = segments[static_cast<std::uint16_t>(i)];
			if (!segmentBone) {
				continue;
			}

			float ageFraction = (totalDistance - segmentDistances[i]) / Constants::kTrailLength;
			ageFraction = ageFraction < 0.0f ? 0.0f : (ageFraction > 1.0f ? 1.0f : ageFraction);

			const float taperedScale = Constants::kTrailSegmentScale * (1.0f - ageFraction);
			segmentBone->local.scale = taperedScale;
			segmentBone->world.scale = taperedScale;
		}

		// Los segmentos todavía sin usar se mantienen pegados a la
		// posición actual de la réplica pero a escala 0 (invisibles) --
		// no representan ninguna posición histórica real y evita
		// amontonar varias copias idénticas superpuestas justo en el
		// punto de anclaje.
		if (currentBoneIdx < segmentCount) {
			RE::NiTransform worldTransform;
			worldTransform.translate = history.back();

			const auto segmentAxis = segmentAxisAt(1.0f);
			Math::SetRotationFromForwardUp(worldTransform.rotate, segmentAxis, upReference, roll);
			worldTransform.scale = 0.0f;

			const auto localTransform = GetLocalTransform(segments[static_cast<std::uint16_t>(currentBoneIdx)].get(), worldTransform);
			for (std::uint32_t i = currentBoneIdx; i < segmentCount; ++i) {
				if (auto& segmentBone = segments[static_cast<std::uint16_t>(i)]) {
					segmentBone->local = localTransform;
					segmentBone->world = worldTransform;
				}
			}
		}

		currentTime += a_deltaSeconds;
	}
}
