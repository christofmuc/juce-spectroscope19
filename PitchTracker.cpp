/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PitchTracker.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace {
constexpr float minimumConcertAHz = 400.0f;
constexpr float maximumConcertAHz = 480.0f;
constexpr float resonatorCycles = 12.0f;
constexpr float noteAttack = 0.45f;
constexpr float noteRelease = 0.93f;
constexpr float notePositionFollow = 0.35f;
constexpr float noteMatchDistance = 2.0f;
constexpr float noteRemovalStrength = 0.01f;
constexpr float fieldSigma = 0.72f;

float clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

float smoothStep(float lower, float upper, float value)
{
	const auto normalised = clamp01((value - lower) / (upper - lower));
	return normalised * normalised * (3.0f - 2.0f * normalised);
}

float wrappedPosition(float position)
{
	position = std::fmod(position, static_cast<float>(PitchTracker::binsPerOctave));
	return position < 0.0f ? position + static_cast<float>(PitchTracker::binsPerOctave) : position;
}

float circularDelta(float from, float to)
{
	const auto period = static_cast<float>(PitchTracker::binsPerOctave);
	auto delta = wrappedPosition(to - from);
	if (delta > period * 0.5f)
		delta -= period;
	return delta;
}

float circularDistance(float first, float second)
{
	return std::abs(circularDelta(first, second));
}
}

void PitchTracker::prepare(double newSampleRate, float newConcertAHz)
{
	sampleRate_ = std::max(0.0, newSampleRate);
	concertAHz_ = std::clamp(newConcertAHz, minimumConcertAHz, maximumConcertAHz);
	rebuildResonators();
	reset();
}

void PitchTracker::reset()
{
	previousInput_ = 0.0f;
	dcBlockerOutput_ = 0.0f;
	std::fill(foldedBins_.begin(), foldedBins_.end(), 0.0f);
	std::fill(smoothedBins_.begin(), smoothedBins_.end(), 0.0f);
	peakCount_ = 0;

	for (auto& resonator : resonators_) {
		resonator.cosinePhase = 1.0f;
		resonator.sinePhase = 0.0f;
		resonator.inPhase = 0.0f;
		resonator.quadrature = 0.0f;
	}
	for (auto& note : trackedNotes_)
		note = {};
}

void PitchTracker::setConcertAHz(float frequencyHz)
{
	const auto clampedFrequency = std::clamp(frequencyHz, minimumConcertAHz, maximumConcertAHz);
	if (std::abs(clampedFrequency - concertAHz_) < 0.001f)
		return;

	concertAHz_ = clampedFrequency;
	rebuildResonators();
	reset();
}

void PitchTracker::process(const float* samples, int numSamples)
{
	if (samples == nullptr || numSamples <= 0 || sampleRate_ <= 0.0)
		return;

	for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
		const auto input = samples[sampleIndex];
		const auto filteredInput = input - previousInput_ + dcBlockerCoefficient_ * dcBlockerOutput_;
		previousInput_ = input;
		dcBlockerOutput_ = filteredInput;

		for (auto& resonator : resonators_) {
			resonator.inPhase = resonator.decay * resonator.inPhase
				+ filteredInput * resonator.cosinePhase;
			resonator.quadrature = resonator.decay * resonator.quadrature
				+ filteredInput * resonator.sinePhase;

			const auto nextCosine = resonator.cosinePhase * resonator.cosineStep
				- resonator.sinePhase * resonator.sineStep;
			const auto nextSine = resonator.sinePhase * resonator.cosineStep
				+ resonator.cosinePhase * resonator.sineStep;
			resonator.cosinePhase = nextCosine;
			resonator.sinePhase = nextSine;
		}
	}

	// Recurrence round-off accumulates very slowly. Renormalising once per input
	// block keeps the oscillators on the unit circle without per-sample square roots.
	for (auto& resonator : resonators_) {
		const auto magnitude = std::hypot(resonator.cosinePhase, resonator.sinePhase);
		if (magnitude > 0.0f) {
			resonator.cosinePhase /= magnitude;
			resonator.sinePhase /= magnitude;
		}
	}
}

