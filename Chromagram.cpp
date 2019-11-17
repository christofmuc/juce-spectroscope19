/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "Chromagram.h"

// Define is for Visual Studio
#define _USE_MATH_DEFINES
#pragma warning(push)
#pragma warning(disable: 4244 4146 4099 4100)
// Trying the AGPL library Gaborator from gaborator.com
#include "gaborator/gaborator.h"
#include "gaborator/render.h"
#pragma warning(pop)

class Chromagram::Impl {
public:
	Chromagram::Impl(double fs, size_t blocksize) : params_(12, 20.0 / fs, 440.0 / fs), analyzer_(params_), coefs_(analyzer_), blocksize_(blocksize) {
		size_t analysis_support = (size_t) ceil(analyzer_.analysis_support());
		size_t synthesis_support = (size_t) ceil(analyzer_.synthesis_support());
		ignoreUnused(analysis_support, synthesis_support);
	}

	void runAnalyze(AudioBuffer<float> &buffer, size_t blocksize, std::vector<float> &amplitudes, int &outX, int &outY) {
		// Streaming mode requires us to discard old history
		//gaborator::forget_before(analyzer_, coefs_, 1024);
		coefs_ = gaborator::coefs<float>(analyzer_);

		analyzer_.analyze(buffer.getReadPointer(0), 0, blocksize, coefs_);

		gaborator::sample_index_t firstSample, lastSample;
		analyzer_.get_coef_bounds(coefs_, firstSample, lastSample);
		ignoreUnused(firstSample, lastSample);

		// Use the render function of gaborator, even if we should render only a single column
		int64_t src_x_origin = 0;
		int64_t src_y_origin = analyzer_.bandpass_bands_begin();
		int x_scale_exp = 10; //TODO - this needs to be mapped with "hop"?
		int y_scale_exp = 0; // One pixel per frequency band
		int64_t dst_x0 = 0; 
		int64_t dst_y0 = 0;
		int64_t dst_x1 = blocksize_ >> x_scale_exp;
		int64_t dst_y1 = (analyzer_.bandpass_bands_end() - analyzer_.bandpass_bands_begin()) >> y_scale_exp;

		// We are now ready to render the spectrogram, producing a vector of floating - point amplitude values, one per pixel.
		// Although this is stored as a 1 - dimensional vector of floats, its contents should be interpreted as a 2 - dimensional rectangular array
		// of(y1 - y0) rows of(x1 - x0) columns each, with the row indices increasing towards lower frequencies and column indices increasing towards later sampling times.
		outX = (int) (dst_x1 - dst_x0);
		outY = (int) (dst_y1 - dst_y0);
		amplitudes.resize(outX * outY, 0.0);
		gaborator::render_p2scale(
			analyzer_,
			coefs_,
			src_x_origin, src_y_origin,
			dst_x0, dst_x1, x_scale_exp,
			dst_y0, dst_y1, y_scale_exp,
			amplitudes.data());
		
		jassert(dst_x1 == 1);
	}

	gaborator::parameters params_;
	gaborator::analyzer<float> analyzer_;
	gaborator::coefs<float> coefs_;
	size_t blocksize_;

	CriticalSection lock;
};

Chromagram::Chromagram(double samplerate, size_t blocksize, std::function<void()> updateCallback) : 
	updateCallback_(updateCallback), fifo_(2, 4 * (int) blocksize), blocksize_(blocksize), height_(0)
{
	impl_ = std::make_unique<Chromagram::Impl>(samplerate, blocksize);
}

Chromagram::~Chromagram()
{
}

void Chromagram::newData(const AudioSourceChannelInfo& data)
{
	fifo_.addToFifo(data);

	// Check if there is enough data available for the next block
	if (fifo_.availableSamples() >= blocksize_) {
		// Read from fifo
		AudioBuffer<float> buffer(2, (int) blocksize_);
		fifo_.readFromFifo(&buffer, (int) blocksize_);
		{
			ScopedLock sl(lock);
			int widthRendered;
			int heightRendered;
			impl_->runAnalyze(buffer, blocksize_, data_, widthRendered, heightRendered);
			jassert(widthRendered == 1);
			height_ = heightRendered;
		}
		updateCallback_();
	}
}

int Chromagram::height() const
{
	return height_;
}

void Chromagram::getData(float *out)
{
	ScopedLock sl(lock);
	// zero fuzzy zeros, do gamma with simple sqrtf and add gain
	float eps = 1 / 65536.0f;
	float gain = 15;
	for (auto &value : data_) {
		if (value < eps)
			value = 0.0f;
		else
			value = gain * sqrtf(value);
	}
	std::copy(data_.data(), data_.data() + data_.size(), out);
}

