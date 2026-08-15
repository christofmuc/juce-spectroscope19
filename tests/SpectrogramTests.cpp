#include "FrequencyAxis.h"
#include "PitchTracker.h"
#include "Spectrogram.h"
#include "TrackedPitch.h"
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

int pitchFieldPeak(const std::vector<float>& field)
{
	return static_cast<int>(std::distance(field.begin(),
		std::max_element(field.begin(), field.end())));
}

int pitchFieldBinForFrequency(double frequency, double concertA)
{
	const auto octavePosition = std::log2(frequency / (concertA / 8.0));
	return static_cast<int>(std::floor(octavePosition / PitchTracker::octaveCount
		* PitchTracker::outputBinCount));
}

float pitchFieldAtFrequency(const std::vector<float>& field, double frequency, double concertA)
{
	const auto index = pitchFieldBinForFrequency(frequency, concertA);
	if (index < 0 || index >= static_cast<int>(field.size()))
		return 0.0f;
	return field[static_cast<std::size_t>(index)];
}

int blocksUntilPitchVisible(PitchTracker::Preset preset, double frequency,
	float visibleConfidence = 0.15f, int maximumBlocks = 30)
{
	constexpr int blockSize = 512;
	constexpr double sampleRate = 48000.0;
	constexpr double concertA = 440.0;
	PitchTracker tracker;
	tracker.setPreset(preset);
	tracker.prepare(sampleRate, static_cast<float>(concertA));
	std::vector<float> samples(blockSize);
	std::vector<float> field(PitchTracker::outputBinCount);
	auto phase = 0.0;
	for (int block = 1; block <= maximumBlocks; ++block) {
		for (auto& sample : samples) {
			sample = static_cast<float>(0.6 * std::sin(phase));
			phase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
			if (phase >= juce::MathConstants<double>::twoPi)
				phase -= juce::MathConstants<double>::twoPi;
		}
		tracker.process(samples.data(), blockSize);
		tracker.calculate(field.data(), static_cast<int>(field.size()));
		if (pitchFieldAtFrequency(field, frequency, concertA) >= visibleConfidence)
			return block;
	}
	return maximumBlocks + 1;
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

void processHarmonicPitchSignal(PitchTracker& tracker, double fundamental,
	int blockCount, std::vector<float>& field)
{
	constexpr int blockSize = 512;
	constexpr double sampleRate = 48000.0;
	constexpr int harmonicCount = 8;
	std::array<double, harmonicCount> phases {};
	std::vector<float> samples(blockSize);
	std::uint32_t randomState = 0x87654321u;
	for (int block = 0; block < blockCount; ++block) {
		for (int sample = 0; sample < blockSize; ++sample) {
			auto value = 0.0;
			for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic) {
				value += std::sin(phases[static_cast<std::size_t>(harmonic - 1)])
					/ static_cast<double>(harmonic);
				phases[static_cast<std::size_t>(harmonic - 1)] +=
					juce::MathConstants<double>::twoPi * fundamental
					* static_cast<double>(harmonic) / sampleRate;
				if (phases[static_cast<std::size_t>(harmonic - 1)]
					>= juce::MathConstants<double>::twoPi) {
					phases[static_cast<std::size_t>(harmonic - 1)]
						-= juce::MathConstants<double>::twoPi;
				}
			}
			randomState = randomState * 1664525u + 1013904223u;
			const auto noise = static_cast<double>((randomState >> 8) & 0x00ffffffu)
				/ static_cast<double>(0x00ffffffu) * 2.0 - 1.0;
			const auto tremolo = 0.75 + 0.25 * std::sin(
				juce::MathConstants<double>::twoPi * 4.5
				* static_cast<double>(block * blockSize + sample) / sampleRate);
			samples[static_cast<std::size_t>(sample)] =
				static_cast<float>(0.35 * tremolo * value + 0.015 * noise);
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

	const auto expectedC = pitchFieldBinForFrequency(c4, concertA);
	const auto stableConfidence = *std::max_element(field.begin(), field.end());
	if (!expect(std::abs(pitchFieldPeak(field) - expectedC) <= 3,
		"tracked C should peak at its absolute log-frequency position")
		|| !expect(stableConfidence > 0.6f,
			"a sustained pure tone should become a confident tracked note (confidence "
				+ std::to_string(stableConfidence) + ")")) {
		return false;
	}

	tracker.reset();
	constexpr double detuningCents = 20.0;
	const auto sharpC = c4 * std::pow(2.0, detuningCents / 1200.0);
	processPitchSignal(tracker, { sharpC }, 80, field);
	const auto expectedSharpC = pitchFieldBinForFrequency(sharpC, concertA);
	const auto actualSharpC = pitchFieldPeak(field);
	return expect(std::abs(actualSharpC - expectedSharpC) <= 4,
		"interpolated tracked pitch should follow a sharp input between note centres (expected bin "
			+ std::to_string(expectedSharpC) + ", got " + std::to_string(actualSharpC) + ")");
}

bool testPitchTrackerDetectionLatency()
{
	const auto a1Blocks = blocksUntilPitchVisible(PitchTracker::Preset::balanced, 55.0);
	const auto a2Blocks = blocksUntilPitchVisible(PitchTracker::Preset::balanced, 110.0);
	const auto a3Blocks = blocksUntilPitchVisible(PitchTracker::Preset::balanced, 220.0);
	const auto a4Blocks = blocksUntilPitchVisible(PitchTracker::Preset::balanced, 440.0);
	std::cout << "Pitch onset hops: A1=" << a1Blocks << ", A2=" << a2Blocks
		<< ", A3=" << a3Blocks << ", A4=" << a4Blocks << '\n';
	return expect(a1Blocks <= 12,
		"A1 should become visible within 128 ms (took " + std::to_string(a1Blocks) + " hops)")
		&& expect(a2Blocks <= 8,
			"A2 should become visible within 86 ms (took " + std::to_string(a2Blocks) + " hops)")
		&& expect(a3Blocks <= 5,
			"A3 should become visible within 54 ms (took " + std::to_string(a3Blocks) + " hops)")
		&& expect(a4Blocks <= 4,
			"A4 should become visible within 43 ms (took " + std::to_string(a4Blocks) + " hops)");
}

bool testPitchTrackerPresets()
{
	PitchTracker tracker;
	if (!expect(tracker.preset() == PitchTracker::Preset::balanced,
		"balanced should remain the default tracking preset")) {
		return false;
	}
	tracker.setPreset(PitchTracker::Preset::fast);
	if (!expect(tracker.preset() == PitchTracker::Preset::fast,
		"the selected tracking preset should be observable")) {
		return false;
	}

	const auto fastBlocks = blocksUntilPitchVisible(PitchTracker::Preset::fast, 110.0);
	const auto balancedBlocks = blocksUntilPitchVisible(PitchTracker::Preset::balanced, 110.0);
	const auto stableBlocks = blocksUntilPitchVisible(PitchTracker::Preset::stable, 110.0);
	std::cout << "A2 preset onset hops: fast=" << fastBlocks
		<< ", balanced=" << balancedBlocks << ", stable=" << stableBlocks << '\n';
	if (!expect(fastBlocks <= balancedBlocks,
		"Fast should not react more slowly than Balanced")
		|| !expect(balancedBlocks <= stableBlocks,
			"Stable should not react faster than Balanced")) {
		return false;
	}

	Spectrogram analyzer;
	analyzer.setPitchTrackingPreset(PitchTracker::Preset::stable);
	return expect(analyzer.pitchTrackingPreset() == PitchTracker::Preset::stable,
		"the analyzer should expose the preset selected by its UI consumer");
}

bool testTrackedPitchMusicalValues()
{
	constexpr double concertA = 440.0;
	constexpr double frequency = 445.0;
	PitchTracker tracker;
	tracker.prepare(48000.0, static_cast<float>(concertA));
	std::vector<float> field(PitchTracker::outputBinCount);
	processPitchSignal(tracker, { frequency }, 80, field);

	std::array<spectroscope::TrackedPitch, 6> notes {};
	const auto noteCount = spectroscope::extractTrackedPitches(field.data(),
		static_cast<int>(field.size()), static_cast<float>(concertA),
		notes.data(), static_cast<int>(notes.size()));
	const auto fieldConfidence = *std::max_element(field.begin(), field.end());
	if (!expect(noteCount > 0,
		"a visible tracked field should expose musical note diagnostics (field confidence "
			+ std::to_string(fieldConfidence) + ")")) {
		return false;
	}

	const auto note = *std::max_element(notes.begin(), notes.begin() + noteCount,
		[](const auto& first, const auto& second) {
			return first.confidence < second.confidence;
		});
	const auto expectedCents = static_cast<float>(1200.0 * std::log2(frequency / concertA));
	return expect(note.midiNote == 69, "445 Hz at A4=440 should be displayed as A4")
		&& expect(std::abs(note.cents - expectedCents) < 3.0f,
			"tracked-note diagnostics should retain sub-semitone cents accuracy");
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

	const auto c3 = frequencyForSemitones(3.0);
	const auto e3 = frequencyForSemitones(7.0);
	const auto g3 = frequencyForSemitones(10.0);
	if (!expect(pitchFieldAtFrequency(field, c3, concertA) > 0.25f,
		"C in a C-major chord should be tracked")
		|| !expect(pitchFieldAtFrequency(field, e3, concertA) > 0.25f,
			"E in a C-major chord should be tracked")
		|| !expect(pitchFieldAtFrequency(field, g3, concertA) > 0.25f,
			"G in a C-major chord should be tracked")) {
		return false;
	}

	const auto strengthBeforeSilence = pitchFieldAtFrequency(field, c3, concertA);
	processPitchSignal(tracker, {}, 1, field);
	if (!expect(pitchFieldAtFrequency(field, c3, concertA) > strengthBeforeSilence * 0.75f,
		"a tracked note should not disappear on the first silent frame")) {
		return false;
	}

	processPitchSignal(tracker, {}, 100, field);
	const auto releasedStrength = *std::max_element(field.begin(), field.end());
	return expect(releasedStrength < 0.05f,
		"tracked notes should eventually release to silence (remaining confidence "
			+ std::to_string(releasedStrength) + ")");
}

bool testPitchTrackerHarmonicMusicalTone()
{
	constexpr double concertA = 440.0;
	const auto a3 = concertA * 0.5;
	PitchTracker tracker;
	tracker.prepare(48000.0, static_cast<float>(concertA));
	std::vector<float> field(PitchTracker::outputBinCount);
	processHarmonicPitchSignal(tracker, a3, 24, field);

	const auto fundamentalStrength = pitchFieldAtFrequency(field, a3, concertA);
	return expect(fundamentalStrength > 0.15f,
		"a short harmonic-rich, noisy and amplitude-modulated note should produce visible pitch confidence")
		&& expect(pitchFieldAtFrequency(field, a3 * 2.0, concertA) < fundamentalStrength * 0.35f,
			"an octave harmonic should remain grey rather than becoming another coloured note")
		&& expect(pitchFieldAtFrequency(field, a3 * 3.0, concertA) < fundamentalStrength * 0.35f,
			"a non-octave harmonic should remain grey rather than becoming another coloured note");
}

bool testPitchTrackerRejectsBroadbandNoise()
{
	for (const auto preset : { PitchTracker::Preset::fast,
		PitchTracker::Preset::balanced, PitchTracker::Preset::stable }) {
		PitchTracker tracker;
		tracker.setPreset(preset);
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

		const auto noiseConfidence = *std::max_element(field.begin(), field.end());
		if (!expect(noiseConfidence < 0.2f,
			"stationary broadband noise should not become a strongly tracked note in any preset "
			"(confidence " + std::to_string(noiseConfidence) + ")")) {
			return false;
		}
	}
	return true;
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
		&& expect(pitchFieldAtFrequency(pitchClass, c3, concertA) > 0.7f,
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

bool testFrequencyAxisMapping()
{
	constexpr double sampleRate = 48000.0;
	constexpr double minimumFrequency = sampleRate / 2048.0;
	const auto logMinimum = spectroscope::frequency_axis::normalisedPosition(
		minimumFrequency, sampleRate, minimumFrequency, true);
	const auto logNyquist = spectroscope::frequency_axis::normalisedPosition(
		sampleRate * 0.5, sampleRate, minimumFrequency, true);
	const auto a4 = spectroscope::frequency_axis::normalisedPosition(
		440.0, sampleRate, minimumFrequency, true);
	const auto linearMidpoint = spectroscope::frequency_axis::normalisedPosition(
		sampleRate * 0.25, sampleRate, minimumFrequency, false);

	return expect(approximatelyEqual(logMinimum, 0.0f),
		"the logarithmic overlay should align its minimum with the shader's left edge")
		&& expect(approximatelyEqual(logNyquist, 1.0f),
			"the logarithmic overlay should align Nyquist with the shader's right edge")
		&& expect(a4 > 0.0f && a4 < 1.0f,
			"an audible tracked pitch should map inside the frequency axis")
		&& expect(approximatelyEqual(linearMidpoint, 0.5f),
			"the linear overlay should use the same normalized coordinate as the shader")
		&& expect(approximatelyEqual(
			spectroscope::frequency_axis::horizontalScreenPosition(0.0f), 1.0f),
			"horizontal mode should place the lowest frequency at the screen bottom")
		&& expect(approximatelyEqual(
			spectroscope::frequency_axis::horizontalScreenPosition(1.0f), 0.0f),
			"horizontal mode should place Nyquist at the screen top");
}
}

int main()
{
	const auto passed = testPitchTrackerStablePitchAndDetuning() && testPitchTrackerDetectionLatency()
		&& testPitchTrackerPresets()
		&& testTrackedPitchMusicalValues()
		&& testPitchTrackerChordAndRelease()
		&& testPitchTrackerHarmonicMusicalTone()
		&& testPitchTrackerRejectsBroadbandNoise()
		&& testSpectrogramPublishesTrackedPitch()
		&& testSilence() && testBinCentredSine() && testResetAndOverflow()
		&& testSpectrumFrameHistoryOrderAndWraparound() && testWaterfallTimelineMapping()
		&& testFrequencyAxisMapping();
	if (passed)
		std::cout << "All spectrogram analyzer tests passed\n";
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
