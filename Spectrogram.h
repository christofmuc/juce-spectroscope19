/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Fifo.h"

class Spectrogram  {
public:
	Spectrogram(std::function<void()> updateCallback);
	virtual ~Spectrogram();

	int fftSize() const;
	void getData(float *out);
	float *peakHoldData();

	void newData(const AudioSourceChannelInfo& data);

private:
	void prepareBufferForSpectrum();

	AudioBufferFiFo<float> fifo_;
	AudioBuffer<GLfloat> readBuffer_;

	dsp::FFT forwardFFT_;
	dsp::WindowingFunction<float> window_;
	std::vector<GLfloat> inputData_; 
	int inputDataAvailable_;
	std::vector<GLfloat> windowedData_;
	std::vector<GLfloat> fft_;
	std::vector<GLfloat> peakData_; 

	float sum_;

	std::function<void()> updateCallback_;

	CriticalSection lock;
};