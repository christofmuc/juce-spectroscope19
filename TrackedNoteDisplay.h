/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "TrackedPitch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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
				if (first.note.confidence < second.note.confidence)
					return true;
				if (second.note.confidence < first.note.confidence)
					return false;
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

// Sequence-anchored annotations for horizontal waterfall history. A currently
// tracked note follows the newest row. On release it is pinned to that row and
// moves left with the same sequence-to-screen mapping as the FFT waterfall.
class TrackedNoteHistory {
public:
	static constexpr int capacity = 48;

	struct Entry {
		TrackedPitch note;
		float historyPosition { 1.0f };
		float opacity { 1.0f };
	};

	void update(const TrackedPitch* notes, int noteCount, std::uint64_t newestSequence) noexcept
	{
		if (newestSequence < latestUpdateSequence_)
			clear();

		std::array<bool, capacity> claimedSlots {};
		const auto validNoteCount = notes != nullptr ? std::max(0, noteCount) : 0;
		for (int noteIndex = 0; noteIndex < validNoteCount; ++noteIndex) {
			const auto& note = notes[noteIndex];
			auto slotIndex = matchingTrackingSlot(note, claimedSlots);
			if (slotIndex < 0)
				slotIndex = reusableSlot(claimedSlots);
			if (slotIndex < 0)
				continue;

			auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			slot.note = note;
			slot.sequence = newestSequence;
			slot.occupied = true;
			slot.tracking = true;
			claimedSlots[static_cast<std::size_t>(slotIndex)] = true;
		}

		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			if (slot.tracking && !claimedSlots[static_cast<std::size_t>(slotIndex)]) {
				slot.tracking = false;
				slot.sequence = newestSequence;
			}
		}
		latestUpdateSequence_ = newestSequence;
	}

	int visibleEntries(std::uint64_t newestSequence, int historyRowCount,
		Entry* destination, int destinationCapacity) const noexcept
	{
		if (historyRowCount <= 0 || destination == nullptr || destinationCapacity <= 0)
			return 0;

		auto count = 0;
		for (const auto& slot : slots_) {
			if (!slot.occupied || count >= destinationCapacity)
				continue;
			const auto anchorSequence = slot.tracking ? newestSequence : slot.sequence;
			const auto ageRows = newestSequence >= anchorSequence
				? newestSequence - anchorSequence : 0;
			if (ageRows >= static_cast<std::uint64_t>(historyRowCount))
				continue;

			const auto denominator = static_cast<float>(std::max(1, historyRowCount - 1));
			const auto historyPosition = 1.0f
				- static_cast<float>(ageRows) / denominator;
			const auto opacity = 1.0f - static_cast<float>(ageRows)
				/ static_cast<float>(historyRowCount);
			destination[count++] = { slot.note, historyPosition, opacity };
		}

		std::sort(destination, destination + count,
			[](const Entry& first, const Entry& second) {
				if (first.note.confidence < second.note.confidence)
					return true;
				if (second.note.confidence < first.note.confidence)
					return false;
				return first.opacity < second.opacity;
			});
		return count;
	}

	void clear() noexcept
	{
		for (auto& slot : slots_)
			slot = {};
		latestUpdateSequence_ = 0;
	}

private:
	struct Slot {
		TrackedPitch note;
		std::uint64_t sequence { 0 };
		bool occupied { false };
		bool tracking { false };
	};

	int matchingTrackingSlot(const TrackedPitch& note,
		const std::array<bool, capacity>& claimedSlots) const noexcept
	{
		auto bestSlot = -1;
		auto bestDistanceCents = 75.0;
		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			const auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			if (!slot.tracking || claimedSlots[static_cast<std::size_t>(slotIndex)]
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

	int reusableSlot(const std::array<bool, capacity>& claimedSlots) const noexcept
	{
		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			if (!slots_[static_cast<std::size_t>(slotIndex)].occupied)
				return slotIndex;
		}

		auto oldestSlot = -1;
		auto oldestSequence = latestUpdateSequence_;
		for (int slotIndex = 0; slotIndex < capacity; ++slotIndex) {
			const auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
			if (!slot.tracking && !claimedSlots[static_cast<std::size_t>(slotIndex)]
				&& slot.sequence <= oldestSequence) {
				oldestSequence = slot.sequence;
				oldestSlot = slotIndex;
			}
		}
		return oldestSlot;
	}

	std::array<Slot, capacity> slots_ {};
	std::uint64_t latestUpdateSequence_ { 0 };
};

}
