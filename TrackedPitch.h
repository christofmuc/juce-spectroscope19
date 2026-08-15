/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PitchTracker.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace spectroscope {
struct TrackedPitch {
	float frequencyHz { 0.0f };
	float confidence { 0.0f };
	float cents { 0.0f };
	int midiNote { 0 };
};

// Extracts the strongest local maxima from an absolute tracked-pitch field.
// Results are ordered by frequency so a diagnostic display remains stable.
inline int extractTrackedPitches(const float* field, int fieldSize, float concertAHz,
	TrackedPitch* destination, int destinationCapacity, float minimumConfidence = 0.05f)
{
	if (field == nullptr || fieldSize != PitchTracker::outputBinCount
		|| destination == nullptr || destinationCapacity <= 0 || concertAHz <= 0.0f) {
		return 0;
	}

	std::array<TrackedPitch, PitchTracker::outputBinCount> candidates {};
	auto candidateCount = 0;
	for (int bin = 0; bin < fieldSize; ++bin) {
		const auto left = bin > 0 ? field[bin - 1] : 0.0f;
		const auto centre = field[bin];
		const auto right = bin + 1 < fieldSize ? field[bin + 1] : 0.0f;
		if (centre < minimumConfidence || centre <= left || centre < right)
			continue;

		const auto denominator = left - 2.0f * centre + right;
		auto offset = 0.0f;
		if (std::abs(denominator) > 0.000001f)
			offset = std::clamp(0.5f * (left - right) / denominator, -0.5f, 0.5f);
		const auto texturePosition = (static_cast<float>(bin) + 0.5f + offset)
			/ static_cast<float>(fieldSize);
		const auto frequency = concertAHz / 8.0f * std::pow(
			2.0f, texturePosition * static_cast<float>(PitchTracker::octaveCount));
		const auto midiPosition = 69.0f + 12.0f * std::log2(frequency / concertAHz);
		const auto midiNote = static_cast<int>(std::round(midiPosition));
		candidates[static_cast<std::size_t>(candidateCount++)] = {
			frequency, centre, 100.0f * (midiPosition - static_cast<float>(midiNote)), midiNote
		};
	}

	const auto selectedCount = std::min(candidateCount, destinationCapacity);
	std::partial_sort(candidates.begin(), candidates.begin() + selectedCount,
		candidates.begin() + candidateCount,
		[](const TrackedPitch& first, const TrackedPitch& second) {
			return first.confidence > second.confidence;
		});
	std::sort(candidates.begin(), candidates.begin() + selectedCount,
		[](const TrackedPitch& first, const TrackedPitch& second) {
			return first.frequencyHz < second.frequencyHz;
		});
	std::copy_n(candidates.begin(), selectedCount, destination);
	return selectedCount;
}
}
