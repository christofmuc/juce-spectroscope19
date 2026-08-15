#include "PitchTracker.h"
#include "Spectrogram.h"
#include "WaterfallTimeline.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

bool approximatelyEqual(float left, float right)
{
	return std::abs(left - right) < 0.00001f;
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

void fillSine(juce::AudioBuffer<float>& buffer, double frequency, double sampleRate, double& phase)
{
	for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
		const auto value = static_cast<float>(0.6 * std::sin(phase));
		phase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
		if (phase >= juce::MathConstants<double>::twoPi)
			phase -= juce::MathConstants<double>::twoPi;
		for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
			buffer.setSample(channel, sample, value);
	}
}

int peakBin(const float* spectrum, int spectrumSize)
{
	const auto peak = std::max_element(spectrum + 1, spectrum + spectrumSize);
	return static_cast<int>(std::distance(spectrum, peak));
}

float circularOutputDistance(int first, int second)
{
	const auto direct = std::abs(first - second);
	return static_cast<float>(std::min(direct, PitchTracker::outputBinCount - direct));
}

int pitchFieldPeak(const std::vector<float>& field)
{
	return static_cast<int>(std::distance(field.begin(),
		std::max_element(field.begin(), field.end())));
}

float pitchFieldAtSemitonesFromA(const std::vector<float>& field, float semitonesFromA)
{
	const auto octavePosition = semitonesFromA / 12.0f - std::floor(semitonesFromA / 12.0f);
	const auto index = static_cast<int>(std::floor(
		octavePosition * static_cast<float>(PitchTracker::outputBinCount)))
		% PitchTracker::outputBinCount;
	return field[static_cast<std::size_t>(index)];
}

void processPitchSignal(PitchTracker& tracker, const std::vector<double>& frequencies,
	int blockCount, std::vector<float>& field)
{
	constexpr int blockSize = 512;
	constexpr double sampleRate = 48000.0;
	std::vector<float> samples(blockSize);
	std::vector<double> phases(frequencies.size(), 0.0);
	for (int block = 0; block < blockCount; ++block) {
		for (int sample = 0; sample < blockSize; ++sample) {
			auto value = 0.0;
			for (std::size_t frequencyIndex = 0; frequencyIndex < frequencies.size(); ++frequencyIndex) {
				value += std::sin(phases[frequencyIndex]);
				phases[frequencyIndex] += juce::MathConstants<double>::twoPi
					* frequencies[frequencyIndex] / sampleRate;
				if (phases[frequencyIndex] >= juce::MathConstants<double>::twoPi)
					phases[frequencyIndex] -= juce::MathConstants<double>::twoPi;
			}
			samples[static_cast<std::size_t>(sample)] = frequencies.empty()
				? 0.0f : static_cast<float>(0.6 * value / static_cast<double>(frequencies.size()));
		}
		tracker.process(samples.data(), blockSize);
		tracker.calculate(field.data(), static_cast<int>(field.size()));
	}
}

bool testPitchTrackerStablePitchAndDetuning()
{
	constexpr double concertA = 440.0;
	const auto c4 = concertA * 0.5 * 0.5 * std::pow(2.0, 3.0 / 12.0);
	PitchTracker tracker;
	tracker.prepare(48000.0, static_cast<float>(concertA));
	std::vector<float> field(PitchTracker::outputBinCount);
	processPitchSignal(tracker, { c4 }, 80, field);

	const auto expectedC = PitchTracker::outputBinCount / 4;
	if (!expect(circularOutputDistance(pitchFieldPeak(field), expectedC) <= 3.0f,
		"tracked C should peak at its octave-relative pitch position")
		|| !expect(*std::max_element(field.begin(), field.end()) > 0.7f,
			"a sustained pure tone should become a confident tracked note")) {
		return false;
	}

	tracker.reset();
	constexpr double detuningCents = 20.0;
	const auto sharpC = c4 * std::pow(2.0, detuningCents / 1200.0);
	processPitchSignal(tracker, { sharpC }, 80, field);
	const auto expectedSharpC = static_cast<int>(std::lround(
		(3.0 + detuningCents / 100.0) / 12.0 * PitchTracker::outputBinCount));
	return expect(circularOutputDistance(pitchFieldPeak(field), expectedSharpC) <= 4.0f,
		"interpolated tracked pitch should follow a sharp input between note centres");
}

bool testPitchTrackerChordAndRelease()
{
	constexpr double concertA = 440.0;
	auto frequencyForSemitones = [concertA](double semitonesFromA) {
		return concertA * 0.25 * std::pow(2.0, semitonesFromA / 12.0);
	};
	PitchTracker tracker;
	tracker.prepare(48000.0, static_cast<float>(concertA));
	std::vector<float> field(PitchTracker::outputBinCount);
	processPitchSignal(tracker,
		{ frequencyForSemitones(3.0), frequencyForSemitones(7.0), frequencyForSemitones(10.0) },
		100, field);

	if (!expect(pitchFieldAtSemitonesFromA(field, 3.0f) > 0.25f,
		"C in a C-major chord should be tracked")
		|| !expect(pitchFieldAtSemitonesFromA(field, 7.0f) > 0.25f,
			"E in a C-major chord should be tracked")
		|| !expect(pitchFieldAtSemitonesFromA(field, 10.0f) > 0.25f,
			"G in a C-major chord should be tracked")) {
		return false;
	}

	const auto strengthBeforeSilence = pitchFieldAtSemitonesFromA(field, 3.0f);
	processPitchSignal(tracker, {}, 1, field);
	if (!expect(pitchFieldAtSemitonesFromA(field, 3.0f) > strengthBeforeSilence * 0.75f,
		"a tracked note should not disappear on the first silent frame")) {
		return false;
	}

	processPitchSignal(tracker, {}, 100, field);
	return expect(*std::max_element(field.begin(), field.end()) < 0.05f,
		"tracked notes should eventually release to silence");
}

