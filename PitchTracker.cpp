/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "PitchTracker.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
constexpr float minimumConcertAHz = 400.0f;
constexpr float maximumConcertAHz = 480.0f;
constexpr float resonatorCycles = 12.0f;
constexpr float levelAttack = 0.35f;
constexpr float levelRelease = 0.015f;
constexpr float noteAttack = 0.45f;
constexpr float noteRelease = 0.93f;
constexpr float notePositionFollow = 0.35f;
constexpr float noteMatchDistance = 2.0f;
constexpr float noteRemovalStrength = 0.01f;
constexpr float fieldSigma = 0.82f;

float clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

float smoothStep(float lower, float upper, float value)
{
	const auto normalised = clamp01((value - lower) / (upper - lower));
	return normalised * normalised * (3.0f - 2.0f * normalised);
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
	currentInputPeak_ = 0.0f;
	adaptiveSignalLevel_ = 0.0f;
	std::fill(analysisBins_.begin(), analysisBins_.end(), 0.0f);
	std::fill(smoothedBins_.begin(), smoothedBins_.end(), 0.0f);
	std::fill(sortedBins_.begin(), sortedBins_.end(), 0.0f);
	candidatePeakCount_ = 0;
	fundamentalPeakCount_ = 0;

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

	currentInputPeak_ = 0.0f;
	for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
		const auto input = samples[sampleIndex];
		currentInputPeak_ = std::max(currentInputPeak_, std::abs(input));
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

	for (int bin = 0; bin < analysisBinCount; ++bin) {
		const auto& resonator = resonators_[static_cast<std::size_t>(bin)];
		analysisBins_[static_cast<std::size_t>(bin)] = 2.0f * (1.0f - resonator.decay)
			* std::hypot(resonator.inPhase, resonator.quadrature);
	}

	for (int bin = 0; bin < analysisBinCount; ++bin) {
		const auto left = std::max(0, bin - 1);
		const auto right = std::min(analysisBinCount - 1, bin + 1);
		smoothedBins_[static_cast<std::size_t>(bin)] =
			0.25f * analysisBins_[static_cast<std::size_t>(left)]
			+ 0.5f * analysisBins_[static_cast<std::size_t>(bin)]
			+ 0.25f * analysisBins_[static_cast<std::size_t>(right)];
	}

	findFundamentalPeaks();
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

	dcBlockerCoefficient_ = static_cast<float>(std::exp(-twoPi * 20.0 / sampleRate_));
}

void PitchTracker::findFundamentalPeaks()
{
	candidatePeakCount_ = 0;
	fundamentalPeakCount_ = 0;
	const auto maximumBin = *std::max_element(smoothedBins_.begin(), smoothedBins_.end());
	const auto levelCoefficient = maximumBin > adaptiveSignalLevel_ ? levelAttack : levelRelease;
	adaptiveSignalLevel_ += levelCoefficient * (maximumBin - adaptiveSignalLevel_);
	if (currentInputPeak_ <= 0.00001f || maximumBin <= 0.000001f
		|| adaptiveSignalLevel_ <= 0.000001f
		|| maximumBin < adaptiveSignalLevel_ * 0.02f) {
		return;
	}

	std::copy(smoothedBins_.begin(), smoothedBins_.end(), sortedBins_.begin());
	std::sort(sortedBins_.begin(), sortedBins_.end());
	const auto noiseFloor = sortedBins_[static_cast<std::size_t>(analysisBinCount / 2)];

	for (int bin = 1; bin < analysisBinCount - 1; ++bin) {
		const auto left = smoothedBins_[static_cast<std::size_t>(bin - 1)];
		const auto centre = smoothedBins_[static_cast<std::size_t>(bin)];
		const auto right = smoothedBins_[static_cast<std::size_t>(bin + 1)];
		if (centre <= left || centre < right)
			continue;

		const auto localProminence = (centre - std::max(left, right))
			/ std::max(centre, 0.000001f);
		const auto noiseContrast = (centre - noiseFloor)
			/ std::max(centre, 0.000001f);
		const auto relativeLevel = centre / std::max(adaptiveSignalLevel_, maximumBin * 0.25f);
		const auto coherentLevel = centre / std::max(currentInputPeak_, 0.000001f);
		const auto strength = std::sqrt(clamp01(relativeLevel))
			* smoothStep(0.03f, 0.25f, localProminence)
			* smoothStep(0.35f, 0.90f, noiseContrast)
			* smoothStep(0.08f, 0.35f, coherentLevel);
		if (strength < 0.025f)
			continue;

		const auto denominator = left - 2.0f * centre + right;
		auto offset = 0.0f;
		if (std::abs(denominator) > 0.000001f)
			offset = std::clamp(0.5f * (left - right) / denominator, -0.5f, 0.5f);
		candidatePeaks_[static_cast<std::size_t>(candidatePeakCount_++)] = {
			static_cast<float>(bin) + offset, strength
		};
	}

	// Low fundamentals are considered before their overtones. A candidate near
	// an integer multiple of an already accepted lower peak is rendered by the
	// FFT but omitted from the colour mask.
	for (int candidateIndex = 0; candidateIndex < candidatePeakCount_; ++candidateIndex) {
		const auto& candidate = candidatePeaks_[static_cast<std::size_t>(candidateIndex)];
		auto explainedByHarmonic = false;
		for (int fundamentalIndex = 0; fundamentalIndex < fundamentalPeakCount_; ++fundamentalIndex) {
			const auto& lower = fundamentalPeaks_[static_cast<std::size_t>(fundamentalIndex)];
			const auto ratio = std::pow(2.0f,
				(candidate.position - lower.position) / static_cast<float>(binsPerOctave));
			const auto harmonic = std::round(ratio);
			if (harmonic < 2.0f || harmonic > 8.0f)
				continue;
			const auto harmonicDistance = std::abs(static_cast<float>(binsPerOctave)
				* std::log2(ratio / harmonic));
			if (harmonicDistance < 0.65f && lower.strength > 0.06f) {
				explainedByHarmonic = true;
				break;
			}
		}
		if (!explainedByHarmonic && fundamentalPeakCount_ < maximumTrackedNotes)
			fundamentalPeaks_[static_cast<std::size_t>(fundamentalPeakCount_++)] = candidate;
	}
}

void PitchTracker::updateTrackedNotes()
{
	std::array<bool, maximumTrackedNotes> matchedTracks {};
	for (int peakIndex = 0; peakIndex < fundamentalPeakCount_; ++peakIndex) {
		const auto& peak = fundamentalPeaks_[static_cast<std::size_t>(peakIndex)];
		int bestTrack = -1;
		auto bestDistance = noteMatchDistance;
		for (int trackIndex = 0; trackIndex < maximumTrackedNotes; ++trackIndex) {
			const auto& track = trackedNotes_[static_cast<std::size_t>(trackIndex)];
			if (!track.active || matchedTracks[static_cast<std::size_t>(trackIndex)])
				continue;
			const auto distance = std::abs(track.position - peak.position);
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
			track.position += notePositionFollow * (peak.position - track.position);
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
				* static_cast<float>(analysisBinCount) / static_cast<float>(outputBinCount);
			const auto distance = std::abs(position - note.position);
			const auto gaussian = std::exp(-0.5f * distance * distance / (fieldSigma * fieldSigma));
			auto& output = destination[outputBin];
			output = std::max(output, clamp01(note.strength * gaussian));
		}
	}
}
