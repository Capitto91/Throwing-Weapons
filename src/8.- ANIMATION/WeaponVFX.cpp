// Implementación del VFX de movimiento (chispas). Ver el header para el
// porqué del bucle de tick manual (tercer intento, tras BSTempEffectParticle
// y NiNode::AttachChild, ninguno de los dos renderizaba nada), y para el
// porqué de los dos Activators/.nif separados (continuo + de un solo uso)
// en vez de un único .nif con dos secuencias que se alternan.

#include "8.- ANIMATION/WeaponVFX.h"

#include "1.- CORE/Constants.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <atomic>
#include <thread>

namespace Animation
{
	namespace
	{
		// Mismo margen que Physics::SpawnReplica -- ~800ms de sobra para lo
		// que suele tardar el 3D de una referencia recién colocada en
		// cargar en segundo plano. Aquí sí tiene sentido un límite bajo: si
		// el 3D nunca llega a cargar es un fallo permanente real (el
		// modelo no existe, o algo similar), seguir reintentando para
		// siempre no serviría de nada.
		constexpr int kMax3DWaitAttempts = 50;

		// Resuelto una sola vez por sesión cada uno (el formulario no
		// cambia entre llamadas) -- FormID local ya enmascarado a 12 bits
		// (ver Constants::kMovementVfxActivatorLocalFormID/
		// kMovementVfxOffActivatorLocalFormID). Misma función para los dos
		// Activators -- cachés y FormID distintos, pasados por referencia.
		RE::TESBoundObject* GetCachedActivatorForm(RE::FormID a_localFormID, RE::TESBoundObject*& a_cache, bool& a_lookupDone)
		{
			if (!a_lookupDone) {
				a_lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					a_cache = dataHandler->LookupForm<RE::TESObjectACTI>(a_localFormID, Constants::kSoundPluginName);
				}
				if (!a_cache) {
					logs::warn("Animation::WeaponVFX: no se encontró el Activator (FormID local 0x{:03X}) en \"{}\".",
						a_localFormID, Constants::kSoundPluginName);
				}
			}
			return a_cache;
		}

		RE::TESBoundObject* GetOnActivatorForm()
		{
			static RE::TESBoundObject* cache = nullptr;
			static bool                lookupDone = false;
			return GetCachedActivatorForm(Constants::kMovementVfxActivatorLocalFormID, cache, lookupDone);
		}

		RE::TESBoundObject* GetOffActivatorForm()
		{
			static RE::TESBoundObject* cache = nullptr;
			static bool                lookupDone = false;
			return GetCachedActivatorForm(Constants::kMovementVfxOffActivatorLocalFormID, cache, lookupDone);
		}

		// VFX activo en este instante, si lo hay -- el continuo mientras el
		// arma se mueve, o el "de un solo uso" mientras se apaga (ver
		// FadeOutMovementVFX). Nunca los dos a la vez salvo el breve
		// solape de Constants::kMovementVfxSwapOverlapDuration (usado tanto
		// aquí como en StartOn, ver los dos), en cuyo caso el VFX saliente
		// se trackea aparte (oldHandle/oldTickToken/oldActivateToken,
		// capturados por valor en el propio punto de relevo) y ya no por
		// estas variables globales.
		RE::ObjectRefHandle g_activeVfxHandle;
		Physics::TickToken  g_tickToken;      // sigue la posición del objetivo (mano/réplica) -- solo el continuo la usa
		Physics::TickToken  g_activateToken;  // reintenta Activate() hasta que tenga éxito -- ver StartActivatingSequence

		// True si g_activeVfxHandle es el "de un solo uso" (ya apagándose
		// por su cuenta), false si es el continuo (o si no hay nada
		// activo). Guarda de reentrancia para FadeOutMovementVFX (ver ese
		// comentario) -- sin esto, dos llamadas seguidas mientras el
		// primer burst todavía no ha terminado colocaban un SEGUNDO burst
		// encima del primero (cada llamada no sabe que ya hay uno en
		// marcha), en vez de dejar que el primero termine solo: se veía
		// como una tanda de chispas tardía y desincronizada, o incluso
		// varias tandas encadenándose si algo seguía llamando de más
		// (bug reportado por el usuario, 2026-08-10).
		bool g_isBurstActive = false;

