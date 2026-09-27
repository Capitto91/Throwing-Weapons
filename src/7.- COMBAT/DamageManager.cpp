// Implementación del sistema de daño.
// Calcula y aplica los efectos de impacto sobre actores.

#include "7.- COMBAT/DamageManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/GameOffsets.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "3.- WEAPON/WeaponManager.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <cmath>
#include <optional>

namespace Combat
{
	namespace
	{
		// Función nativa del motor que procesa un golpe ya calculado (ver
		// GameOffsets::ResolveProcessHit para de dónde sale y por qué no
		// es un ID de Address Library). Se resuelve una vez en Init;
		// nullptr en VR o si el sitio del call no tiene la forma esperada,
		// y entonces se usa el camino anterior (ApplyDamage + NotifyHit).
		GameOffsets::tProcessHit g_processHit = nullptr;

		void ApplyDamage(RE::Actor* a_target, float a_amount)
		{
			// Actor hereda ActorValueOwner (de donde viene
			// DamageActorValue) como una base más de una herencia
			// múltiple, no la primera — su offset dentro de Actor cambia
			// entre versiones del juego (0xB0 antes de 1.6.629, 0xB8 en
			// AE/posteriores, ver Actor.h). Acceder a DamageActorValue
			// directamente sobre un Actor* usa el offset fijo que decida
			// el compilador para este build multi-runtime, que no tiene
			// por qué coincidir con la versión real del juego — crasheaba
			// el juego (comprobado). Actor::AsActorValueOwner() es el
			// accessor de commonlibsse-ng que calcula el offset correcto
			// según la versión real detectada en tiempo de ejecución.
			auto* avOwner = a_target->AsActorValueOwner();
			if (avOwner) {
				avOwner->DamageActorValue(RE::ActorValue::kHealth, a_amount);
			}
		}

		// Camino de respaldo (solo sin g_processHit: VR, o sitio del call
		// irreconocible). Actor::HandleHealthDamage + Actor::SetBeenAttacked
		// (probado en el juego, ver CHANGELOG) no bastan por sí solos para
		// que la IA reaccione (perseguir, aggro) ni para que un aliado lo
		// tome como agresión. GameOffsets::DealDamage (Actor::CombatHit) sí
		// -- pero calcula su propio daño a partir del arma que el atacante
		// tenga equipada en ese instante, y durante todo nuestro ciclo la
		// mano va vacía (arma real oculta, punto 2), así que ese importe es
		// daño de puñetazo. Se usa aquí solo por su efecto colateral (el
		// aviso a IA de combate/crimen), revirtiendo lo que le haya hecho a
		// la vida; el importe real ya lo aplicó ApplyDamage.
		void NotifyHit(RE::Actor* a_attacker, RE::Actor* a_target, float a_amount)
		{
			auto* avOwner = a_target->AsActorValueOwner();

			if (REL::Module::IsVR()) {
				a_target->SetBeenAttacked(true);
				a_target->HandleHealthDamage(a_attacker, a_amount);
				logs::info("Combat::NotifyHit (VR, sin DealDamage verificado): \"{}\" avisado.", a_target->GetName());
				return;
			}

			const float before = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;

			GameOffsets::DealDamage(a_attacker, a_target, nullptr, false);

			const float afterDealDamage = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;
			const float vanillaDelta = before - afterDealDamage;
			if (avOwner && vanillaDelta > 0.0f) {
				avOwner->RestoreActorValue(RE::ActorValue::kHealth, vanillaDelta);
			}
		}

		// Entrada REAL del arma en el inventario del atacante (el arma
		// nunca sale del inventario durante el ciclo, solo se desequipa):
		// conserva sus extraLists (mejora de forja, encantamiento aplicado
		// por el jugador), que HitData::Populate lee para el daño. nullptr
		// si no se encuentra (el llamante usa entonces una entrada temporal
		// sin extras).
		RE::InventoryEntryData* FindInventoryEntry(RE::Actor* a_owner, RE::TESBoundObject* a_object)
		{
			auto* changes = a_owner ? a_owner->GetInventoryChanges() : nullptr;
			if (!changes || !changes->entryList) {
				return nullptr;
			}

			for (auto* entry : *changes->entryList) {
				if (entry && entry->object == a_object) {
					return entry;
				}
			}
			return nullptr;
		}

