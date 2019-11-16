/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Spectrogram.h"

const int kFFTOrder = 11;
const int kFFTSize = 1 << kFFTOrder;
const int kFFTBufferSize = kFFTSize * 2;
const int kWindowSize = kFFTSize;
const int kRingBufferReadSize = 2 * kFFTSize; // We read that much data from the ring buffer 
const int kHopSize = kFFTSize/16;

float kMinusPerFrame = 2.0f;

Spectrogram::Spectrogram(std::function<void()> updateCallback) :
	fifo_(2, kFFTSize * 4),
	readBuffer_(2, kRingBufferReadSize),
	forwardFFT_(kFFTOrder),
	window_(kWindowSize, dsp::WindowingFunction<float>::hann, true),
	updateCallback_(updateCallback),
	inputDataAvailable_(0)
{
	// Reserve data
	inputData_.resize(kFFTBufferSize, 0.0f); // Sample stream
	windowedData_.resize(kFFTBufferSize);
	fft_.resize(kFFTBufferSize);
	peakData_.resize(kFFTBufferSize, -100.0f);

	std::vector<float> windowSum(kWindowSize, 1.0f);
	window_.multiplyWithWindowingTable(windowSum.data(), kWindowSize);
	sum_ = 0;
	for (float s : windowSum) {
		sum_ += s;
	}
}

Spectrogram::~Spectrogram()
{
}

int Spectrogram::fftSize() const
{
	return kFFTSize;
}

void Spectrogram::newData(const AudioSourceChannelInfo& data)
{
	fifo_.addToFifo(data);

	// Check if there is enough data available for the next block
	if (fifo_.availableSamples() >= kHopSize) {
		prepareBufferForSpectrum();
		updateCallback_();
	}
}

void Spectrogram::getData(float *out)
{
	ScopedLock sl(lock);
	std::copy(fft_.data(), fft_.data() + kFFTSize / 2, out);
}

float * Spectrogram::peakHoldData()
{
	return peakData_.data();
}

void Spectrogram::prepareBufferForSpectrum()
{
	// Get the next kHopSize samples that are ready
	readBuffer_.clear();
	fifo_.readFromFifo(&readBuffer_, kHopSize);

	if (inputDataAvailable_ < kFFTSize) {
		// Not enough data, append newest hop and return to wait for new data
		for (int i = 0; i < 1 /*readBuffer_.getNumChannels()*/; ++i)
		{
			FloatVectorOperations::add(inputData_.data() + inputDataAvailable_, readBuffer_.getReadPointer(i, 0), kHopSize);
		}
		inputDataAvailable_ += kHopSize;
		return;
	}

	// In this case, the buffer is already full and we can just append the new hop data
	// First, move one hop forward
	for (int i = kHopSize; i < kFFTSize; i++) inputData_[i - kHopSize] = inputData_[i];
	for (int i = 1; i <= kHopSize; i++) inputData_[kFFTSize - i] = 0.0f;

	// Sum channels together, appending the new hop
	
	for (int i = 0; i < 1 /*readBuffer_.getNumChannels()*/; ++i)
	{
		FloatVectorOperations::add(inputData_.data() + kFFTSize - kHopSize, readBuffer_.getReadPointer(i, 0), kHopSize);
	}
	//FloatVectorOperations::multiply(&fftData_[0], 1.0f/readBuffer_.getNumChannels(), (int) kFFTSize);

	// Check our setup
	jassert(inputData_.size() == (1 << kFFTOrder) * 2);
	jassert(kFFTSize >= kWindowSize);

	//int hN = kFFTSize / 2 + 1;			// Size of positive spectrum including sample 0
	int hM1 = (kWindowSize + 1) / 2;	// half analysis window rounded
	int hM2 = (kWindowSize) / 2;		// half analysis window floored

	// Window incoming data
	std::copy(inputData_.data(), inputData_.data() + kWindowSize, windowedData_.begin());
	window_.multiplyWithWindowingTable(windowedData_.data(), kWindowSize);
	//FloatVectorOperations::multiply(windowedData_.data(), 1.0f / sum_, kWindowSize);

	// Lock the FFT data section
	ScopedLock sl(lock);

	// Zero-phase windowing
	FloatVectorOperations::clear(fft_.data(), kFFTSize * 2);
	for (int i = 0; i < hM1; i++) fft_[i] = windowedData_[hM2 + i];
	for (int i = 0; i < hM2; i++) fft_[kFFTSize - hM2 + i] = windowedData_[i];

	// Calculate the FFT
	forwardFFT_.performFrequencyOnlyForwardTransform(fft_.data());

	for (int i = 0; i < kFFTSize / 2; i++) {
		if (fft_[i] < 1e-6f) fft_[i] = 1e-6f;
	}

	//FloatVectorOperations::multiply(&fftData_[0], (kFFTSize/2.0f), (int) kFFTSize/2);
	for (int i = 0; i < kFFTSize / 2; i++) {
		fft_[i] = std::log10(fft_[i]);
	}
	FloatVectorOperations::multiply(fft_.data(), 20.0f, kFFTSize / 2);

	float max = -100.0;
	for (float f : fft_) if (f > max) max = f;

	// peak hold
	for (int i = 0; i < kFFTSize / 2; i++) {
		if (fft_[i] > peakData_[i]) {
			peakData_[i] = fft_[i];
		}
		else {
			peakData_[i] -= kMinusPerFrame;
			if (peakData_[i] < -100.0f) {
				peakData_[i] = 100.0f;
			}
		}
	}
}
