/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_opengl/juce_opengl.h>

#include "Fifo.h"

class Spectrogram  {
public:
	Spectrogram(std::function<void()> updateCallback);
	virtual ~Spectrogram();

	int fftSize() const;
	void getData(float *out);
	float *peakHoldData();

	void newData(const juce::AudioSourceChannelInfo &data);

private:
	void prepareBufferForSpectrum();

	AudioBufferFiFo<float> fifo_;
	juce::AudioBuffer<GLfloat> readBuffer_;

	juce::dsp::FFT forwardFFT_;
	juce::dsp::WindowingFunction<float> window_;
	std::vector<GLfloat> inputData_; 
	int inputDataAvailable_;
	std::vector<GLfloat> windowedData_;
	std::vector<GLfloat> fft_;
	std::vector<GLfloat> peakData_; 

	float sum_;

	std::function<void()> updateCallback_;

	juce::CriticalSection lock;
};