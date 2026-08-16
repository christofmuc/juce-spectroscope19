/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "TrackedPitch.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace spectroscope {

// Message-thread state for the diagnostic overlay. Fresh observations replace
// nearby notes, while missing observations remain briefly and then fade. The
// returned paint order is weakest-to-strongest so confident labels win overlaps.
class TrackedNoteDisplay {
public:
	static constexpr int capacity = 12;
	static constexpr double holdDurationMs = 120.0;
	static constexpr double fadeOutDurationMs = 750.0;

	struct Entry {
		TrackedPitch note;
		float opacity { 0.0f };
	};

	void update(const TrackedPitch* notes, int noteCount, double nowMs) noexcept
	{
		std::array<bool, capacity> claimedSlots {};
		const auto validNoteCount = notes != nullptr ? std::max(0, noteCount) : 0;
		for (int noteIndex = 0; noteIndex < validNoteCount; ++noteIndex) {
			const auto& note = notes[noteIndex];
			auto slotIndex = matchingSlot(note, claimedSlots);
			if (slotIndex < 0)
				slotIndex = reusableSlot(claimedSlots, nowMs);
			if (slotIndex < 0)
				continue;

			auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			slot.note = note;
			slot.lastSeenMs = nowMs;
			slot.active = true;
			claimedSlots[static_cast<std::size_t>(slotIndex)] = true;
		}
	}

	int visibleEntries(double nowMs, Entry* destination, int destinationCapacity) const noexcept
	{
		if (destination == nullptr || destinationCapacity <= 0)
			return 0;

		auto count = 0;
		for (const auto& slot : slots_) {
			if (!slot.active || count >= destinationCapacity)
				continue;
			const auto opacity = opacityForAge(nowMs - slot.lastSeenMs);
			if (opacity <= 0.0f)
				continue;
			destination[count++] = { slot.note, opacity };
		}

		std::sort(destination, destination + count,
			[](const Entry& first, const Entry& second) {
				if (first.note.confidence != second.note.confidence)
					return first.note.confidence < second.note.confidence;
				return first.opacity < second.opacity;
			});
		return count;
	}

	void clear() noexcept
	{
		for (auto& slot : slots_)
			slot = {};
	}

	static float opacityForAge(double ageMs) noexcept
	{
		if (ageMs <= holdDurationMs)
			return 1.0f;
		const auto fadeProgress = (ageMs - holdDurationMs) / fadeOutDurationMs;
		return static_cast<float>(std::clamp(1.0 - fadeProgress, 0.0, 1.0));
	}

private:
	struct Slot {
		TrackedPitch note;
		double lastSeenMs { 0.0 };
		bool active { false };
	};

	int matchingSlot(const TrackedPitch& note,
		const std::array<bool, capacity>& claimedSlots) const noexcept
	{
		auto bestSlot = -1;
		auto bestDistanceCents = 75.0;
		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			const auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			if (!slot.active || claimedSlots[static_cast<std::size_t>(slotIndex)]
				|| slot.note.midiNote != note.midiNote
				|| slot.note.frequencyHz <= 0.0f || note.frequencyHz <= 0.0f) {
				continue;
			}
			const auto distanceCents = std::abs(1200.0 * std::log2(
				static_cast<double>(note.frequencyHz) / slot.note.frequencyHz));
			if (distanceCents < bestDistanceCents) {
				bestDistanceCents = distanceCents;
				bestSlot = slotIndex;
			}
		}
		return bestSlot;
	}

	int reusableSlot(const std::array<bool, capacity>& claimedSlots, double nowMs) const noexcept
	{
		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			if (!slots_[static_cast<std::size_t>(slotIndex)].active)
				return slotIndex;
		}

		auto oldestSlot = -1;
		auto oldestSeenMs = nowMs;
		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			const auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			if (!claimedSlots[static_cast<std::size_t>(slotIndex)]
				&& slot.lastSeenMs <= oldestSeenMs) {
				oldestSeenMs = slot.lastSeenMs;
				oldestSlot = slotIndex;
			}
		}
		return oldestSlot;
	}

	std::array<Slot, capacity> slots_ {};
};

}