void PitchTracker::calculate(float* destination, int destinationSize)
{
	if (destination == nullptr || destinationSize < outputBinCount)
		return;

	if (sampleRate_ <= 0.0) {
		std::fill_n(destination, outputBinCount, 0.0f);
		return;
	}

	std::fill(foldedBins_.begin(), foldedBins_.end(), 0.0f);
	constexpr std::array<float, octaveCount> octaveWeights { 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f };
	constexpr float octaveWeightSum = 5.0f;
	for (int octave = 0; octave < octaveCount; ++octave) {
		for (int pitchBin = 0; pitchBin < binsPerOctave; ++pitchBin) {
			const auto& resonator = resonators_[static_cast<std::size_t>(
				octave * binsPerOctave + pitchBin)];
			const auto magnitude = 2.0f * (1.0f - resonator.decay)
				* std::hypot(resonator.inPhase, resonator.quadrature);
			foldedBins_[static_cast<std::size_t>(pitchBin)] +=
				octaveWeights[static_cast<std::size_t>(octave)] * magnitude / octaveWeightSum;
		}
	}

	for (int pitchBin = 0; pitchBin < binsPerOctave; ++pitchBin) {
		const auto left = (pitchBin + binsPerOctave - 1) % binsPerOctave;
		const auto right = (pitchBin + 1) % binsPerOctave;
		smoothedBins_[static_cast<std::size_t>(pitchBin)] =
			0.25f * foldedBins_[static_cast<std::size_t>(left)]
			+ 0.5f * foldedBins_[static_cast<std::size_t>(pitchBin)]
			+ 0.25f * foldedBins_[static_cast<std::size_t>(right)];
	}

	updateTrackedNotes();
	renderTrackedField(destination);
}

double PitchTracker::sampleRate() const noexcept
{
	return sampleRate_;
}

float PitchTracker::concertAHz() const noexcept
{
	return concertAHz_;
}

void PitchTracker::rebuildResonators()
{
	if (sampleRate_ <= 0.0) {
		for (auto& resonator : resonators_)
			resonator = {};
		dcBlockerCoefficient_ = 0.0f;
		return;
	}

	const auto lowestA = static_cast<double>(concertAHz_) / 8.0;
	const auto twoPi = 2.0 * std::acos(-1.0);
	for (int bin = 0; bin < analysisBinCount; ++bin) {
		const auto frequency = lowestA * std::pow(2.0,
			static_cast<double>(bin) / static_cast<double>(binsPerOctave));
		const auto radians = twoPi * frequency / sampleRate_;
		auto& resonator = resonators_[static_cast<std::size_t>(bin)];
		resonator.cosineStep = static_cast<float>(std::cos(radians));
		resonator.sineStep = static_cast<float>(std::sin(radians));
		resonator.decay = static_cast<float>(std::exp(
			-frequency / (static_cast<double>(resonatorCycles) * sampleRate_)));
	}

	// A gentle DC blocker prevents offsets and subsonic motion from lighting the
	// lowest pitch bins while leaving the analysed musical range untouched.
	dcBlockerCoefficient_ = static_cast<float>(std::exp(-twoPi * 20.0 / sampleRate_));
}

