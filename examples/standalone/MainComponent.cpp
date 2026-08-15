/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MainComponent.h"

DemoAnalysisWorker::DemoAnalysisWorker(std::shared_ptr<Spectrogram> analyzer)
	: juce::Thread("Spectroscope demo analysis")
	, analyzer_(std::move(analyzer))
{
}

DemoAnalysisWorker::~DemoAnalysisWorker()
{
	release();
}

void DemoAnalysisWorker::prepare(double sampleRate)
{
	release();
	if (analyzer_ == nullptr || sampleRate <= 0.0)
		return;

	analyzer_->prepare(sampleRate);
	startThread();
}

void DemoAnalysisWorker::release()
{
	signalThreadShouldExit();
	notify();
	stopThread(2000);
	fifo_.reset();
	if (analyzer_ != nullptr)
		analyzer_->reset();
}

bool DemoAnalysisWorker::enqueue(
	const float* const* channels, int numChannels, int numSamples) noexcept
{
	if (!isThreadRunning() || channels == nullptr || numChannels <= 0
		|| numSamples <= 0 || numSamples > maximumBlockSize)
		return false;

	int start1 = 0;
	int size1 = 0;
	int start2 = 0;
	int size2 = 0;
	fifo_.prepareToWrite(1, start1, size1, start2, size2);
	juce::ignoreUnused(start2, size2);
	if (size1 == 0)
		return false;

	auto& frame = frames_[static_cast<std::size_t>(start1)];
	frame.numSamples = numSamples;
	for (int destinationChannel = 0; destinationChannel < 2; ++destinationChannel) {
		const auto sourceChannel = juce::jmin(destinationChannel, numChannels - 1);
		auto* destination = frame.channels[static_cast<std::size_t>(destinationChannel)].data();
		if (channels[sourceChannel] != nullptr)
			juce::FloatVectorOperations::copy(destination, channels[sourceChannel], numSamples);
		else
			juce::FloatVectorOperations::clear(destination, numSamples);
	}

	fifo_.finishedWrite(1);
	notify();
	return true;
}

void DemoAnalysisWorker::run()
{
	while (!threadShouldExit() || fifo_.getNumReady() > 0) {
		int start1 = 0;
		int size1 = 0;
		int start2 = 0;
		int size2 = 0;
		fifo_.prepareToRead(1, start1, size1, start2, size2);
		juce::ignoreUnused(start2, size2);
		if (size1 == 0) {
			wait(20);
			continue;
		}

		process(frames_[static_cast<std::size_t>(start1)]);
		fifo_.finishedRead(1);
	}
}

void DemoAnalysisWorker::process(Frame& frame)
{
	std::array<float*, 2> channelPointers {
		frame.channels[0].data(), frame.channels[1].data()
	};
	juce::AudioBuffer<float> buffer(channelPointers.data(), 2, frame.numSamples);
	analyzer_->process({ &buffer, 0, frame.numSamples });
}

MainComponent::MainComponent()
	: analyzer_(std::make_shared<Spectrogram>())
	, analysisWorker_(analyzer_)
	, spectrogram_(analyzer_)
	, deviceSelector_(deviceManager_, 1, 2, 0, 0, false, false, true, false)
{
	addAndMakeVisible(spectrogram_);
	addAndMakeVisible(deviceButton_);
	addAndMakeVisible(logarithmicButton_);
	addAndMakeVisible(horizontalButton_);
	addAndMakeVisible(statusLabel_);
	addChildComponent(deviceSelector_);

	logarithmicButton_.setToggleState(true, juce::dontSendNotification);
	deviceButton_.onClick = [this] {
		const auto showSettings = !deviceSelector_.isVisible();
		deviceSelector_.setVisible(showSettings);
		spectrogram_.setVisible(!showSettings);
		resized();
	};
	logarithmicButton_.onClick = [this] {
		spectrogram_.setXAxis(logarithmicButton_.getToggleState());
	};
	horizontalButton_.onClick = [this] {
		spectrogram_.setHorizontalMode(horizontalButton_.getToggleState());
	};

	setSize(900, 650);
	startTimerHz(30);

	if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
		&& !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
		juce::Component::SafePointer<MainComponent> safeThis(this);
		juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
			[safeThis](bool granted) {
				if (safeThis == nullptr)
					return;
				if (granted)
					safeThis->initialiseAudio();
				else
					safeThis->statusLabel_.setText("Microphone permission denied", juce::dontSendNotification);
			});
	} else {
		initialiseAudio();
	}
}

MainComponent::~MainComponent()
{
	stopTimer();
	if (audioCallbackRegistered_)
		deviceManager_.removeAudioCallback(this);
	analysisWorker_.release();
}

void MainComponent::paint(juce::Graphics& graphics)
{
	graphics.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
	auto area = getLocalBounds().reduced(10);
	auto controls = area.removeFromBottom(36);
	deviceButton_.setBounds(controls.removeFromLeft(110));
	logarithmicButton_.setBounds(controls.removeFromLeft(130));
	horizontalButton_.setBounds(controls.removeFromLeft(160));
	statusLabel_.setBounds(controls);

	area.removeFromBottom(8);
	deviceSelector_.setBounds(area);
	spectrogram_.setBounds(area);
}

void MainComponent::initialiseAudio()
{
	const auto error = deviceManager_.initialiseWithDefaultDevices(2, 0);
	if (error.isNotEmpty()) {
		statusLabel_.setText(error, juce::dontSendNotification);
		return;
	}

	deviceManager_.addAudioCallback(this);
	audioCallbackRegistered_ = true;
	statusLabel_.setText("Listening to the default audio input", juce::dontSendNotification);
}

void MainComponent::timerCallback()
{
	spectrogram_.refreshData();
}

void MainComponent::audioDeviceIOCallbackWithContext(
	const float* const* inputChannelData,
	int numInputChannels,
	float* const* outputChannelData,
	int numOutputChannels,
	int numSamples,
	const juce::AudioIODeviceCallbackContext&)
{
	analysisWorker_.enqueue(inputChannelData, numInputChannels, numSamples);
	for (int channel = 0; channel < numOutputChannels; ++channel) {
		if (outputChannelData[channel] != nullptr)
			juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
	}
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
	analysisWorker_.prepare(device != nullptr ? device->getCurrentSampleRate() : 0.0);
}

void MainComponent::audioDeviceStopped()
{
	analysisWorker_.release();
}
