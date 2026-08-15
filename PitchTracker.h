/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <array>

// A low-latency logarithmic pitch analyser. It maintains a constant-Q-like
// resonator bank, finds adaptive local peaks, rejects peaks explained as
// harmonics of lower fundamentals, and tracks the remaining notes over time.
// The published field spans six absolute octaves from concert A / 8.
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
	void findFundamentalPeaks();
	void updateTrackedNotes();
	void renderTrackedField(float* destination) const;

	double sampleRate_ { 0.0 };
	float concertAHz_ { 440.0f };
	float previousInput_ { 0.0f };
	float dcBlockerOutput_ { 0.0f };
	float dcBlockerCoefficient_ { 0.0f };
	float currentInputPeak_ { 0.0f };
	float adaptiveSignalLevel_ { 0.0f };

	std::array<Resonator, analysisBinCount> resonators_ {};
	std::array<float, analysisBinCount> analysisBins_ {};
	std::array<float, analysisBinCount> smoothedBins_ {};
	std::array<float, analysisBinCount> sortedBins_ {};
	std::array<Peak, analysisBinCount> candidatePeaks_ {};
	std::array<Peak, maximumTrackedNotes> fundamentalPeaks_ {};
	std::array<TrackedNote, maximumTrackedNotes> trackedNotes_ {};
	int candidatePeakCount_ { 0 };
	int fundamentalPeakCount_ { 0 };
};