void PitchTracker::updateTrackedNotes()
{
	peakCount_ = 0;
	const auto maximumBin = *std::max_element(smoothedBins_.begin(), smoothedBins_.end());
	if (maximumBin > 0.0f) {
		for (int pitchBin = 0; pitchBin < binsPerOctave; ++pitchBin) {
			const auto leftIndex = (pitchBin + binsPerOctave - 1) % binsPerOctave;
			const auto rightIndex = (pitchBin + 1) % binsPerOctave;
			const auto left = smoothedBins_[static_cast<std::size_t>(leftIndex)];
			const auto centre = smoothedBins_[static_cast<std::size_t>(pitchBin)];
			const auto right = smoothedBins_[static_cast<std::size_t>(rightIndex)];
			if (centre <= left || centre < right)
				continue;

			const auto prominenceRatio = (centre - std::max(left, right))
				/ std::max(centre, 0.000001f);
			const auto tonalConfidence = smoothStep(0.03f, 0.25f, prominenceRatio);
			const auto absoluteConfidence = smoothStep(0.0005f, 0.02f, centre);
			const auto strength = clamp01(centre / maximumBin)
				* tonalConfidence * absoluteConfidence;
			if (strength < 0.02f)
				continue;

			const auto denominator = left - 2.0f * centre + right;
			auto offset = 0.0f;
			if (std::abs(denominator) > 0.000001f)
				offset = std::clamp(0.5f * (left - right) / denominator, -0.5f, 0.5f);

			peaks_[static_cast<std::size_t>(peakCount_++)] = {
				wrappedPosition(static_cast<float>(pitchBin) + offset), strength
			};
		}
	}

	std::sort(peaks_.begin(), peaks_.begin() + static_cast<std::ptrdiff_t>(peakCount_),
		[](const Peak& first, const Peak& second) {
		return first.strength > second.strength;
	});

	std::array<bool, maximumTrackedNotes> matchedTracks {};
	for (int peakIndex = 0; peakIndex < peakCount_; ++peakIndex) {
		const auto& peak = peaks_[static_cast<std::size_t>(peakIndex)];
		int bestTrack = -1;
		auto bestDistance = noteMatchDistance;
		for (int trackIndex = 0; trackIndex < maximumTrackedNotes; ++trackIndex) {
			const auto& track = trackedNotes_[static_cast<std::size_t>(trackIndex)];
			if (!track.active || matchedTracks[static_cast<std::size_t>(trackIndex)])
				continue;
			const auto distance = circularDistance(track.position, peak.position);
			if (distance < bestDistance) {
				bestDistance = distance;
				bestTrack = trackIndex;
			}
		}

		if (bestTrack < 0) {
			for (int trackIndex = 0; trackIndex < maximumTrackedNotes; ++trackIndex) {
				if (!trackedNotes_[static_cast<std::size_t>(trackIndex)].active) {
					bestTrack = trackIndex;
					break;
				}
			}
		}
		if (bestTrack < 0)
			continue;

		auto& track = trackedNotes_[static_cast<std::size_t>(bestTrack)];
		if (!track.active) {
			track.position = peak.position;
			track.strength = peak.strength * noteAttack;
			track.active = true;
		} else {
			track.position = wrappedPosition(track.position
				+ notePositionFollow * circularDelta(track.position, peak.position));
			track.strength += noteAttack * (peak.strength - track.strength);
		}
		matchedTracks[static_cast<std::size_t>(bestTrack)] = true;
	}

	for (int trackIndex = 0; trackIndex < maximumTrackedNotes; ++trackIndex) {
		auto& track = trackedNotes_[static_cast<std::size_t>(trackIndex)];
		if (!track.active || matchedTracks[static_cast<std::size_t>(trackIndex)])
			continue;
		track.strength *= noteRelease;
		if (track.strength < noteRemovalStrength)
			track = {};
	}
}

void PitchTracker::renderTrackedField(float* destination) const
{
	std::fill_n(destination, outputBinCount, 0.0f);
	for (const auto& note : trackedNotes_) {
		if (!note.active)
			continue;
		for (int outputBin = 0; outputBin < outputBinCount; ++outputBin) {
			const auto position = (static_cast<float>(outputBin) + 0.5f)
				* static_cast<float>(binsPerOctave) / static_cast<float>(outputBinCount);
			const auto distance = circularDistance(position, note.position);
			const auto gaussian = std::exp(-0.5f * distance * distance / (fieldSigma * fieldSigma));
			auto& output = destination[outputBin];
			output = std::max(output, clamp01(note.strength * gaussian));
		}
	}
}