		// Incrementada en cada Start/Stop -- los reintentos de espera de 3D
		// (WaitFor3DThenStartTicking/WaitFor3DThenStartBurst) capturan la
		// generación vigente y la comprueban de nuevo antes de actuar,
		// para descartarse en silencio si un Start/Stop posterior ya
		// decidió el destino del VFX mientras dormían.
		std::atomic<std::uint64_t> g_generation{ 0 };

		// Devuelve el NiControllerManager del VFX colgado de a_handle, si
		// lo hay -- confirmado que cuelga directamente del nodo raíz
		// (BSFadeNode, ver NiObjectNET::controller) tanto en
		// ThorMjolnirSparks.nif como en ThorMjolnirSparksOff.nif, así que
		// GetController<T>() (recorre la cadena Next de controladores del
		// propio objeto, comparando RTTI) lo encuentra en el primer salto.
		RE::NiControllerManager* GetVfxControllerManager(RE::ObjectRefHandle a_handle)
		{
			auto  ref = a_handle.get();
			auto* root = ref ? ref->Get3D() : nullptr;
			return root ? root->GetController<RE::NiControllerManager>() : nullptr;
		}

		// Reintenta activar Constants::kMovementVfxSequenceName
		// (RE::NiControllerSequence::Activate, API real del SDK Gamebryo/
		// NetImmerse -- ver el header de WeaponVFX.h para el porqué de
		// esto en vez de la graph variable "fToggleBlend" usada hasta
		// v1.14.11) cada tick hasta que tenga éxito, reutilizando
		// Physics::StartTickLoop en vez de una cadena de
		// std::thread(...).detach() (usada hasta v1.14.17) -- ver
		// CHANGELOG.md v1.14.18 para el porqué del cambio (el patrón de
		// hilos independientes compitiendo por estado global ya había
		// causado una carrera real, confirmada con log). Con cada .nif
		// teniendo ahora una única secuencia (ver el header), ya no hace
		// falta ningún chequeo de "secuencia deseada" -- no hay ninguna
		// otra secuencia con la que competir dentro del mismo manager.
		//
		// Activate() no siempre devuelve true a la primera, y su valor de
		// retorno tampoco es señal fiable de si la secuencia está
		// realmente activa (diagnóstico 2026-08-10, ver CHANGELOG.md) --
		// por eso se comprueba Animating() directamente en vez de fiarse
		// solo de lo que devuelve la llamada.
		//
		// a_onActivated (opcional): invocado una sola vez, en el primer
		// tick en que se confirma Animating()==true (tanto si ya lo
		// estaba como si acaba de activarse ahora mismo) -- señal de "ya
		// se está reproduciendo de verdad", más fiable que cualquier
		// margen de tiempo fijo dado que la carga del 3D + el número de
		// reintentos de Activate() necesarios varía (a veces bastante,
		// ver CHANGELOG.md v1.14.13-v1.14.20). Usado por StartOn/
		// WaitFor3DThenStartBurst para saber el instante exacto en que ya
		// es seguro destruir lo que este VFX releva, sin esperar más de
		// la cuenta ni menos.
		void StartActivatingSequence(RE::ObjectRefHandle a_handle, std::function<void()> a_onActivated = {})
		{
			Physics::CancelTickLoop(g_activateToken);
			g_activateToken = Physics::StartTickLoop(a_handle, [onActivated = std::move(a_onActivated)](RE::TESObjectREFR& a_refr, float) {
				auto* manager = a_refr.Get3D() ? a_refr.Get3D()->GetController<RE::NiControllerManager>() : nullptr;
				auto* sequence = manager ? manager->GetSequenceByName(Constants::kMovementVfxSequenceName) : nullptr;

				if (sequence && sequence->Animating()) {
					if (onActivated) {
						onActivated();
					}
					return false;
				}

				const bool activated = sequence && sequence->Activate(0, false, 1.0f, 0.0f, nullptr, false);
				logs::info("Animation::WeaponVFX::StartActivatingSequence: manager={}, sequence={}, activated={}, Animating() tras el intento={}.",
					manager != nullptr, sequence != nullptr, activated, sequence ? sequence->Animating() : false);
				if (activated && onActivated) {
					onActivated();
				}
				return !activated;
			});
		}

