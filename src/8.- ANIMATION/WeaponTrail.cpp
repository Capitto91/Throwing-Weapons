// Implementación de la estela visual del arma.
// Reposiciona los segmentos ya modelados del NIF de efecto siguiendo el
// historial de posiciones de la réplica -- ver WeaponTrail.h y
// PLAN-trail.md para el diseño completo.
//
// Portado de AttackTrail::Update de Precision (Ershin, MIT License,
// github.com/ersh1/Precision, src/AttackTrail.cpp), quitando todo lo que
// no aplica a nuestro caso de uso (GetTrailDefinition -- siempre es la
// misma réplica, no hay que elegir NIF según arma/encantamiento --, el
// desvanecido por bExpired -- no hace falta "Stop" explícito, ver
// PLAN-trail.md -- y el escalado según la longitud del arma equipada).
//
// La dirección de cada segmento se calcula distinto al original: Precision
// deduce la tangente comparando dos interpolaciones Catmull-Rom (posición e
// "hacia dónde mira" la propia réplica en cada muestra), lo que asume que
// la rotación de la réplica cambia de una muestra a otra -- válido para su
// caso (el arma gira de verdad durante un mandoble), no para el nuestro:
// Throw::LaunchWeapon nunca actualiza el ángulo de la réplica en vuelo
// (Physics::SyncHavok se llama con a_refr.GetAngle() sin cambios cada
// tick), así que esa técnica degenera en una dirección constante e
// incorrecta (comprobado: Catmull-Rom es afín, CR(p+C,t) = CR(p,t)+C, así
// que con rotación constante la resta de las dos interpolaciones da
// siempre el mismo vector fijo). En su lugar, la tangente sale
// directamente del propio historial de posiciones (p3-p0, las muestras más
// antigua y más reciente de las 4 usadas para interpolar), que sí refleja
// el recorrido real sin depender de ningún ángulo de la réplica.

#include "8.- ANIMATION/WeaponTrail.h"

#include "1.- CORE/Constants.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"

namespace Animation
{
	namespace
	{
		// Tiempo de vida del propio efecto (no confundir con
		// Constants::kTrailSegmentLifetime, que es el de cada segmento
		// individual): margen amplio para cubrir cualquier vuelo, mismo
		// valor que Precision.
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

		// Punto real desde el que "nace" la estela: el nodo raíz del arma
		// (a_weaponTransform, tal cual la pasan Throw/Return) no coincide
		// con el centro visual de la pieza en la mayoría de armas de
		// Skyrim -- Constants::kTrailAnchorLocalOffset (comprobado por el
		// usuario en NifSkope + ajustado a ojo en el juego) se aplica en
		// el espacio local del arma, así que acompaña su rotación si
		// llega a cambiar.
		RE::NiPoint3 ComputeAnchorPosition(const RE::NiTransform& a_weaponTransform)
		{
			return a_weaponTransform.translate + a_weaponTransform.rotate * Constants::kTrailAnchorLocalOffset;
		}

		// Aparca todos los segmentos de a_trailRootNode en a_parkedTransform
		// (escala 0, invisibles) -- la orientación no importa a escala 0.
		// Usada tanto por Start (si el 3D del efecto ya está listo en ese
		// mismo instante) como por Update (mientras no hay historial
		// suficiente para interpolar una dirección real), para que los
		// segmentos nunca lleguen a renderizarse en la pose de fábrica que
		// trae el NIF.
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

