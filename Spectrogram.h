/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "PitchTracker.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <cstdint>
#include <vector>

// Stateful FFT and fundamental-pitch analyzer. process() is intended to run on an analysis
// worker, never on a real-time audio callback. The UI may copy completed
// synchronized spectrum and tracked-pitch frames concurrently.
class Spectrogram {
public:
	static constexpr int defaultFftOrder = 11;
	static constexpr float defaultFloorDb = -100.0f;
	static constexpr int spectrumHistoryCapacity = 128;

	explicit Spectrogram(int fftOrder = defaultFftOrder, int hopSize = 0, float floorDb = defaultFloorDb);

	int fftSize() const noexcept;
	int spectrumSize() const noexcept;
	int pitchClassSize() const noexcept;
	int hopSize() const noexcept;
	float floorDb() const noexcept;

	void prepare(double sampleRate);
	void reset();

	// Accepts any channel count and downmixes active channels to mono. Returns
	// the number of complete spectrum rows produced by this call. If the input
	// exceeds the internal staging capacity, the newest excess samples are
	// dropped rather than blocking the caller.
	int process(const juce::AudioSourceChannelInfo& data);

	// Copies the most recent row. Returns false until the first FFT has been
	// produced or when the destination is too small.
	bool copyLatestSpectrum(float* destination, int destinationSize, std::uint64_t* sequence = nullptr) const;

	// Copies completed spectrum rows newer than afterSequence, oldest first. If
	// destination cannot hold the entire backlog, the newest rows are retained.
	// Returns the number of copied rows and reports their newest sequence number.
	int copySpectrumFramesAfter(std::uint64_t afterSequence, float* destination,
		int destinationSize, std::uint64_t* copiedThroughSequence = nullptr) const;

	// Copies synchronized FFT and tracked fundamental-pitch rows. Both destinations
	// receive the same oldest-to-newest sequence range.
	int copyAnalysisFramesAfter(std::uint64_t afterSequence,
		float* spectrumDestination, int spectrumDestinationSize,
		float* pitchDestination, int pitchDestinationSize,
		std::uint64_t* copiedThroughSequence = nullptr) const;

	bool copyLatestPitchClass(float* destination, int destinationSize,
		std::uint64_t* copiedSequence = nullptr) const;

	// Thread-safe tuning target; the analysis worker applies changes at the next hop.
	void setConcertAHz(float frequencyHz) noexcept;
	float concertAHz() const noexcept;
	void setPitchTrackingPreset(PitchTracker::Preset preset) noexcept;
	PitchTracker::Preset pitchTrackingPreset() const noexcept;

	std::uint64_t sequence() const noexcept;
	std::uint64_t droppedSamples() const noexcept;
	double sampleRate() const noexcept;

private:
	int writeInput(const juce::AudioSourceChannelInfo& data);
	void readHop();
	void appendHop();
	void calculateSpectrum();

	const int fftOrder_;
	const int fftSize_;
	const int hopSize_;
	const float floorDb_;
	const int fifoCapacity_;

	juce::AbstractFifo fifo_;
	juce::AudioBuffer<float> fifoBuffer_;
	juce::AudioBuffer<float> hopBuffer_;

	juce::dsp::FFT forwardFFT_;
	juce::dsp::WindowingFunction<float> window_;
	PitchTracker pitchTracker_;
	std::vector<float> inputData_;
	std::vector<float> windowedData_;
	std::vector<float> fftWork_;
	std::vector<float> nextSpectrum_;
	std::vector<float> nextPitchClass_;
	std::vector<float> publishedSpectra_;
	std::vector<float> publishedPitchClasses_;
	int inputDataAvailable_ { 0 };
	float windowMagnitudeScale_ { 1.0f };

	mutable juce::CriticalSection publishedSpectrumLock_;
	std::atomic<std::uint64_t> sequence_ { 0 };
	std::atomic<std::uint64_t> droppedSamples_ { 0 };
	std::atomic<double> sampleRate_ { 0.0 };
	std::atomic<float> concertAHz_ { 440.0f };
	std::atomic<PitchTracker::Preset> pitchTrackingPreset_ { PitchTracker::Preset::balanced };
};
