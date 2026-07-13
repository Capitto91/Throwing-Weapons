// Herramientas matemáticas para rotaciones.
// Calcula orientaciones, direcciones y alineaciones del arma.

#pragma once

namespace Math
{
	// Convierte grados a radianes, para las constantes en grados de
	// Constants.h (más legibles) que las funciones de rotación esperan en
	// radianes.
	[[nodiscard]] constexpr float DegreesToRadians(float a_degrees) noexcept
	{
		return a_degrees * 0.017453292519943295f;
	}

	// Ángulos de cabeceo/guiñada (convención Z-X-Y de Skyrim, ver
	// TESObjectREFR::SetAngle y Throw::GetCameraAimAngles) para que el eje
	// "adelante" (Y local) de un objeto apunte hacia a_direction. No
	// calcula alabeo: sin datos de las normales de la malla de colisión
	// (Mecanica del arma.txt, punto 11) no hay forma de saber qué eje del
	// modelo es el filo o el mango, así que el enderezado se limita a
	// apuntar el eje de avance del modelo en la dirección de vuelo. El eje
	// Y local coincide con la dirección mango→cabeza del arma soportada
	// (comprobado con las cajas de colisión del NIF: ambas están
	// desplazadas solo en Y respecto al origen).
	[[nodiscard]] RE::NiPoint3 ComputeLookAtAngles(const RE::NiPoint3& a_direction);

	// Interpola angularmente de a_from a a_to (radianes) por el camino más
	// corto, para mezclar ángulos (p. ej. guiñada) sin saltos al cruzar el
	// límite ±π.
	[[nodiscard]] float LerpAngle(float a_from, float a_to, float a_t);
}
