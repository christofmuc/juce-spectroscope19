/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <algorithm>
#include <cmath>

namespace spectroscope::frequency_axis {

inline float normalisedPosition(
	double frequencyHz, double sampleRate, double minimumFrequencyHz, bool logarithmic) noexcept
{
	const auto nyquist = sampleRate * 0.5;
	if (frequencyHz <= 0.0 || nyquist <= 0.0)
		return 0.0f;

	if (!logarithmic)
		return static_cast<float>(std::clamp(frequencyHz / nyquist, 0.0, 1.0));

	const auto minimumFrequency = std::clamp(minimumFrequencyHz, 0.001, nyquist);
	if (minimumFrequency >= nyquist)
		return 0.0f;

	const auto position = std::log(frequencyHz / minimumFrequency)
		/ std::log(nyquist / minimumFrequency);
	return static_cast<float>(std::clamp(position, 0.0, 1.0));
}

inline float horizontalScreenPosition(float normalisedFrequency) noexcept
{
	return 1.0f - std::clamp(normalisedFrequency, 0.0f, 1.0f);
}

}
