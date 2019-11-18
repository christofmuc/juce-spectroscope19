/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ChromagramMath.h"

#include "Chromagram.h"

class ChromagramMath::Impl {
public:
	ChromagramMath::Impl(double fs, size_t blocksize) : chromagram_((int) blocksize, (int) fs), blocksize_(blocksize) {
	}

	void runAnalyze(AudioBuffer<float> &buffer, size_t blocksize, std::vector<float> &amplitudes, int &outX, int &outY) {
		double *values = new double[blocksize];
		const float *readptr = buffer.getReadPointer(0);
		double *writeptr = values;
		for (int i = 0; i < blocksize; i++) *(writeptr++) = *(readptr++);
		chromagram_.processAudioFrame(values);
		if (chromagram_.isReady()) {
			std::vector<double> chroma = chromagram_.getChromagram();
			amplitudes.clear();
			for (double value : chroma) amplitudes.push_back((float) value);
			outX = 1;
			outY = (int) chroma.size();
		}
		else {
			outX = 0; 
			outY = 0;
		}
		delete[] values;
	}

	Chromagram chromagram_;
	size_t blocksize_;

	CriticalSection lock;
};

ChromagramMath::ChromagramMath(double samplerate, size_t blocksize, std::function<void()> updateCallback) : 
	updateCallback_(updateCallback), fifo_(2, 4 * (int) blocksize), blocksize_(blocksize), height_(0)
{
	impl_ = std::make_unique<ChromagramMath::Impl>(samplerate, blocksize);
}

ChromagramMath::~ChromagramMath()
{
}

void ChromagramMath::newData(const AudioSourceChannelInfo& data)
{
	fifo_.addToFifo(data);

	// Check if there is enough data available for the next block
	if (fifo_.availableSamples() >= blocksize_) {
		// Read from fifo
		AudioBuffer<float> buffer(2, (int) blocksize_);
		fifo_.readFromFifo(&buffer, (int) blocksize_);
		int widthRendered;
		{
			ScopedLock sl(lock);
			int heightRendered;
			impl_->runAnalyze(buffer, blocksize_, data_, widthRendered, heightRendered);
			height_ = heightRendered;
		}
		if (widthRendered > 0) {
			updateCallback_();
		}
	}
}

int ChromagramMath::height() const
{
	return height_;
}

void ChromagramMath::getData(float *out)
{
	ScopedLock sl(lock);
	// zero fuzzy zeros, do gamma with simple sqrtf and add gain
/*	float eps = 1 / 65536.0f;
	float gain = 15;
	for (auto &value : data_) {
		if (value < eps)
			value = 0.0f;
		else
			value = gain * sqrtf(value);
	}*/
	std::copy(data_.data(), data_.data() + data_.size(), out);
}

