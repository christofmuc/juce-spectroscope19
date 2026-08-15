/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "Spectrogram.h"
#include "SpectrogramWidget.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

class DemoAnalysisWorker final : private juce::Thread {
public:
	explicit DemoAnalysisWorker(std::shared_ptr<Spectrogram> analyzer);
	~DemoAnalysisWorker() override;

	void prepare(double sampleRate);
	void release();
	bool enqueue(const float* const* channels, int numChannels, int numSamples) noexcept;

private:
	static constexpr int queueCapacity = 8;
	static constexpr int maximumBlockSize = 8192;

	struct Frame {
		int numSamples { 0 };
		std::array<std::array<float, maximumBlockSize>, 2> channels {};
	};

	void run() override;
	void process(Frame& frame);

	std::shared_ptr<Spectrogram> analyzer_;
	juce::AbstractFifo fifo_ { queueCapacity };
	std::array<Frame, queueCapacity> frames_ {};
};

class MainComponent final : public juce::Component,
	private juce::AudioIODeviceCallback,
	private juce::Timer {
public:
	MainComponent();
	~MainComponent() override;

	void paint(juce::Graphics& graphics) override;
	void resized() override;

private:
	void initialiseAudio();
	void timerCallback() override;
	void audioDeviceIOCallbackWithContext(
		const float* const* inputChannelData,
		int numInputChannels,
		float* const* outputChannelData,
		int numOutputChannels,
		int numSamples,
		const juce::AudioIODeviceCallbackContext&) override;
	void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
	void audioDeviceStopped() override;

	std::shared_ptr<Spectrogram> analyzer_;
	DemoAnalysisWorker analysisWorker_;
	SpectrogramWidget spectrogram_;
	juce::AudioDeviceManager deviceManager_;
	juce::AudioDeviceSelectorComponent deviceSelector_;
	juce::TextButton deviceButton_ { "Audio input" };
	juce::ToggleButton logarithmicButton_ { "Log frequency" };
	juce::ToggleButton horizontalButton_ { "Horizontal history" };
	juce::ToggleButton pitchColourButton_ { "Pitch colours" };
	juce::Label concertALabel_;
	juce::Slider concertASlider_;
	juce::Label statusLabel_;
	bool audioCallbackRegistered_ { false };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
