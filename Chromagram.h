/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Fifo.h"

class Chromagram  {
public:
	Chromagram(double samplerate, size_t blocksize, std::function<void()> updateCallback);
	virtual ~Chromagram();

	void getData(float *out);
	void newData(const AudioSourceChannelInfo& data);

	int height() const;

private:
	std::function<void()> updateCallback_;
	AudioBufferFiFo<float> fifo_;
	std::vector<float> data_;
	int height_;
	size_t blocksize_;
	CriticalSection lock;

	// Hide implementation, we will try out different libraries for this
	class Impl;
	std::unique_ptr<Impl> impl_;
};
