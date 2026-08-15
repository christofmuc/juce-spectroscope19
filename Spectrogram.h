/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <cstdint>
#include <vector>

// Stateful FFT analyzer. process() is intended to run on an analysis worker,
// never on a real-time audio callback. The UI may copy the latest completed
// spectrum concurrently via copyLatestSpectrum().
class Spectrogram {
public:
	static constexpr int defaultFftOrder = 11;
	static constexpr float defaultFloorDb = -100.0f;

	explicit Spectrogram(int fftOrder = defaultFftOrder, int hopSize = 0, float floorDb = defaultFloorDb);

	int fftSize() const noexcept;
	int spectrumSize() const noexcept;
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
	std::vector<float> inputData_;
	std::vector<float> windowedData_;
	std::vector<float> fftWork_;
	std::vector<float> nextSpectrum_;
	std::vector<float> publishedSpectrum_;
	int inputDataAvailable_ { 0 };
	float windowMagnitudeScale_ { 1.0f };

	mutable juce::CriticalSection publishedSpectrumLock_;
	std::atomic<std::uint64_t> sequence_ { 0 };
	std::atomic<std::uint64_t> droppedSamples_ { 0 };
	std::atomic<double> sampleRate_ { 0.0 };
};
