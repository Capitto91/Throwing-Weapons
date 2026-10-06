// Golpe de cámara al atrapar: la cámara sube de golpe y rebota como un muelle, cada vez menos, hasta pararse.
// Se suma a la rotación de FirstPersonState/ThirdPersonState (hooks por la vtable); lo arranca WeaponManager.

#pragma once

namespace Animation::CameraKick
{
	// Instala los hooks de GetRotation; false en VR (posición sin verificar). Lo llama Plugin::Init.
	bool Install();

	// Arranca o reinicia el golpe: ángulo del primer impulso hacia arriba, tiempo hasta apagarse y rebotes por
	// segundo. false sin hooks (VR): el llamante usa RE::ShakeCamera. Seguro desde cualquier hilo.
	bool Start(float a_angleDegrees, float a_durationSeconds, float a_bouncesPerSecond);

	// Inclinación hacia arriba (radianes) sumada a la cámara en la última lectura; 0 sin golpe.
	// La descuenta Throw para apuntar en tercera persona con la cámara en reposo.
	[[nodiscard]] float GetAppliedPitch() noexcept;
}
