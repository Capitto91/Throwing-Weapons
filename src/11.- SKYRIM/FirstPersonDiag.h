// Diagnóstico temporal de primera persona: vuelca al log la cámara y el hueso "WEAPON" de cada esqueleto
// del jugador. Lo llaman WeaponState (cambios de estado), FrameHook (cambios de cámara) y OARFunctions.

#pragma once

namespace Diag
{
	// Vuelca cámara y huesos de la mano de los dos esqueletos del jugador, con a_reason como contexto.
	void DumpHands(std::string_view a_reason);

	// Vuelca los huesos si la cámara ha cambiado desde la última llamada. Lo llama FrameHook en cada fotograma.
	void PollCamera();

	// Vuelca el apuntado con retícula: orientación de la cámara, primer impacto sin filtrar del rayo y punto de mira
	// usado (a_aimPoint, a_hitUsed). Lo llama Throw al calcular la dirección sin target lock.
	void DumpAim(RE::Actor* a_shooter, const RE::NiPoint3& a_origin, const RE::NiPoint3& a_cameraPos, const RE::NiPoint3& a_forward, const RE::NiPoint3& a_aimPoint, bool a_hitUsed);

	// Vuelca cada efecto del jugador con el arte a_art o el shader a_shader: edad, vida, terminado, esqueleto y
	// partículas vivas. Lo llama LightningDash cada 0,25 s durante 5 s desde el inicio del dash.
	void DumpDashEffects(RE::BGSArtObject* a_art, RE::TESEffectShader* a_shader, float a_secondsSinceStart);

	// Traza de a_seconds (amplía la que esté en curso): todos los eventos de animación de los grafos del jugador y,
	// cuando cambia, ataque, arma, mano derecha y hueso WEAPON. La arrancan Lanzar y Lightning Dash.
	void StartTrace(std::string_view a_reason, float a_seconds);

	// Apunta en la traza en curso una acción propia sobre el grafo o el equipo (a_what), con su origen a_who.
	void NoteSent(std::string_view a_what, std::string_view a_who);

	// Caja negra: guarda a_text (sin escribirlo) entre las últimas entradas que se vuelcan si el arma se suelta.
	void Record(std::string a_text);

	// Vigila cada fotograma el hueso WEAPON; si el arma deja de colgar de la mano estando equipada y desenvainada,
	// vuelca al log la caja negra (eventos de animación, acciones propias y estados). Lo llama FrameHook.
	void PollWeaponAttach();
}
