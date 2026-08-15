/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Spectrogram.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace {
int validatedFftOrder(int order)
{
	return juce::jlimit(5, 16, order);
}

int fftSizeForOrder(int order)
{
	return 1 << validatedFftOrder(order);
}

int validatedHopSize(int requestedHopSize, int fftSize)
{
	if (requestedHopSize <= 0)
		return fftSize / 4;

	return juce::jlimit(1, fftSize, requestedHopSize);
}
}

Spectrogram::Spectrogram(int fftOrder, int requestedHopSize, float requestedFloorDb)
	: fftOrder_(validatedFftOrder(fftOrder))
	, fftSize_(fftSizeForOrder(fftOrder_))
	, hopSize_(validatedHopSize(requestedHopSize, fftSize_))
	, floorDb_(juce::jmin(-1.0f, requestedFloorDb))
	, fifoCapacity_(fftSize_ * 8)
	, fifo_(fifoCapacity_)
	, fifoBuffer_(1, fifoCapacity_)
	, hopBuffer_(1, hopSize_)
	, forwardFFT_(fftOrder_)
	, window_(static_cast<size_t>(fftSize_), juce::dsp::WindowingFunction<float>::hann, false)
	, inputData_(static_cast<size_t>(fftSize_), 0.0f)
	, windowedData_(static_cast<size_t>(fftSize_), 0.0f)
	, fftWork_(static_cast<size_t>(fftSize_ * 2), 0.0f)
	, nextSpectrum_(static_cast<size_t>(fftSize_ / 2), floorDb_)
	, publishedSpectra_(static_cast<size_t>(fftSize_ / 2 * spectrumHistoryCapacity), floorDb_)
{
	std::vector<float> windowValues(static_cast<size_t>(fftSize_), 1.0f);
	window_.multiplyWithWindowingTable(windowValues.data(), static_cast<size_t>(fftSize_));
	const auto windowSum = std::accumulate(windowValues.begin(), windowValues.end(), 0.0f);
	windowMagnitudeScale_ = windowSum > 0.0f ? 2.0f / windowSum : 1.0f;
}

int Spectrogram::fftSize() const noexcept
{
	return fftSize_;
}

int Spectrogram::spectrumSize() const noexcept
{
	return fftSize_ / 2;
}

int Spectrogram::hopSize() const noexcept
{
	return hopSize_;
}

float Spectrogram::floorDb() const noexcept
{
	return floorDb_;
}

void Spectrogram::prepare(double newSampleRate)
{
	sampleRate_.store(newSampleRate > 0.0 ? newSampleRate : 0.0, std::memory_order_relaxed);
	reset();
}

void Spectrogram::reset()
{
	fifo_.reset();
	fifoBuffer_.clear();
	hopBuffer_.clear();
	std::fill(inputData_.begin(), inputData_.end(), 0.0f);
	std::fill(windowedData_.begin(), windowedData_.end(), 0.0f);
	std::fill(fftWork_.begin(), fftWork_.end(), 0.0f);
	std::fill(nextSpectrum_.begin(), nextSpectrum_.end(), floorDb_);
	inputDataAvailable_ = 0;
	droppedSamples_.store(0, std::memory_order_relaxed);

	{
		const juce::ScopedLock lock(publishedSpectrumLock_);
		std::fill(publishedSpectra_.begin(), publishedSpectra_.end(), floorDb_);
		sequence_.store(0, std::memory_order_release);
	}
}

int Spectrogram::process(const juce::AudioSourceChannelInfo& data)
{
	if (data.buffer == nullptr || data.numSamples <= 0)
		return 0;

	writeInput(data);

	int rowsProduced = 0;
	while (fifo_.getNumReady() >= hopSize_) {
		readHop();
		appendHop();

		if (inputDataAvailable_ == fftSize_) {
			calculateSpectrum();
			++rowsProduced;
		}
	}

	return rowsProduced;
}

bool Spectrogram::copyLatestSpectrum(float* destination, int destinationSize, std::uint64_t* copiedSequence) const
{
	if (destination == nullptr || destinationSize < spectrumSize())
		return false;

	const juce::ScopedLock lock(publishedSpectrumLock_);
	const auto currentSequence = sequence_.load(std::memory_order_relaxed);
	if (currentSequence == 0)
		return false;

	const auto row = static_cast<size_t>((currentSequence - 1)
		% static_cast<std::uint64_t>(spectrumHistoryCapacity));
	const auto rowStart = publishedSpectra_.begin() + static_cast<std::ptrdiff_t>(row * spectrumSize());
	std::copy_n(rowStart, spectrumSize(), destination);
	if (copiedSequence != nullptr)
		*copiedSequence = currentSequence;
	return true;
}

