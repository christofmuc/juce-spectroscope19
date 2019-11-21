/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

// 
// After I spend two nights with the JUCE OpenGL multi-threading code and trying to find out why the 
// **** it deadlocked on exit, I found the solution. Because I will easily forget it again, I create this helper class
// to hide the logic

template<typename T>
class ManagedOpenGLComponent : public Component {
public:
	ManagedOpenGLComponent() {
	}

	virtual ~ManagedOpenGLComponent() {
	}

	void start(T *embeddedGLComponent) {
		openGLComponent_.reset(embeddedGLComponent);
		if (openGLComponent_) {
			addAndMakeVisible(openGLComponent_.get());
			openGLComponent_->setContinuousRedrawing(true);
			resized();
		}
	}

	void stop() {
		if (openGLComponent_) {
			openGLComponent_->setContinuousRedrawing(false);
			removeChildComponent(openGLComponent_.get()); // This is critical, else you can get deadlocks in the shutdown phase
		}
		openGLComponent_.reset();
	}

	virtual void resized() override {
		if (openGLComponent_) openGLComponent_->setBounds(getLocalBounds());
	}

	T *glComponent() {
		return openGLComponent_.get();
	}

private:
	std::unique_ptr<T> openGLComponent_;
};