		// Sigue cada tick la posición mundial actual de a_getTargetPosition
		// (el hueso "WEAPON", o el nodo raíz de la réplica -- reevaluado en
		// cada tick, no una foto fija del instante de arranque, para que el
		// VFX seguía moviéndose con su objetivo) -- mismo patrón de control
		// manual ya usado en todo el proyecto para la réplica del arma
		// (SetPosition + Physics::SyncHavok + Update3DPosition, ver
		// Physics::SpawnReplica/StartTickLoop). El propio bucle nunca se
		// autodetiene (siempre devuelve true) -- el único punto de parada
		// es StopMovementVFX, vía Physics::CancelTickLoop. Solo la usa el
		// Activator continuo -- el "de un solo uso" se queda quieto donde
		// se coloca, ver WaitFor3DThenStartBurst.
		void StartTicking(RE::ObjectRefHandle a_vfxHandle, std::function<RE::NiPoint3()> a_getTargetPosition, std::function<void()> a_onReady)
		{
			auto  vfxRef = a_vfxHandle.get();
			auto* node3D = vfxRef ? vfxRef->Get3D() : nullptr;
			if (!node3D) {
				return;
			}

			// Modo Havok "movido por código" -- mismo motivo que
			// Physics::SpawnReplica: sin esto, forzar la posición de un
			// cuerpo simulado activamente produce tirones/clipping.
			node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);

			StartActivatingSequence(a_vfxHandle, std::move(a_onReady));

			// g_activeVfxHandle NO se fija aquí -- StartOn ya lo hizo de
			// forma síncrona, en el mismo instante en que se colocó el
			// Activator (ver ese comentario, bug real confirmado en el
			// juego 2026-08-10). Fijarlo aquí, tras la espera asíncrona de
			// 3D, dejaba una ventana real donde g_activeVfxHandle seguía
			// apuntando al VFX anterior -- si algo llamaba a
			// FadeOutMovementVFX en ese hueco (comprobado: un lanzamiento
			// contra un objetivo muy cercano, clavado antes de que el 3D
			// del nuevo continuo llegara a cargar), relevaba el VFX
			// equivocado y el recién colocado se quedaba huérfano para
			// siempre -- nadie con su handle, nunca activado ni destruido.
			g_tickToken = Physics::StartTickLoop(a_vfxHandle, [getPos = std::move(a_getTargetPosition)](RE::TESObjectREFR& a_refr, float) {
				const auto pos = getPos();
				a_refr.SetPosition(pos);
				Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				return true;
			});