		// Golpe con el daño real del arma lanzada, escalado por a_mult
		// (Settings: 75% en el golpe inicial, 25% en cada golpe del
		// regreso). HitData::Populate calcula el daño como un golpe
		// cuerpo a cuerpo normal con esa arma (perks, armadura del
		// objetivo, sigilo, críticos), y la función nativa de procesar
		// golpe lo aplica con todo el pipeline del motor (reacción de
		// golpe, aviso a IA, muerte con autoría). El stagger que calcule el
		// motor se anula (hitData.stagger = 0): el del regreso lo garantiza
		// el nuestro propio (ApplyReturnHit), y en el golpe inicial el
		// objetivo queda paralizado -- sin esto podría tambalearse dos
		// veces (decisión del usuario 2026-09-27).
		void ApplyWeaponHit(RE::Actor* a_attacker, RE::Actor* a_target, float a_mult, const RE::NiPoint3& a_hitPosition)
		{
			auto* manager = Weapon::WeaponManager::GetSingleton();
			auto* weapon = manager ? manager->GetActiveWeapon() : nullptr;
			if (!weapon) {
				logs::warn("Combat::ApplyWeaponHit: no hay arma activa en el ciclo, golpe sin daño.");
				return;
			}

			std::optional<RE::InventoryEntryData> tempEntry;
			auto*                                 entry = FindInventoryEntry(a_attacker, weapon);
			if (!entry) {
				tempEntry.emplace(weapon, 1);
				entry = std::addressof(*tempEntry);
			}

			auto* hitData = RE::HitData::Create(a_attacker, a_target, entry, false);
			if (!hitData) {
				logs::warn("Combat::ApplyWeaponHit: HitData::Create devolvió nullptr.");
				return;
			}

			const float fullDamage = hitData->totalDamage;
			hitData->totalDamage = fullDamage * a_mult;
			hitData->stagger = 0.0f;
			hitData->hitPosition = a_hitPosition;
			auto direction = a_target->GetPosition() - a_attacker->GetPosition();
			if (direction.Length() > 0.0f) {
				direction.Unitize();
			}
			hitData->hitDirection = direction;

			auto*       avOwner = a_target->AsActorValueOwner();
			const float before = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;

			if (g_processHit) {
				g_processHit(a_target, *hitData);
			} else {
				ApplyDamage(a_target, hitData->totalDamage);
				NotifyHit(a_attacker, a_target, hitData->totalDamage);
			}

			const float after = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;
			logs::info(
				"Combat::ApplyWeaponHit: \"{}\" daño arma {:.1f} x {:.2f} = {:.1f} ({}) -> vida {:.1f} -> {:.1f}.",
				a_target->GetName(), fullDamage, a_mult, fullDamage * a_mult,
				g_processHit ? "pipeline nativo" : "respaldo", before, after);

			hitData->~HitData();
			RE::free(hitData);
		}

		RE::BGSHazard* LookupHazardForm(RE::FormID a_localFormID)
		{
			RE::BGSHazard* form = nullptr;
			if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
				form = dataHandler->LookupForm<RE::BGSHazard>(a_localFormID, Constants::kSoundPluginName);
			}
			if (!form) {
				logs::warn("Combat::LookupHazardForm: no se encontró el BGSHazard (FormID local 0x{:03X}) en \"{}\".",
					a_localFormID, Constants::kSoundPluginName);
			}
			return form;
		}

		// Hazards eléctricos propios resueltos una sola vez por sesión
		// (static local, mismo patrón que GetImpactExplosionForm en
		// WeaponImpactVFX.cpp).
		RE::BGSHazard* GetActorHazardForm()
		{
			static RE::BGSHazard* cache = LookupHazardForm(Constants::kEmbeddedHazardLocalFormID);
			return cache;
		}

		RE::BGSHazard* GetSurfaceHazardForm()
		{
			static RE::BGSHazard* cache = LookupHazardForm(Constants::kSurfaceHazardLocalFormID);
			return cache;
		}

