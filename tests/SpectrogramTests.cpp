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

void fillBinCentredSine(juce::AudioBuffer<float>& buffer, int fftSize, int bin)
{
	for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
		const auto value = std::sin(juce::MathConstants<double>::twoPi
			* static_cast<double>(bin * sample) / static_cast<double>(fftSize));
		for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
			buffer.setSample(channel, sample, static_cast<float>(value));
	}
}

int peakBin(const float* spectrum, int spectrumSize)
{
	const auto peak = std::max_element(spectrum + 1, spectrum + spectrumSize);
	return static_cast<int>(std::distance(spectrum, peak));
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
	fillBinCentredSine(buffer, analyzer.fftSize(), expectedBin);

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
	return expect(peakBin(spectrum.data(), static_cast<int>(spectrum.size())) == expectedBin,
		"bin-centred sine should peak in the expected bin")
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

bool testSpectrumFrameHistoryOrderAndWraparound()
{
	constexpr int fftOrder = 8;
	constexpr int fftSize = 1 << fftOrder;
	Spectrogram analyzer(fftOrder, fftSize);
	analyzer.prepare(48000.0);
	juce::AudioBuffer<float> frame(1, fftSize);
	const auto framesToProduce = Spectrogram::spectrumHistoryCapacity + 3;

	for (int frameIndex = 0; frameIndex < framesToProduce; ++frameIndex) {
		const auto bin = 2 + frameIndex % 100;
		fillBinCentredSine(frame, fftSize, bin);
		if (!expect(analyzer.process({ &frame, 0, frame.getNumSamples() }) == 1,
			"each complete non-overlapping window should produce one spectrum frame")) {
			return false;
		}
	}

	std::vector<float> frames(static_cast<size_t>(
		analyzer.spectrumSize() * Spectrogram::spectrumHistoryCapacity));
	std::uint64_t copiedThrough = 0;
	const auto copiedRows = analyzer.copySpectrumFramesAfter(
		0, frames.data(), static_cast<int>(frames.size()), &copiedThrough);
	if (!expect(copiedRows == Spectrogram::spectrumHistoryCapacity,
		"history should retain exactly its bounded capacity after wraparound")
		|| !expect(copiedThrough == static_cast<std::uint64_t>(framesToProduce),
			"history should report the newest copied sequence")) {
		return false;
	}

	for (int copiedRow = 0; copiedRow < copiedRows; ++copiedRow) {
		const auto originalFrameIndex = framesToProduce - copiedRows + copiedRow;
		const auto expectedBin = 2 + originalFrameIndex % 100;
		const auto* spectrum = frames.data() + copiedRow * analyzer.spectrumSize();
		if (!expect(peakBin(spectrum, analyzer.spectrumSize()) == expectedBin,
			"wrapped history rows should remain in chronological order")) {
			return false;
		}
	}

	std::vector<float> newestTwo(static_cast<size_t>(analyzer.spectrumSize() * 2));
	std::uint64_t newestSequence = 0;
	if (!expect(analyzer.copySpectrumFramesAfter(
		0, newestTwo.data(), static_cast<int>(newestTwo.size()), &newestSequence) == 2,
		"a small destination should receive only the newest rows")) {
		return false;
	}
	for (int copiedRow = 0; copiedRow < 2; ++copiedRow) {
		const auto originalFrameIndex = framesToProduce - 2 + copiedRow;
		const auto expectedBin = 2 + originalFrameIndex % 100;
		if (!expect(peakBin(newestTwo.data() + copiedRow * analyzer.spectrumSize(),
			analyzer.spectrumSize()) == expectedBin,
			"truncated history should preserve chronological order")) {
			return false;
		}
	}

	return expect(newestSequence == copiedThrough,
		"truncated history should still report the newest copied sequence")
		&& expect(analyzer.copySpectrumFramesAfter(
			copiedThrough, frames.data(), static_cast<int>(frames.size())) == 0,
			"already consumed spectrum frames should not be copied again");
}
}

int main()
{
	const auto passed = testSilence() && testBinCentredSine() && testResetAndOverflow()
		&& testSpectrumFrameHistoryOrderAndWraparound();
	if (passed)
		std::cout << "All spectrogram analyzer tests passed\n";
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