int Spectrogram::copySpectrumFramesAfter(std::uint64_t afterSequence, float* destination,
	int destinationSize, std::uint64_t* copiedThroughSequence) const
{
	if (destination == nullptr || destinationSize < spectrumSize())
		return 0;

	const auto destinationRows = destinationSize / spectrumSize();
	const juce::ScopedLock lock(publishedSpectrumLock_);
	const auto newestSequence = sequence_.load(std::memory_order_relaxed);
	if (newestSequence == 0 || newestSequence <= afterSequence)
		return 0;

	const auto oldestRetainedSequence = newestSequence > spectrumHistoryCapacity
		? newestSequence - spectrumHistoryCapacity + 1
		: 1;
	auto firstSequence = juce::jmax(afterSequence + 1, oldestRetainedSequence);
	const auto availableRows = newestSequence - firstSequence + 1;
	if (availableRows > static_cast<std::uint64_t>(destinationRows))
		firstSequence = newestSequence - static_cast<std::uint64_t>(destinationRows) + 1;

	const auto copiedRows = static_cast<int>(newestSequence - firstSequence + 1);
	for (int destinationRow = 0; destinationRow < copiedRows; ++destinationRow) {
		const auto sourceSequence = firstSequence + static_cast<std::uint64_t>(destinationRow);
		const auto sourceRow = static_cast<size_t>((sourceSequence - 1)
			% static_cast<std::uint64_t>(spectrumHistoryCapacity));
		const auto source = publishedSpectra_.begin()
			+ static_cast<std::ptrdiff_t>(sourceRow * spectrumSize());
		std::copy_n(source, spectrumSize(), destination + destinationRow * spectrumSize());
	}

	if (copiedThroughSequence != nullptr)
		*copiedThroughSequence = newestSequence;
	return copiedRows;
}

std::uint64_t Spectrogram::sequence() const noexcept
{
	return sequence_.load(std::memory_order_acquire);
}

std::uint64_t Spectrogram::droppedSamples() const noexcept
{
	return droppedSamples_.load(std::memory_order_relaxed);
}

double Spectrogram::sampleRate() const noexcept
{
	return sampleRate_.load(std::memory_order_relaxed);
}

int Spectrogram::writeInput(const juce::AudioSourceChannelInfo& data)
{
	const auto availableChannels = data.buffer->getNumChannels();
	const auto validStart = juce::jlimit(0, data.buffer->getNumSamples(), data.startSample);
	const auto availableSamples = data.buffer->getNumSamples() - validStart;
	const auto requestedSamples = juce::jlimit(0, availableSamples, data.numSamples);

	int start1 = 0;
	int size1 = 0;
	int start2 = 0;
	int size2 = 0;
	fifo_.prepareToWrite(requestedSamples, start1, size1, start2, size2);
	const auto writtenSamples = size1 + size2;
	const auto gain = availableChannels > 0 ? 1.0f / static_cast<float>(availableChannels) : 0.0f;

	auto writeSection = [&](int destinationStart, int sourceStart, int count) {
		if (count <= 0)
			return;

		fifoBuffer_.clear(0, destinationStart, count);
		for (int channel = 0; channel < availableChannels; ++channel)
			fifoBuffer_.addFrom(0, destinationStart, *data.buffer, channel, sourceStart, count, gain);
	};

	writeSection(start1, validStart, size1);
	writeSection(start2, validStart + size1, size2);
	fifo_.finishedWrite(writtenSamples);

	if (writtenSamples < requestedSamples)
		droppedSamples_.fetch_add(static_cast<std::uint64_t>(requestedSamples - writtenSamples), std::memory_order_relaxed);

	return writtenSamples;
}

void Spectrogram::readHop()
{
	int start1 = 0;
	int size1 = 0;
	int start2 = 0;
	int size2 = 0;
	fifo_.prepareToRead(hopSize_, start1, size1, start2, size2);

	hopBuffer_.clear();
	if (size1 > 0)
		hopBuffer_.copyFrom(0, 0, fifoBuffer_, 0, start1, size1);
	if (size2 > 0)
		hopBuffer_.copyFrom(0, size1, fifoBuffer_, 0, start2, size2);

	fifo_.finishedRead(size1 + size2);
}

void Spectrogram::appendHop()
{
	const auto* hop = hopBuffer_.getReadPointer(0);
	if (inputDataAvailable_ < fftSize_) {
		const auto samplesToCopy = juce::jmin(hopSize_, fftSize_ - inputDataAvailable_);
		std::copy_n(hop, samplesToCopy, inputData_.begin() + inputDataAvailable_);
		inputDataAvailable_ += samplesToCopy;
		return;
	}

	std::memmove(inputData_.data(), inputData_.data() + hopSize_,
		static_cast<size_t>(fftSize_ - hopSize_) * sizeof(float));
	std::copy_n(hop, hopSize_, inputData_.end() - hopSize_);
}

void Spectrogram::calculateSpectrum()
{
	std::copy(inputData_.begin(), inputData_.end(), windowedData_.begin());
	window_.multiplyWithWindowingTable(windowedData_.data(), static_cast<size_t>(fftSize_));

	std::fill(fftWork_.begin(), fftWork_.end(), 0.0f);
	std::copy(windowedData_.begin(), windowedData_.end(), fftWork_.begin());
	forwardFFT_.performFrequencyOnlyForwardTransform(fftWork_.data());

	for (int bin = 0; bin < spectrumSize(); ++bin) {
		const auto normalizedMagnitude = fftWork_[static_cast<size_t>(bin)] * windowMagnitudeScale_;
		nextSpectrum_[static_cast<size_t>(bin)] = juce::jlimit(
			floorDb_, 0.0f, juce::Decibels::gainToDecibels(normalizedMagnitude, floorDb_));
	}

	{
		const juce::ScopedLock lock(publishedSpectrumLock_);
		const auto nextSequence = sequence_.load(std::memory_order_relaxed) + 1;
		const auto destinationRow = static_cast<size_t>((nextSequence - 1)
			% static_cast<std::uint64_t>(spectrumHistoryCapacity));
		auto destination = publishedSpectra_.begin()
			+ static_cast<std::ptrdiff_t>(destinationRow * spectrumSize());
		std::copy(nextSpectrum_.begin(), nextSpectrum_.end(), destination);
		sequence_.store(nextSequence, std::memory_order_release);
	}
}
