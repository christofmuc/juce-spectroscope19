#include "Spectrogram.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool expect(bool condition, const std::string& message)
{
	if (!condition)
		std::cerr << "FAILED: " << message << '\n';
	return condition;
}

bool testSilence()
{
	Spectrogram analyzer;
	analyzer.prepare(48000.0);
	juce::AudioBuffer<float> buffer(2, analyzer.fftSize());
	buffer.clear();

	const auto rows = analyzer.process({ &buffer, 0, buffer.getNumSamples() });
	std::vector<float> spectrum(static_cast<size_t>(analyzer.spectrumSize()));
	if (!expect(rows == 1, "one FFT row should be produced after one full window")
		|| !expect(analyzer.copyLatestSpectrum(spectrum.data(), static_cast<int>(spectrum.size())),
			"silence spectrum should be available")) {
		return false;
	}

	return expect(std::all_of(spectrum.begin(), spectrum.end(), [&](float value) {
		return std::isfinite(value) && value == analyzer.floorDb();
	}), "silence should remain at the configured finite floor");
}

bool testBinCentredSine()
{
	Spectrogram analyzer;
	constexpr double sampleRate = 48000.0;
	constexpr int expectedBin = 64;
	analyzer.prepare(sampleRate);

	juce::AudioBuffer<float> buffer(2, analyzer.fftSize());
	const auto frequency = sampleRate * static_cast<double>(expectedBin) / static_cast<double>(analyzer.fftSize());
	for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
		const auto value = std::sin(juce::MathConstants<double>::twoPi * frequency
			* static_cast<double>(sample) / sampleRate);
		buffer.setSample(0, sample, static_cast<float>(value));
		buffer.setSample(1, sample, static_cast<float>(value));
	}

	if (!expect(analyzer.process({ &buffer, 0, buffer.getNumSamples() }) == 1,
		"sine input should produce one FFT row")) {
		return false;
	}

	std::vector<float> spectrum(static_cast<size_t>(analyzer.spectrumSize()));
	if (!expect(analyzer.copyLatestSpectrum(spectrum.data(), static_cast<int>(spectrum.size())),
		"sine spectrum should be available")) {
		return false;
	}

	const auto peak = std::max_element(spectrum.begin() + 1, spectrum.end());
	const auto peakBin = static_cast<int>(std::distance(spectrum.begin(), peak));
	return expect(peakBin == expectedBin, "bin-centred sine should peak in the expected bin")
		&& expect(*peak > -0.1f && *peak <= 0.0f, "full-scale bin-centred sine should normalize near 0 dBFS");
}

bool testResetAndOverflow()
{
	Spectrogram analyzer;
	analyzer.prepare(48000.0);
	juce::AudioBuffer<float> largeBuffer(1, analyzer.fftSize() * 10);
	largeBuffer.clear();
	analyzer.process({ &largeBuffer, 0, largeBuffer.getNumSamples() });

	if (!expect(analyzer.droppedSamples() > 0, "oversized input should be dropped rather than blocking"))
		return false;

	analyzer.reset();
	std::vector<float> spectrum(static_cast<size_t>(analyzer.spectrumSize()));
	return expect(analyzer.sequence() == 0, "reset should clear the published sequence")
		&& expect(analyzer.droppedSamples() == 0, "reset should clear the dropped-sample counter")
		&& expect(!analyzer.copyLatestSpectrum(spectrum.data(), static_cast<int>(spectrum.size())),
			"reset should invalidate the previous spectrum");
}

bool testOverlappingFrameHistory()
{
	Spectrogram analyzer;
	analyzer.prepare(48000.0);
	juce::AudioBuffer<float> initialWindow(1, analyzer.fftSize());
	initialWindow.clear();
	if (!expect(analyzer.process({ &initialWindow, 0, initialWindow.getNumSamples() }) == 1,
		"initial window should produce one spectrum frame")) {
		return false;
	}

	juce::AudioBuffer<float> overlappingHops(1, analyzer.hopSize() * 3);
	overlappingHops.clear();
	if (!expect(analyzer.process({ &overlappingHops, 0, overlappingHops.getNumSamples() }) == 3,
		"each overlapping hop should produce a spectrum frame")) {
		return false;
	}

	std::vector<float> frames(static_cast<size_t>(analyzer.spectrumSize() * 3));
	std::uint64_t copiedThrough = 0;
	const auto copiedRows = analyzer.copySpectrumFramesAfter(
		1, frames.data(), static_cast<int>(frames.size()), &copiedThrough);
	return expect(copiedRows == 3, "all overlapping frames should be retained for the display")
		&& expect(copiedThrough == 4, "frame history should report the newest copied sequence")
		&& expect(analyzer.copySpectrumFramesAfter(
			copiedThrough, frames.data(), static_cast<int>(frames.size())) == 0,
			"already consumed spectrum frames should not be copied again");
}
}

int main()
{
	const auto passed = testSilence() && testBinCentredSine() && testResetAndOverflow()
		&& testOverlappingFrameHistory();
	if (passed)
		std::cout << "All spectrogram analyzer tests passed\n";
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