			logs::info("Animation::WeaponVFX: siguiendo por tick, posición inicial ({:.1f},{:.1f},{:.1f}).", node3D->world.translate.x, node3D->world.translate.y, node3D->world.translate.z);
		}

		void WaitFor3DThenStartTicking(RE::ObjectRefHandle a_vfxHandle, std::function<RE::NiPoint3()> a_getTargetPosition, int a_attemptsLeft, std::uint64_t a_generation, std::function<void()> a_onReady)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto vfxRef = a_vfxHandle.get();
			if (!vfxRef) {
				return;
			}

			if (vfxRef->Get3D()) {
				StartTicking(a_vfxHandle, std::move(a_getTargetPosition), std::move(a_onReady));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponVFX: el 3D del VFX nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_vfxHandle, getPos = std::move(a_getTargetPosition), a_attemptsLeft, a_generation, onReady = std::move(a_onReady)]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_vfxHandle, getPos = std::move(getPos), a_attemptsLeft, a_generation, onReady = std::move(onReady)]() mutable {
					WaitFor3DThenStartTicking(a_vfxHandle, std::move(getPos), a_attemptsLeft - 1, a_generation, std::move(onReady));
				});
			}).detach();
		}

		// Coordina la destrucción de un VFX "saliente" (a_oldHandle/
		// a_oldTickToken/a_oldActivateToken) frente al que lo releva,
		// combinando dos señales independientes en vez de fiarse de la
		// primera que llegue (2026-08-10, segunda ronda -- ver
		// Constants::kMovementVfxSwapOverlapDuration para el porqué
		// completo, el usuario seguía viendo un corte seco pese al diseño
		// "orientado a evento" de la primera ronda):
		//   1) Constants::kMovementVfxSwapOverlapDuration ya vencido --
		//      solape MÍNIMO garantizado, para que el ojo humano llegue a
		//      percibir de verdad los dos coexistiendo, no unos pocos
		//      ticks imperceptibles.
		//   2) el nuevo confirma que ya se está reproduciendo de verdad
		//      (Animating()==true) -- para no destruir el viejo antes de
		//      tiempo si la carga del 3D/los reintentos de Activate()
		//      tardan más de lo normal (bug real ya visto, ver
		//      CHANGELOG.md v1.14.27).
		// El viejo se destruye solo cuando se cumplen LAS DOS. Devuelve la
		// función a pasar como "onReady"/"a_onReady" al nuevo VFX
		// (WaitFor3DThenStartTicking/WaitFor3DThenStartBurst) -- señala la
		// mitad 2) del gate. Constants::kMovementVfxSwapSafetyTimeout
		// (más generosa, independiente del gate) fuerza la destrucción de
		// todas formas si la señal 2) nunca llega -- nunca deja el viejo
		// huérfano para siempre.
		std::function<void()> ScheduleOldVfxSwap(RE::ObjectRefHandle a_oldHandle, Physics::TickToken a_oldTickToken, Physics::TickToken a_oldActivateToken)
		{
			struct SwapGate
			{
				std::atomic<bool> minOverlapElapsed{ false };
				std::atomic<bool> newVfxReady{ false };
				std::atomic<bool> destroyed{ false };
			};
			auto gate = std::make_shared<SwapGate>();

			auto destroyOld = [a_oldHandle, a_oldTickToken, a_oldActivateToken, gate]() {
				bool expected = false;
				if (gate->destroyed.compare_exchange_strong(expected, true)) {
					Physics::CancelTickLoop(a_oldTickToken);
					Physics::CancelTickLoop(a_oldActivateToken);
					Physics::DestroyReplica(a_oldHandle);
				}
			};

			auto tryDestroy = [gate, destroyOld]() {
				if (gate->minOverlapElapsed.load() && gate->newVfxReady.load()) {
					destroyOld();
				}
			};

			std::thread([gate, tryDestroy]() {
				std::this_thread::sleep_for(Constants::kMovementVfxSwapOverlapDuration);
				SKSE::GetTaskInterface()->AddTask([gate, tryDestroy]() {
					gate->minOverlapElapsed.store(true);
					tryDestroy();
				});
			}).detach();

			std::thread([destroyOld]() {
				std::this_thread::sleep_for(Constants::kMovementVfxSwapSafetyTimeout);
				SKSE::GetTaskInterface()->AddTask([destroyOld]() {
					destroyOld();
				});
			}).detach();

			return [gate, tryDestroy]() {
				gate->newVfxReady.store(true);
				tryDestroy();
			};
		}

		// Común a StartMovementVFXOnActor/OnReplica: coloca a_form en
		// a_spawnAt y espera su 3D antes de arrancar el seguimiento por
		// tick sobre a_getTargetPosition.
		//
		// Solapa con el VFX que hubiera antes en vez de cortarlo primero y
		// colocar éste después -- a petición del usuario (2026-08-10): con
		// el corte-antes-que-coloca de antes, la transición kThrowing-
		// >kThrown (soltar el arma de la mano) se notaba, como un "empieza
		// otra vez" en vez de una continuación (el VFX enganchado a la
		// mano se destruía de inmediato en TransitionState, y el
		// enganchado a la réplica no se colocaba hasta que su handle
		// estuviera listo, un hueco real de por medio). Captura el
		// handle/tokens vigentes por valor antes de tocar nada: si no
		// había ningún VFX activo, son handles/tokens vacíos y las
		// llamadas de más abajo son no-ops inofensivos -- mismo patrón que
		// ya usa FadeOutMovementVFX para el relevo hacia el "de un solo
		// uso", aplicado aquí al relevo entre dos VFX que sí siguen algo
		// (mano/réplica).
		void StartOn(RE::TESObjectREFR& a_spawnAt, std::function<RE::NiPoint3()> a_getTargetPosition, RE::TESBoundObject* a_form)
		{
			if (!a_form) {
				return;
			}

			auto ref = a_spawnAt.PlaceObjectAtMe(a_form, false);
			if (!ref) {
				logs::warn("Animation::WeaponVFX: PlaceObjectAtMe devolvió nullptr.");
				return;
			}

			// Diagnóstico (2026-08-10, bug reportado por el usuario: chispas
			// que a veces no cesan nunca al probar con NPCs) -- FormID del
			// nuevo/antiguo Activator y generación, para poder correlacionar
			// en el log real qué instancia concreta se quedó sin un cierre
			// final la próxima vez que se reproduzca.
			logs::info("Animation::WeaponVFX::StartOn: nuevo continuo 0x{:08X}, antiguo 0x{:08X} (isBurst={}), generación previa {}.",
				ref->GetFormID(), g_activeVfxHandle.get() ? g_activeVfxHandle.get()->GetFormID() : 0,
				g_isBurstActive, g_generation.load());

			// Igual que la réplica del arma (Physics::SpawnReplica): sin
			// esto, el jugador podría activarlo/recogerlo con la tecla de
			// activar.
			ref->SetActivationBlocked(true);

			auto oldHandle = g_activeVfxHandle;
			auto oldTickToken = g_tickToken;
			auto oldActivateToken = g_activateToken;

			// Fija el nuevo handle como el activo YA, de forma síncrona --
			// bug real confirmado en el juego (2026-08-10, log de una
			// pelea contra un NPC a corta distancia): antes esto no pasaba
			// hasta que StartTicking corría, una vez el 3D del nuevo
			// Activator terminaba de cargar en segundo plano (asíncrono,
			// nunca en el mismo instante). En ese hueco, g_activeVfxHandle
			// seguía apuntando al VFX anterior -- un lanzamiento contra un
			// objetivo muy cercano podía clavarse antes de que el hueco se
			// cerrara, y FadeOutMovementVFX (llamada por onStuck) relevaba
			// el VFX equivocado (el anterior, ya de camino a destruirse de
			// todas formas) dejando el recién colocado sin nadie que lo
			// activara, apagara o destruyera nunca -- huérfano para
			// siempre en el mundo. g_tickToken/g_activateToken se limpian
			// a la vez -- todavía no hay ninguno propio (StartTicking los
			// arrancará en cuanto el 3D esté listo); los del VFX anterior
			// ya están a salvo en oldTickToken/oldActivateToken de arriba.
			g_activeVfxHandle = RE::ObjectRefHandle(ref.get());
			g_tickToken = {};
			g_activateToken = {};

			// A partir de aquí ya no estamos "apagándonos" -- StartOn
			// siempre coloca el continuo (ver FadeOutMovementVFX para el
			// otro caso).
			g_isBurstActive = false;

			// Ver ScheduleOldVfxSwap: el anterior se destruye solo cuando
			// pasa el solape mínimo garantizado Y el nuevo confirma que ya
			// se está reproduciendo de verdad -- no con la primera señal
			// que llegue.
			auto onReady = ScheduleOldVfxSwap(oldHandle, oldTickToken, oldActivateToken);

			const auto generation = ++g_generation;
			WaitFor3DThenStartTicking(RE::ObjectRefHandle(ref.get()), std::move(a_getTargetPosition), kMax3DWaitAttempts, generation, std::move(onReady));
		}

		// Reevaluado cada tick (no una foto fija) -- compartido por
		// StartMovementVFXOnActor y RetargetMovementVFXToActor. El propio
		// jugador es un singleton estable durante toda la sesión
		// (WeaponManager solo llama a estas funciones con RE::
		// PlayerCharacter::GetSingleton()).
		RE::NiPoint3 GetPlayerHandPosition()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* node = player ? player->GetNodeByName("WEAPON") : nullptr;
			return node ? node->world.translate : RE::NiPoint3{};
		}

		// Contraparte de WaitFor3DThenStartTicking para el Activator "de un
		// solo uso" (ver FadeOutMovementVFX): a diferencia del continuo, no
		// sigue nada cada tick -- se coloca una vez en a_position (fija,
		// capturada en el instante del relevo) y se queda ahí quieto. Su
		// propio apagado va horneado en el .nif (ver el header), así que
		// lo único que hace falta desde C++ es activar su única secuencia.
		void WaitFor3DThenStartBurst(RE::ObjectRefHandle a_handle, RE::NiPoint3 a_position, int a_attemptsLeft, std::uint64_t a_generation, std::function<void()> a_onReady)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto ref = a_handle.get();
			if (!ref) {
				return;
			}

			if (auto* node3D = ref->Get3D()) {
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
				ref->SetPosition(a_position);
				Physics::SyncHavok(*ref, a_position, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				StartActivatingSequence(a_handle, std::move(a_onReady));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponVFX: el 3D del VFX \"de un solo uso\" nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_handle, a_position, a_attemptsLeft, a_generation, onReady = std::move(a_onReady)]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_handle, a_position, a_attemptsLeft, a_generation, onReady = std::move(onReady)]() mutable {
					WaitFor3DThenStartBurst(a_handle, a_position, a_attemptsLeft - 1, a_generation, std::move(onReady));
				});
			}).detach();
		}
	}

	void StartMovementVFXOnActor(RE::Actor& a_actor)
	{
		auto* handNode = a_actor.GetNodeByName("WEAPON");
		if (!handNode) {
			logs::warn("Animation::StartMovementVFXOnActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		StartOn(a_actor, GetPlayerHandPosition, GetOnActivatorForm());
	}

	void RetargetMovementVFXToActor(RE::Actor& a_actor)
	{
		if (!g_activeVfxHandle) {
			return;
		}

		if (!a_actor.GetNodeByName("WEAPON")) {
			logs::warn("Animation::RetargetMovementVFXToActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		// Misma instancia de siempre (g_activeVfxHandle) -- solo se
		// cambia qué posición sigue el bucle de tick, sin recolocar el
		// Activator ni tocar su secuencia (ya en marcha). Cancelar el
		// bucle anterior antes de arrancar el nuevo es obligatorio -- sin
		// esto, los dos escribirían la posición cada tick a la vez (ver
		// Physics::TickToken).
		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = Physics::StartTickLoop(g_activeVfxHandle, [](RE::TESObjectREFR& a_refr, float) {
			const auto pos = GetPlayerHandPosition();
			a_refr.SetPosition(pos);
			Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
			return true;
		});
	}

	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle)
	{
		auto  replica = a_handle.get();
		auto* root = replica ? replica->Get3D() : nullptr;
		if (!replica || !root) {
			logs::warn("Animation::StartMovementVFXOnReplica: réplica sin 3D todavía.");
			return;
		}

		// "Auto-reparable": si la réplica del arma deja de existir (p. ej.
		// Weapon::WeaponManager::ReequipAndReset la destruye al reequipar
		// el arma real de verdad) mientras este VFX sigue enganchado a
		// ella, se queda congelado en su última posición válida conocida
		// en vez de saltar al origen del mundo (RE::NiPoint3{} por
		// defecto) -- mismo bug ya resuelto una vez con un mecanismo
		// distinto (v1.14.5, Animation::FreezeMovementVFX, eliminada en
		// v1.14.23/v1.14.24: ya no hace falta congelar nada a mano desde
		// fuera, este lambda se congela solo). Deliberado a petición del
		// usuario: el fundido del VFX ya no se dispara en el instante del
		// reequipado real, sino al final de la animación completa de
		// Atrape (ver Weapon::WeaponManager::FinishCatchAnimation) -- este
		// hueco, ahora más largo, es justo lo que este lambda tiene que
		// cubrir sin saltos.
		auto getPos = [handle = a_handle, lastPosition = root->world.translate]() mutable -> RE::NiPoint3 {
			auto replicaRef = handle.get();
			auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
			if (replicaRoot) {
				lastPosition = replicaRoot->world.translate;
			}
			return lastPosition;
		};

		StartOn(*replica, std::move(getPos), GetOnActivatorForm());
	}

	void StopMovementVFX()
	{
		++g_generation;

		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = {};
		Physics::CancelTickLoop(g_activateToken);
		g_activateToken = {};

		// Diagnóstico (2026-08-10, ver StartOn/FadeOutMovementVFX) -- este
		// es el único punto que de verdad limpia g_activeVfxHandle del
		// todo; si un VFX activo "no cesa nunca", el log real dirá si
		// StopMovementVFX llegó a ejecutarse alguna vez para su FormID o
		// no.
		if (g_activeVfxHandle) {
			logs::info("Animation::WeaponVFX::StopMovementVFX: destruyendo 0x{:08X} (isBurst={}).",
				g_activeVfxHandle.get() ? g_activeVfxHandle.get()->GetFormID() : 0, g_isBurstActive);
			Physics::DestroyReplica(g_activeVfxHandle);
			g_activeVfxHandle = {};
		}

		g_isBurstActive = false;
	}

	void FadeOutMovementVFX(bool a_extraSettleDelay)
	{
		if (!g_activeVfxHandle) {
			return;
		}

		// a_extraSettleDelay: no captura la posición todavía -- deja que
		// el VFX siga activo y siguiendo la mano (nada de lo de abajo se
		// toca) durante Constants::kCatchVfxSettleDelay más, y entonces
		// se reinvoca a sí misma para ejecutar el cuerpo real. Guardado
		// por generación, mismo patrón que el resto del archivo: si algo
		// más releva el VFX mientras tanto (p. ej. un lanzamiento nuevo
		// ya en marcha), este cierre diferido se descarta en silencio en
		// vez de fundir un VFX que ya no tiene nada que ver con el catch
		// que lo pidió.
		if (a_extraSettleDelay) {
			const auto generation = g_generation.load();
			std::thread([generation]() {
				std::this_thread::sleep_for(Constants::kCatchVfxSettleDelay);
				SKSE::GetTaskInterface()->AddTask([generation]() {
					if (g_generation.load() == generation) {
						FadeOutMovementVFX(false);
					}
				});
			}).detach();
			return;
		}

		// Reentrancia: ya se está apagando (el "de un solo uso" ya
		// colocado, apagándose solo según su propia curva) -- una segunda
		// llamada mientras tanto (p. ej. Weapon::WeaponManager::
		// RecallWeapon llamada sobre un arma que ya se había clavado y
		// empezado a apagarse) no debe colocar OTRO burst encima; deja que
		// el que ya está en marcha termine por su cuenta. Sin esto, cada
		// llamada de más apilaba un burst nuevo sobre el anterior -- se
		// veía como una tanda de chispas tardía y desincronizada (bug
		// reportado por el usuario, 2026-08-10).
		if (g_isBurstActive) {
			logs::info("Animation::WeaponVFX::FadeOutMovementVFX: reentrante, ya hay un burst en marcha (0x{:08X}) -- no-op.",
				g_activeVfxHandle.get() ? g_activeVfxHandle.get()->GetFormID() : 0);
			return;
		}

		// Congela el continuo donde esté en este instante -- bucle de tick
		// propio, independiente del que mueva la réplica del arma en sí.
		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = {};
		Physics::CancelTickLoop(g_activateToken);
		g_activateToken = {};

		auto  oldHandle = g_activeVfxHandle;
		auto  oldRef = oldHandle.get();
		auto* oldRoot = oldRef ? oldRef->Get3D() : nullptr;
		if (!oldRef || !oldRoot) {
			// Sin 3D no hay ni posición que darle al "de un solo uso" --
			// limpiar el continuo tal cual y salir, no queda nada que
			// hacer aquí.
			StopMovementVFX();
			return;
		}
		const RE::NiPoint3 position = oldRoot->world.translate;

		// Coloca el Activator "de un solo uso" en la misma posición donde
		// estaba el continuo -- nace ahora, antes de que el continuo se
		// destruya (ver más abajo), así que nunca hay un frame sin
		// ninguna partícula visible durante el relevo. Su propio apagado
		// va horneado en el .nif (ver el header) -- no hace falta tocar
		// nada más desde aquí una vez colocado y activado.
		auto burstRef = oldRef->PlaceObjectAtMe(GetOffActivatorForm(), false);
		if (!burstRef) {
			logs::warn("Animation::FadeOutMovementVFX: PlaceObjectAtMe del \"de un solo uso\" devolvió nullptr -- corte inmediato de reserva.");
			StopMovementVFX();
			return;
		}
		burstRef->SetActivationBlocked(true);

		const auto generation = ++g_generation;
		g_activeVfxHandle = RE::ObjectRefHandle(burstRef.get());
		g_isBurstActive = true;

		logs::info("Animation::WeaponVFX::FadeOutMovementVFX: burst 0x{:08X} colocado (releva a continuo 0x{:08X}), generación {}.",
			burstRef->GetFormID(), oldRef->GetFormID(), generation);

		// Ver ScheduleOldVfxSwap: el continuo se destruye solo cuando pasa
		// el solape mínimo garantizado Y el burst confirma que ya se está
		// reproduciendo de verdad -- no con la primera señal que llegue
		// (independiente de g_activeVfxHandle, que a partir de aquí ya
		// apunta al burst, y de la generación -- si mientras tanto arranca
		// un VFX completamente nuevo, StopMovementVFX ya se encarga de
		// destruir lo que sea que sea g_activeVfxHandle en ese momento;
		// esta limpieza del continuo es independiente y no debe
		// descartarse por eso, o se quedaría huérfano en el mundo). Los
		// tokens de tick/activación del continuo ya se cancelaron arriba
		// (congelado en su sitio), así que aquí solo hace falta el handle.
		auto onReady = ScheduleOldVfxSwap(oldHandle, {}, {});

		WaitFor3DThenStartBurst(g_activeVfxHandle, position, kMax3DWaitAttempts, generation, std::move(onReady));

		// Destruye el "de un solo uso" de verdad pasado
		// Constants::kMovementVfxFadeOutSafetyMargin (cubre su ciclo
		// completo -- ver ese comentario). Guardado por generación, mismo
		// patrón que WaitFor3DThenStartTicking: si otro Start/Stop corre
		// antes de que venza el margen, este cierre se descarta en
		// silencio.
		std::thread([generation]() {
			std::this_thread::sleep_for(Constants::kMovementVfxFadeOutSafetyMargin);
			SKSE::GetTaskInterface()->AddTask([generation]() {
				if (g_generation.load() == generation) {
					StopMovementVFX();
				}
			});
		}).detach();
	}
}