		// Clona el material de brillo del segmento (para no compartir ni
		// mutar el material original del NIF entre varias estelas activas
		// a la vez, p. ej. ida y vuelta solapadas) y le aplica el color
		// por defecto. Solo hace falta una vez por instancia -- ver
		// appliedColorSettings.
		RE::BSVisit::BSVisitControl ApplyColorSettings(RE::BSGeometry* a_geometry)
		{
			auto* effectShader = netimmerse_cast<RE::BSEffectShaderProperty*>(a_geometry->GetGeometryRuntimeData().shaderProperty.get());
			if (!effectShader) {
				return RE::BSVisit::BSVisitControl::kContinue;
			}

			auto* material = effectShader->GetMaterial();
			if (!material) {
				return RE::BSVisit::BSVisitControl::kContinue;
			}

			if (auto* newMaterial = static_cast<RE::BSEffectShaderMaterial*>(material->Create())) {
				newMaterial->CopyMembers(material);
				effectShader->SetMaterial(newMaterial, false);
				// SetMaterial copia el material, así que el clon temporal
				// ya no hace falta.
				newMaterial->~BSEffectShaderMaterial();
				RE::free(newMaterial);

				material = effectShader->GetMaterial();
				material->baseColor = Constants::kTrailBaseColor;
				material->baseColorScale = Constants::kTrailBaseColorScaleMult;
			}

			return RE::BSVisit::BSVisitControl::kContinue;
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

	void WeaponTrail::Start(RE::TESObjectCELL* a_cell, const RE::NiTransform& a_initialTransform)
	{
		if (!a_cell) {
			return;
		}

		RE::NiTransform anchorTransform = a_initialTransform;
		anchorTransform.translate = ComputeAnchorPosition(a_initialTransform);

		// Nace a escala 0 (invisible) a propósito -- ver Update, que la
		// restaura a 1 en su primera llamada exitosa. BSTempEffectParticle
		// carga su 3D en segundo plano de forma asíncrona (igual que
		// cualquier 3D nuevo), así que intentar "cazar" el instante justo
		// en que está listo para aparcar los segmentos (intentado antes,
		// ver CHANGELOG) es una carrera que a veces se gana y a veces no
		// -- de ahí que el defecto reportado por el usuario fuera
		// intermitente (a veces claro, a veces apenas, a veces nada).
		// Naciendo a escala 0 no hay ninguna carrera que ganar: el efecto
		// es invisible desde el primer fotograma en que llega a
		// renderizarse, sea cual sea el momento exacto en que su 3D
		// termine de cargar.
		particle = RE::NiPointer<RE::BSTempEffectParticle>(
			RE::BSTempEffectParticle::Spawn(a_cell, kParticleLifetime, Constants::kTrailEffectPath, anchorTransform.rotate, anchorTransform.translate, 0.0f, 7, nullptr));

		if (!particle) {
			logs::warn("Animation::WeaponTrail::Start: no se pudo crear el efecto '{}'.", Constants::kTrailEffectPath);
		}
	}

	void WeaponTrail::Update(const RE::NiTransform& a_currentTransform, float a_deltaSeconds)
	{
		if (!particle || !particle->particleObject) {
			return;
		}

		RE::NiTransform anchorTransform = a_currentTransform;
		anchorTransform.translate = ComputeAnchorPosition(a_currentTransform);
		history.emplace_back(anchorTransform);

		auto* fadeNode = particle->particleObject->AsFadeNode();
		if (!fadeNode) {
			return;
		}

		if (!appliedColorSettings) {
			RE::BSVisit::TraverseScenegraphGeometries(fadeNode, ApplyColorSettings);
			appliedColorSettings = true;
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

		// Catmull-Rom necesita 4 muestras para interpolar una dirección real
		// (ver más abajo) -- hasta entonces no hay suficiente historial, y
		// sin esto los segmentos se quedaban sin tocar (visibles, en la pose
		// que trae el NIF de fábrica) durante los primeros ~3 ticks tras
		// aparecer la réplica: se veían "descuadrados" justo al salir de la
		// mano (reportado por el usuario). Se aparcan a escala 0 en la
		// posición actual, igual que el aparcado normal de segmentos sin
		// usar más abajo -- la orientación no importa a escala 0.
		if (history.size() < 4) {
			RE::NiTransform parkedTransform = history.back();
			parkedTransform.scale = 0.0f;
			ParkAllSegments(*trailRootNode, parkedTransform);
			currentTime += a_deltaSeconds;
			return;
		}

		// Tangente del recorrido: diferencia entre la muestra más antigua y
		// la más reciente de las 4 usadas para interpolar (ver comentario
		// al inicio del archivo -- no depende de ningún ángulo de la
		// réplica, a diferencia del original de Precision). Se calcula una
		// única vez por tick y se reutiliza para todos los segmentos que
		// toquen añadirse ahora, y para los que se dejan pegados a la
		// posición actual más abajo.
		const auto& p0 = (history.rbegin() + 3)->translate;
		const auto& p3 = history.back().translate;

		RE::NiPoint3 travelDir = p3 - p0;
		const float  travelLength = travelDir.Length();
		travelDir = travelLength > 0.0f ? travelDir / travelLength : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };

		// Math::SetRotationMatrix alinea el eje Y local del segmento con el
		// vector que se le pasa (comprobado: columna Y de la matriz
		// resultante = el vector dado, por construcción de la fórmula). El
		// eje Y del segmento va de tenue (origen) a intenso (creciente),
		// confirmado en NifSkope -- pero en el juego el resultado salía al
		// revés (intenso lejos del arma, tenue cerca, comprobado por el
		// usuario), así que se alinea con -travelDir en vez de travelDir
		// para invertirlo (un giro alrededor del propio eje, probado antes,
		// no podía arreglar esto -- no mueve nada a lo largo de ese mismo
		// eje). Ningún otro cálculo depende de este signo (travelDir en sí
		// no se usa para nada más).
		const auto segmentAxis = -travelDir;

		float      segmentsToAdd = segmentsToAddRemainder + a_deltaSeconds * static_cast<float>(Constants::kTrailSegmentsPerSecond);
		const auto segmentsToAddTrunc = static_cast<std::uint32_t>(segmentsToAdd);
		segmentsToAddRemainder = segmentsToAdd - static_cast<float>(segmentsToAddTrunc);

		// Recicla segmentos ya expirados (o fuerza el hueco necesario si
		// no caben todos los nuevos) antes de añadir ninguno.
		if (!segmentTimestamps.empty()) {
			std::uint32_t segmentsToMove = 0;
			for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
				if (i < segmentTimestamps.size() && currentTime > segmentTimestamps[i] + Constants::kTrailSegmentLifetime) {
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
			const auto timestampCount = static_cast<std::uint32_t>(segmentTimestamps.size());
			if (segmentsToMove > timestampCount) {
				segmentsToMove = timestampCount;
			}

			if (segmentsToMove > 0) {
				segmentTimestamps.erase(segmentTimestamps.begin(), segmentTimestamps.begin() + segmentsToMove);

				for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
					if (segmentCount > i + segmentsToMove) {
						segments[static_cast<std::uint16_t>(i)]->local = segments[static_cast<std::uint16_t>(i + segmentsToMove)]->local;
					}
				}

				currentBoneIdx -= segmentsToMove;
			}
		}

		// Añade los segmentos nuevos, interpolados con Catmull-Rom entre
		// las 4 últimas muestras del historial (misma dirección travelDir
		// para todos los que toquen en este tick -- normalmente 1-2, ver
		// Constants::kTrailSegmentsPerSecond, así que perder granularidad
		// de dirección dentro de un mismo tick es aceptable).
		if (segmentsToAdd > 0.0f) {
			auto p3It = history.rbegin();
			auto p2It = p3It + 1;
			auto p1It = p2It + 1;
			auto p0It = p1It + 1;

			const auto& ip0 = p0It->translate;
			const auto& ip1 = p1It->translate;
			const auto& ip2 = p2It->translate;
			const auto& ip3 = p3It->translate;

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

				RE::NiTransform newTransform = segmentBone->world;
				Math::SetRotationMatrix(newTransform.rotate, -segmentAxis.x, segmentAxis.y, segmentAxis.z);
				newTransform.translate = interpolatedPos;
				newTransform.scale = Constants::kTrailSegmentScale;

				segmentBone->local = GetLocalTransform(segmentBone.get(), newTransform);
				segmentBone->world = newTransform;

				segmentTimestamps.emplace_back(currentTime + a_deltaSeconds * t);
				++currentBoneIdx;
			}
		}

		// Estrecha cada segmento activo según su antigüedad (tamaño
		// completo recién añadido, hacia 0 según se acerca a
		// kTrailSegmentLifetime) -- con la malla repitiéndose igual en
		// cada hueso, escalar todos por igual (como antes) se veía como
		// una cinta de anchura uniforme con costuras marcadas; así el
		// conjunto forma un cono real que se va cerrando hacia la cola,
		// pedido explícitamente por el usuario tras ver el resultado
		// anterior. Recorre solo el tramo activo [0, currentBoneIdx) --
		// los aparcados más abajo no tienen antigüedad real todavía.
		for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
			if (i >= segmentTimestamps.size()) {
				break;
			}

			auto& segmentBone = segments[static_cast<std::uint16_t>(i)];
			if (!segmentBone) {
				continue;
			}

			float ageFraction = (currentTime - segmentTimestamps[i]) / Constants::kTrailSegmentLifetime;
			ageFraction = ageFraction < 0.0f ? 0.0f : (ageFraction > 1.0f ? 1.0f : ageFraction);

			const float taperedScale = Constants::kTrailSegmentScale * (1.0f - ageFraction);
			segmentBone->local.scale = taperedScale;
			segmentBone->world.scale = taperedScale;
		}

		// Los segmentos todavía sin usar se mantienen pegados a la
		// posición actual de la réplica pero a escala 0 (invisibles) --
		// antes se dejaban a tamaño completo, lo que además de no
		// representar ninguna posición histórica real, amontonaba varias
		// copias idénticas superpuestas justo en el punto de anclaje
		// (comprobado en el juego, contribuía al aspecto de "vela"
		// sólida de una ronda anterior).
		if (currentBoneIdx < segmentCount) {
			RE::NiTransform worldTransform = history.back();

			Math::SetRotationMatrix(worldTransform.rotate, -segmentAxis.x, segmentAxis.y, segmentAxis.z);
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
