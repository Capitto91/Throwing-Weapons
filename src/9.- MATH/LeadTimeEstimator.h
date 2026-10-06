// Tiempo de un clip hasta su anotación, por cámara: mediana de las últimas medidas válidas, o el nominal mientras
// no haya ninguna. Lo usan el Atrape (WeaponManager) y el golpe en salto (LightningDash).

#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace Math
{
	class LeadTimeEstimator
	{
	public:
		// Nominal de cada cámara; una medida vale si está entre a_minFactor y a_maxFactor veces su nominal; la mediana
		// usa las a_sampleCount últimas medidas válidas (impar: la mediana es una medida real).
		LeadTimeEstimator(float a_nominalThirdPerson, float a_nominalFirstPerson, float a_minFactor, float a_maxFactor, std::size_t a_sampleCount);

		// Tiempo actual para la cámara indicada.
		[[nodiscard]] float Get(bool a_firstPerson) const;

		// Añade a_measured y recalcula la mediana de esa cámara. false si está fuera de rango (tirón durante el
		// gesto o pausa sin FrameHook): se descarta y se conserva el valor anterior.
		bool Record(bool a_firstPerson, float a_measured);

	private:
		struct View
		{
			float              nominal;
			float              current;
			std::vector<float> samples;  // la más antigua primero
		};

		std::array<View, 2> views;  // [0] tercera persona, [1] primera
		float               minFactor;
		float               maxFactor;
		std::size_t         sampleCount;
	};
}