bool testPitchTrackerRejectsBroadbandNoise()
{
	PitchTracker tracker;
	tracker.prepare(48000.0, 440.0f);
	std::vector<float> field(PitchTracker::outputBinCount);
	std::vector<float> noise(512);
	std::uint32_t randomState = 0x12345678u;
	for (int block = 0; block < 150; ++block) {
		for (auto& sample : noise) {
			randomState = randomState * 1664525u + 1013904223u;
			const auto normalised = static_cast<float>((randomState >> 8) & 0x00ffffffu)
				/ static_cast<float>(0x00ffffffu);
			sample = 0.3f * (normalised * 2.0f - 1.0f);
		}
		tracker.process(noise.data(), static_cast<int>(noise.size()));
		tracker.calculate(field.data(), static_cast<int>(field.size()));
	}

	return expect(*std::max_element(field.begin(), field.end()) < 0.2f,
		"stationary broadband noise should not become a strongly tracked note");
}

bool testSpectrogramPublishesTrackedPitch()
{
	constexpr double sampleRate = 48000.0;
	constexpr float concertA = 444.0f;
	const auto c3 = static_cast<double>(concertA) * 0.25 * std::pow(2.0, 3.0 / 12.0);
	Spectrogram analyzer;
	analyzer.setConcertAHz(concertA);
	analyzer.prepare(sampleRate);
	juce::AudioBuffer<float> block(1, analyzer.hopSize());
	auto phase = 0.0;
	for (int iteration = 0; iteration < 100; ++iteration) {
		fillSine(block, c3, sampleRate, phase);
		analyzer.process({ &block, 0, block.getNumSamples() });
	}

	std::vector<float> pitchClass(static_cast<size_t>(analyzer.pitchClassSize()));
	std::uint64_t pitchSequence = 0;
	return expect(analyzer.copyLatestPitchClass(
		pitchClass.data(), static_cast<int>(pitchClass.size()), &pitchSequence),
		"spectrogram should publish the latest tracked pitch field")
		&& expect(pitchSequence == analyzer.sequence(),
			"latest pitch and FFT rows should share a sequence")
		&& expect(pitchFieldAtSemitonesFromA(pitchClass, 3.0f) > 0.7f,
			"spectrogram should feed audio and its A4 reference into the pitch tracker");
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
		return std::isfinite(value) && approximatelyEqual(value, analyzer.floorDb());
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

	std::vector<float> synchronizedSpectra(frames.size());
	std::vector<float> synchronizedPitch(static_cast<size_t>(
		analyzer.pitchClassSize() * Spectrogram::spectrumHistoryCapacity));
	std::uint64_t synchronizedSequence = 0;
	if (!expect(analyzer.copyAnalysisFramesAfter(0,
		synchronizedSpectra.data(), static_cast<int>(synchronizedSpectra.size()),
		synchronizedPitch.data(), static_cast<int>(synchronizedPitch.size()),
		&synchronizedSequence) == Spectrogram::spectrumHistoryCapacity,
		"combined history should copy an aligned pitch row for every FFT row")
		|| !expect(synchronizedSequence == copiedThrough,
			"combined FFT and pitch history should report the same sequence")) {
		return false;
	}
	if (!expect(std::all_of(synchronizedPitch.begin(), synchronizedPitch.end(), [](float value) {
		return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
	}), "published pitch confidence should stay finite and normalized")) {
		return false;
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

bool testWaterfallTimelineMapping()
{
	constexpr int rowCount = 8;
	constexpr int newestRow = 2;
	constexpr int expectedRows[] { 3, 4, 5, 6, 7, 0, 1, 2 };

	if (!expect(spectroscope::waterfall::nextRow(6, rowCount) == 7,
		"waterfall row should advance without wrapping early")
		|| !expect(spectroscope::waterfall::nextRow(7, rowCount) == 0,
			"waterfall row should wrap after the last row")
		|| !expect(approximatelyEqual(
			spectroscope::waterfall::oldestRowCentre(newestRow, rowCount), 3.5f / 8.0f),
			"history should start at the centre of the row after the newest row")
		|| !expect(approximatelyEqual(
			spectroscope::waterfall::historySpan(rowCount), 7.0f / 8.0f),
			"history should span each row exactly once")) {
		return false;
	}

	for (int displayColumn = 0; displayColumn < rowCount; ++displayColumn) {
		const auto progress = static_cast<float>(displayColumn) / static_cast<float>(rowCount - 1);
		const auto coordinate = spectroscope::waterfall::textureCoordinate(
			progress, newestRow, rowCount);
		const auto wrappedCoordinate = coordinate - std::floor(coordinate);
		const auto sampledRow = static_cast<int>(std::floor(wrappedCoordinate * rowCount));
		if (!expect(sampledRow == expectedRows[displayColumn],
			"horizontal history should visit oldest-to-newest rows in chronological order")) {
			return false;
		}
	}

	return true;
}
}

int main()
{
	const auto passed = testPitchTrackerStablePitchAndDetuning() && testPitchTrackerChordAndRelease()
		&& testPitchTrackerRejectsBroadbandNoise()
		&& testSpectrogramPublishesTrackedPitch()
		&& testSilence() && testBinCentredSine() && testResetAndOverflow()
		&& testSpectrumFrameHistoryOrderAndWraparound() && testWaterfallTimelineMapping();
	if (passed)
		std::cout << "All spectrogram analyzer tests passed\n";
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