		// Hazard del impacto actual, para quitarlo al desclavar el arma
		// (RemoveImpactHazard). Solo hay un ciclo de lanzamiento a la vez,
		// así que basta uno. g_hazardGeneration cubre la carrera con la
		// colocación diferida un tick (Throw::LaunchWeapon): si se desclava
		// antes de que llegue a colocarse, la generación ya no coincide y
		// no se coloca. Todo en el hilo principal.
		RE::ObjectRefHandle g_activeHazard;
		std::uint32_t       g_hazardGeneration = 0;

		// Coloca a_form sobre a_anchor y lo registra como hazard activo.
		// ownerActor = atacante para atribuirle el daño (campo de
		// commonlibsse-ng, accessor versionado GetHazardRuntimeData; sin
		// confirmar en el juego que el motor lo use para la autoría).
		RE::TESObjectREFR* PlaceHazard(RE::BGSHazard* a_form, RE::Actor* a_attacker, RE::TESObjectREFR& a_anchor, std::uint32_t a_generation)
		{
			if (!a_form) {
				return nullptr;
			}
			if (a_generation != g_hazardGeneration) {
				logs::info("Combat::PlaceHazard: el arma ya se desclavó antes de colocarlo, se omite.");
				return nullptr;
			}

			auto ref = a_anchor.PlaceObjectAtMe(a_form, false);
			if (!ref) {
				logs::warn("Combat::PlaceHazard: PlaceObjectAtMe devolvió nullptr.");
				return nullptr;
			}

			if (auto* hazard = ref->As<RE::Hazard>()) {
				hazard->GetHazardRuntimeData().ownerActor = RE::ActorHandle(a_attacker);
			} else {
				logs::warn("Combat::PlaceHazard: la referencia colocada no es un RE::Hazard, sin dueño asignado.");
			}

			if (auto previous = g_activeHazard.get()) {
				previous->Disable();
				previous->SetDelete(true);
			}
			g_activeHazard = ref->CreateRefHandle();
			return ref.get();
		}

		// Formularios resueltos por EditorID una sola vez, no en cada golpe/
		// recuperación (punto 6 de la revisión de buenas prácticas,
		// 2026-09-23) -- ninguno de los dos cambia de FormID durante una
		// sesión, así que repetir la búsqueda por cadena cada vez es trabajo
		// de sobra. static local (mismo patrón Meyers ya usado en los
		// singletons de este proyecto, ver WeaponManager::GetSingleton()):
		// se resuelve solo la primera vez que hace falta de verdad, sin
		// depender de enganchar esto a kDataLoaded aparte. El aviso de log
		// se mantiene fuera del static, así que sigue avisando en cada
		// llamada mientras el formulario no aparezca -- solo se ahorra la
		// búsqueda en sí, no el aviso de que falta.
		RE::SpellItem* GetEmbeddedParalysisSpell()
		{
			static RE::SpellItem* spell = RE::TESForm::LookupByEditorID<RE::SpellItem>(Constants::kEmbeddedParalysisSpell);
			if (!spell) {
				logs::warn("Combat::GetEmbeddedParalysisSpell: no se encontró el hechizo \"{}\" (revisa que exista en la Creation Kit).", Constants::kEmbeddedParalysisSpell);
			}
			return spell;
		}

