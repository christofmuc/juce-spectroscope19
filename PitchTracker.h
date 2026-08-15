/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <array>

// A low-latency logarithmic pitch analyser. It maintains a constant-Q-like
// resonator bank, folds matching bins across octaves, interpolates local peaks,
// and tracks those peaks over time. The published field is circular: position
// zero is concert A and one full texture width is one octave.
class PitchTracker {
public:
	static constexpr int binsPerOctave = 24;
	static constexpr int octaveCount = 6;
	static constexpr int analysisBinCount = binsPerOctave * octaveCount;
	static constexpr int outputBinCount = 256;
	static constexpr int maximumTrackedNotes = 12;

	void prepare(double sampleRate, float concertAHz = 440.0f);
	void reset();
	void setConcertAHz(float frequencyHz);

	void process(const float* samples, int numSamples);
	void calculate(float* destination, int destinationSize);

	double sampleRate() const noexcept;
	float concertAHz() const noexcept;

private:
	struct Resonator {
		float cosineStep { 1.0f };
		float sineStep { 0.0f };
		float cosinePhase { 1.0f };
		float sinePhase { 0.0f };
		float decay { 0.0f };
		float inPhase { 0.0f };
		float quadrature { 0.0f };
	};

	struct Peak {
		float position { 0.0f };
		float strength { 0.0f };
	};

	struct TrackedNote {
		float position { 0.0f };
		float strength { 0.0f };
		bool active { false };
	};

	void rebuildResonators();
	void updateTrackedNotes();
	void renderTrackedField(float* destination) const;

	double sampleRate_ { 0.0 };
	float concertAHz_ { 440.0f };
	float previousInput_ { 0.0f };
	float dcBlockerOutput_ { 0.0f };
	float dcBlockerCoefficient_ { 0.0f };

	std::array<Resonator, analysisBinCount> resonators_ {};
	std::array<float, binsPerOctave> foldedBins_ {};
	std::array<float, binsPerOctave> smoothedBins_ {};
	std::array<Peak, binsPerOctave> peaks_ {};
	std::array<TrackedNote, maximumTrackedNotes> trackedNotes_ {};
	int peakCount_ { 0 };
};
