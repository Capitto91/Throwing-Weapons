// Tiempo de un clip hasta su anotación -- ver LeadTimeEstimator.h.

#include "9.- MATH/LeadTimeEstimator.h"

#include <algorithm>

namespace Math
{
	LeadTimeEstimator::LeadTimeEstimator(float a_nominalThirdPerson, float a_nominalFirstPerson, float a_minFactor, float a_maxFactor, std::size_t a_sampleCount) :
		views{ View{ a_nominalThirdPerson, a_nominalThirdPerson, {} }, View{ a_nominalFirstPerson, a_nominalFirstPerson, {} } },
		minFactor(a_minFactor),
		maxFactor(a_maxFactor),
		sampleCount(a_sampleCount)
	{}

	float LeadTimeEstimator::Get(bool a_firstPerson) const
	{
		return views[a_firstPerson ? 1 : 0].current;
	}

	bool LeadTimeEstimator::Record(bool a_firstPerson, float a_measured)
	{
		auto& view = views[a_firstPerson ? 1 : 0];
		if (!(a_measured >= view.nominal * minFactor && a_measured <= view.nominal * maxFactor)) {
			return false;
		}

		view.samples.push_back(a_measured);
		if (view.samples.size() > sampleCount) {
			view.samples.erase(view.samples.begin());
		}

		// Mediana: un gesto retrasado suelto no cambia el valor.
		std::vector<float> sorted = view.samples;
		std::ranges::sort(sorted);
		const std::size_t middle = sorted.size() / 2;
		view.current = sorted.size() % 2 != 0 ? sorted[middle] : 0.5f * (sorted[middle - 1] + sorted[middle]);
		return true;
	}
}