		RE::EffectSetting* GetEmbeddedParalysisEffect()
		{
			static RE::EffectSetting* effect = RE::TESForm::LookupByEditorID<RE::EffectSetting>(Constants::kEmbeddedParalysisEffect);
			if (!effect) {
				logs::warn("Combat::GetEmbeddedParalysisEffect: no se encontró el efecto \"{}\" (revisa que exista en la Creation Kit).", Constants::kEmbeddedParalysisEffect);
			}
			return effect;
		}
	}

	void Init()
	{
		g_processHit = GameOffsets::ResolveProcessHit();
		if (g_processHit) {
			logs::info("Combat::Init: función nativa de procesar golpe resuelta, se usa el pipeline de combate real.");
		} else {
			logs::warn("Combat::Init: no se pudo resolver la función nativa de procesar golpe (VR, o sitio del call inesperado), se usa el camino de respaldo.");
		}
	}

	void BeginEmbeddedEffect(
		RE::Actor*                              a_attacker,
		RE::Actor*                              a_target,
		RE::ObjectRefHandle                     a_replicaHandle,
		std::function<void(RE::ActorHandle)>    a_onStuck,
		std::function<void()>                   a_onAutoRecall,
		std::function<void(Physics::TickToken)> a_onTickStarted)
	{
		if (!a_attacker || !a_target) {
			return;
		}

		// Posición de la réplica en el instante del impacto (punto de
		// golpe para el motor); respaldo, la del objetivo.
		auto               impactReplica = a_replicaHandle.get();
		const RE::NiPoint3 hitPosition = impactReplica ? impactReplica->GetPosition() : a_target->GetPosition();
		ApplyWeaponHit(a_attacker, a_target, Settings::GetThrowHitMult(), hitPosition);

		// Quién es inmune (dragones, criaturas concretas...) lo decide
		// solo la condición del propio efecto en la Creation Kit — no se
		// duplica esa lógica aquí. El sondeo de más abajo
		// (MagicTarget::HasMagicEffect) ya cubre cualquier caso de
		// inmunidad de forma genérica, así que cambiar quién es inmune
		// solo requiere tocar la condición en el CK, nunca este código.
		a_onStuck(RE::ActorHandle(a_target));

		if (auto* spell = GetEmbeddedParalysisSpell()) {
			a_target->AddSpell(spell);
		}

		// El propio efecto mágico (EffectSetting) dentro del hechizo,
		// distinto del hechizo en sí — hace falta para comprobar más
		// abajo, con MagicTarget::HasMagicEffect, si de verdad ha quedado
		// activo en el objetivo (AddSpell siempre tiene éxito aunque la
		// condición del efecto se lo impida, ver Constants::kEmbeddedParalysisEffect).
		auto* paralysisEffect = GetEmbeddedParalysisEffect();

		// Desplazamiento respecto al hueso más cercano al punto de impacto
		// (ActorUtils::FindNearestBoneName) en el instante del impacto,
		// guardado en su espacio LOCAL (girado con su rotación de ese
		// momento) en vez de un vector fijo en espacio del mundo — un
		// vector fijo se notaba flotando lejos del cuerpo en cuanto el
		// actor caía por la parálisis (comprobado en el juego): la
		// orientación cambia por completo al caer, y un desplazamiento en
		// espacio del mundo no la sigue. Cada tick se reconvierte a
		// espacio del mundo con la rotación actual de ESE hueso (ver más
		// abajo), así que el arma sigue el movimiento real del cuerpo
		// (respiración, tembleque de la parálisis, reacciones de golpe) en
		// vez de solo el del nodo raíz, que antes se notaba flotando —
		// comprobado en el juego. Se guarda el *nombre* del hueso, no un
		// puntero crudo, y se resuelve de nuevo cada tick (ver más abajo),
		// mismo motivo de cautela que ya había para el nodo raíz: el 3D
		// podría recargarse. Si no se encuentra ningún hueso (actor sin
		// 3D), se cae al nodo raíz — mismo comportamiento que antes.
		auto                    replica = a_replicaHandle.get();
		const RE::BSFixedString boneName = replica ? ActorUtils::FindNearestBoneName(a_target, replica->GetPosition()) : RE::BSFixedString{};
		auto*                   rootNode = a_target->Get3D();
		auto*                   trackedNode = (!boneName.empty() && rootNode) ? rootNode->GetObjectByName(boneName) : rootNode;
		RE::NiPoint3            localOffset{};
		if (replica && trackedNode) {
			localOffset = trackedNode->world.rotate.Transpose() * (replica->GetPosition() - trackedNode->world.translate);
		}
		RE::ActorHandle targetHandle(a_target);

		// Punto 10 (segunda mitad, caso impacto): eliminado el enderezado
		// al clavarse (decisión del usuario, 2026-08-08, ver
		// Constants::kSpinStraightenLeadTime para el porqué) -- el arma se
		// queda congelada en el ángulo de vuelo arbitrario que tuviera al
		// golpear, sin ningún ajuste posterior.
		auto token = Physics::StartTickLoop(a_replicaHandle, [targetHandle, localOffset, boneName, paralysisEffect, onAutoRecall = a_onAutoRecall, totalElapsed = 0.0f, effectConfirmed = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			auto target = targetHandle.get();
			if (!target) {
				// El actor ya no existe (p. ej. la celda se ha
				// descargado); la réplica se queda donde estaba, sigue
				// pudiendo recuperarse con el botón.
				return false;
			}

			auto* currentRoot = target->Get3D();
			auto* currentNode = (!boneName.empty() && currentRoot) ? currentRoot->GetObjectByName(boneName) : currentRoot;
			if (!currentNode) {
				currentNode = currentRoot;
			}
			const auto nextPos = currentNode ?
			                         currentNode->world.translate + currentNode->world.rotate * localOffset :
			                         target->GetPosition();
			a_refr.SetPosition(nextPos);
			Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());

			totalElapsed += a_deltaSeconds;

			// Comprobación exacta (no una inferencia): MagicTarget no es
			// la primera clase base de Actor tampoco, así que se usa
			// Actor::AsMagicTarget() (accessor versionado, mismo motivo
			// que ActorValueOwner/ActorState) para preguntar directamente
			// si nuestro efecto concreto está activo de verdad. AddSpell
			// siempre tiene éxito aunque la condición del propio efecto
			// (inmune a parálisis, dragón...) le impida aplicarse — el
			// motor necesita al menos un tick para reflejarlo, así que se
			// comprueba cada tick hasta confirmarse o agotar el margen.
			if (!effectConfirmed) {
				auto* magicTarget = target->AsMagicTarget();
				if (paralysisEffect && magicTarget && magicTarget->HasMagicEffect(paralysisEffect)) {
					effectConfirmed = true;
				} else if (totalElapsed >= Constants::kImmunityCheckDelay) {
					logs::info("Combat: el objetivo no se paralizó (inmune, o el efecto no se encontró), recuperando automáticamente.");
					onAutoRecall();
					return false;
				}
			}

			// Nerfeo pedido tras las primeras pruebas: duración máxima
			// clavada, pasado ese tiempo el arma vuelve sola.
			if (totalElapsed >= Constants::kEmbeddedMaxDuration) {
				logs::info("Combat: duración máxima clavada alcanzada, recuperando automáticamente.");
				onAutoRecall();
				return false;
			}

			return true;
		});

		a_onTickStarted(token);
	}

	std::uint32_t GetHazardGeneration()
	{
		return g_hazardGeneration;
	}

	void SpawnActorHazard(RE::Actor* a_attacker, RE::Actor& a_target, std::uint32_t a_generation)
	{
		// Desactivable desde [Damage] HazardOnActor (Settings). Sin hazard
		// no hay daño continuo mientras el arma sigue clavada: solo el
		// golpe inicial.
		if (!Settings::GetHazardOnActor()) {
			logs::info("Combat::SpawnActorHazard: desactivado en la configuración (HazardOnActor), no se coloca.");
			return;
		}

		if (auto* ref = PlaceHazard(GetActorHazardForm(), a_attacker, a_target, a_generation)) {
			const auto pos = ref->GetPosition();
			logs::info("Combat::SpawnActorHazard: hazard colocado sobre \"{}\" en ({:.1f},{:.1f},{:.1f}).",
				a_target.GetName(), pos.x, pos.y, pos.z);
		}
	}

	void SpawnSurfaceHazard(RE::Actor* a_attacker, RE::TESObjectREFR& a_anchor, const RE::NiPoint3& a_point, const RE::NiPoint3& a_normal, std::uint32_t a_generation)
	{
		// Desactivable desde [Damage] HazardOnSurface (Settings).
		if (!Settings::GetHazardOnSurface()) {
			logs::info("Combat::SpawnSurfaceHazard: desactivado en la configuración (HazardOnSurface), no se coloca.");
			return;
		}

		auto* ref = PlaceHazard(GetSurfaceHazardForm(), a_attacker, a_anchor, a_generation);
		if (!ref) {
			return;
		}

		// Eje Z local del hazard sobre la normal. Convención de ángulos de
		// una referencia: angle.z = rumbo (0 = +Y, creciente hacia +X),
		// angle.x = cabeceo positivo hacia abajo del eje "adelante" --
		// que, para el eje "arriba", es inclinarlo hacia delante: arriba =
		// (sin z·sin x, cos z·sin x, cos x). Despejando para arriba = n:
		// x = acos(n.z), z = atan2(n.x, n.y). Suelo (n = +Z) -> sin giro.
		// Suposición sin verificar: que la malla del hazard
		// (ShockWallFX01.nif) está pensada con Z como "arriba".
		RE::NiPoint3 n = a_normal;
		if (n.Length() < 0.001f) {
			n = { 0.0f, 0.0f, 1.0f };
		} else {
			n.Unitize();
		}
		const float nz = n.z > 1.0f ? 1.0f : (n.z < -1.0f ? -1.0f : n.z);
		const RE::NiPoint3 angle{ std::acos(nz), 0.0f, std::atan2(n.x, n.y) };

		ref->SetPosition(a_point);
		ref->SetAngle(angle);

		logs::info("Combat::SpawnSurfaceHazard: hazard en ({:.1f},{:.1f},{:.1f}), normal ({:.2f},{:.2f},{:.2f}), ángulo x={:.2f} z={:.2f} rad.",
			a_point.x, a_point.y, a_point.z, n.x, n.y, n.z, angle.x, angle.z);
	}

	void RemoveImpactHazard()
	{
		++g_hazardGeneration;
		if (auto hazard = g_activeHazard.get()) {
			hazard->Disable();
			hazard->SetDelete(true);
			logs::info("Combat::RemoveImpactHazard: hazard retirado al desclavar el arma.");
		}
		g_activeHazard = {};
	}

	void EndEmbeddedEffect(RE::Actor* a_target)
	{
		if (!a_target) {
			return;
		}

		if (auto* spell = GetEmbeddedParalysisSpell()) {
			a_target->RemoveSpell(spell);
		}
	}

	void ApplyReturnHit(RE::Actor* a_attacker, RE::Actor* a_target, const RE::NiPoint3& a_hitPosition)
	{
		if (!a_attacker || !a_target) {
			return;
		}

		logs::info("Combat::ApplyReturnHit: golpe durante el regreso contra \"{}\".", a_target->GetName());

		// El golpe se aplica siempre, aunque el multiplicador sea 0 (INI
		// [Damage] ReturnHitMultiplier): la reacción del objetivo
		// (perseguir, o que un aliado se lo tome como agresión) no debe
		// depender de esa opción, solo de que hubo un golpe de verdad.
		ApplyWeaponHit(a_attacker, a_target, Settings::GetReturnHitMult(), a_hitPosition);

		// Desactivable desde [Damage] ReturnStagger (Settings): el golpe
		// sigue aplicándose, solo se omite el tambaleo.
		if (!Settings::GetReturnStagger()) {
			logs::info("Combat::ApplyReturnHit: stagger desactivado en la configuración (ReturnStagger).");
			return;
		}

		// Mejora Kratos #2 (PLAN-mejoras-kratos.md): stagger escrito
		// directamente en el animation graph del actor golpeado, en vez de
		// concederle un hechizo propio y retirarlo con un hilo (mecanismo
		// anterior). SetGraphVariableFloat/NotifyAnimationGraph existen en
		// IAnimationGraphManagerHolder (verificado,
		// commonlibsse-ng/include/RE/I/IAnimationGraphManagerHolder.h) y
		// Actor los hereda a través de TESObjectREFR (primera base de
		// Actor, offset 0) con IAnimationGraphManagerHolder a offset fijo
		// 0x38 dentro de TESObjectREFR (sin variación por runtime, a
		// diferencia de ActorValueOwner/MagicTarget) — llamable
		// directamente sobre Actor* sin ningún accessor AsX(). Los nombres
		// "staggerMagnitude"/"staggerDirection" están pre-registrados como
		// BSFixedString propias del motor (FixedStrings.h), y
		// "staggerStart" es el evento real que usa KratosCombat
		// (FenixUtils::stagger, ver PLAN-proyectil-nativo.md) para el mismo
		// propósito sobre su propia arma.
		a_target->SetGraphVariableFloat("staggerMagnitude", Constants::kStaggerMagnitude);
		a_target->SetGraphVariableFloat("staggerDirection", 0.0f);  // placeholder, "de frente" -- sin verificar unidades/rango real
		a_target->NotifyAnimationGraph("staggerStart");
		logs::info(
			"Combat::ApplyReturnHit: stagger vía animation graph, magnitude={:.1f}, direction=0.0.",
			Constants::kStaggerMagnitude);
	}
}
