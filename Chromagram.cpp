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
#pragma warning(pop)

class Chromagram::Impl {
public:
	Chromagram::Impl(double fs, size_t blocksize) : params_(12, 200.0 / fs, 440.0 / fs), analyzer_(params_), coefs_(analyzer_), blocksize_(blocksize) {
		size_t analysis_support = (size_t) ceil(analyzer_.analysis_support());
		size_t synthesis_support = (size_t) ceil(analyzer_.synthesis_support());
		ignoreUnused(analysis_support, synthesis_support);
	}

	void runAnalyze(AudioBuffer<float> &buffer, size_t blocksize) {
		analyzer_.analyze(buffer.getReadPointer(0), 0, blocksize, coefs_);

		gaborator::sample_index_t firstSample, lastSample;
		analyzer_.get_coef_bounds(coefs_, firstSample, lastSample);
		ignoreUnused(firstSample, lastSample);

		// Special case - we have only one slice in the coefs output, as we ran exactly blocksize samples through it!
		std::vector<std::pair<double, float>> spectrum;

		// Iterate over the global band numbers (0 highest band)
		for (auto band = analyzer_.bands_begin(); band < analyzer_.bands_end(); band++) {
			// Calculate octave and sub-band from global band
			int octave;
			unsigned int subband;
			analyzer_.bno_split(band, octave, subband, true);
			double frequency = analyzer_.bandpass_band_ff(band);

			// Access the samples available for this octave, subband pair (this would be one row in the chromoscope)
			if (coefs_.octaves[octave].slices.has_index(0)) {
				auto c = coefs_.octaves[octave].slices.get_existing(0);
				std::complex<float> *band_data = c->bands[subband];
				if (band_data) {
					spectrum.push_back(std::make_pair(48000.0 * frequency, band_data->real()));
				}
			}
		}
	}

	gaborator::parameters params_;
	gaborator::analyzer<float> analyzer_;
	gaborator::coefs<float> coefs_;
	size_t blocksize_;

	CriticalSection lock;
};

Chromagram::Chromagram(double samplerate, size_t blocksize, std::function<void()> updateCallback) : fifo_(2, 4 * (int) blocksize), blocksize_(blocksize)
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

		impl_->runAnalyze(buffer, blocksize_);
		//updateCallback_();
	}
}

void Chromagram::getData(float *out)
{
	ScopedLock sl(lock);
	ignoreUnused(out);
}

