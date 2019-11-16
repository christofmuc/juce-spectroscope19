/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

template <class T>
struct AudioBufferFiFo 
{
	AudioBufferFiFo(int numChannels, int numSamples) {
		abstractFifo_ = std::make_unique<AbstractFifo>(numSamples);
		audioBuffer_ = std::make_unique<AudioBuffer<float>>(numChannels, numSamples);
	}

	void addToFifo(const AudioSourceChannelInfo& bufferToFill)
	{
		jassert(bufferToFill.buffer != nullptr);
		jassert(bufferToFill.buffer->getNumChannels() == audioBuffer_->getNumChannels());

		int start1, size1, start2, size2;
		abstractFifo_->prepareToWrite(bufferToFill.numSamples, start1, size1, start2, size2);
		for (int channel = 0; channel < audioBuffer_->getNumChannels(); channel++) {
			if (size1 > 0) {
				audioBuffer_->copyFrom(channel, start1, *bufferToFill.buffer, channel, bufferToFill.startSample, size1);
				//copySomeData(myBuffer + start1, someData, size1);
			}
			if (size2 > 0) {
				audioBuffer_->copyFrom(channel, start2, *bufferToFill.buffer, channel, bufferToFill.startSample + size1, size2);
				//copySomeData(myBuffer + start2, someData + size1, size2);
			}
		}
		abstractFifo_->finishedWrite(size1 + size2);
	}

	void readFromFifo(AudioBuffer<float> *destBuffer, int numSamples)
	{
		jassert(numSamples <= abstractFifo_->getNumReady());
		jassert(destBuffer->getNumChannels() == audioBuffer_->getNumChannels());

		int start1, size1, start2, size2;
		abstractFifo_->prepareToRead(numSamples, start1, size1, start2, size2);
		for (int channel = 0; channel < audioBuffer_->getNumChannels(); channel++) {
			if (size1 > 0) {
				destBuffer->copyFrom(channel, 0, *audioBuffer_, channel, start1, size1);
				//copySomeData(someData, myBuffer + start1, size1);
			}
			if (size2 > 0) {
				destBuffer->copyFrom(channel, size1, *audioBuffer_, channel, start2, size2);
				//copySomeData(someData + size1, myBuffer + start2, size2);
			}
		}
		abstractFifo_->finishedRead(size1 + size2);
	}

	int availableSamples() {
		return abstractFifo_->getNumReady();
	}

private:
	std::unique_ptr<AbstractFifo> abstractFifo_;
	std::unique_ptr<AudioBuffer<float>> audioBuffer_;
};