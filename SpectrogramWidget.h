/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "OpenGLFloatTexture.h"
#include "ShaderBasedComponent.h"
#include "Spectrogram.h"

#include <atomic>
#include <array>
#include <cstdint>

class SpectrogramWidget final : public ShaderBasedComponent {
public:
	explicit SpectrogramWidget(std::weak_ptr<Spectrogram> spectrogram);
	~SpectrogramWidget() override;

	void newOpenGLContextCreated() override;
	void openGLContextClosing() override;
	void renderOpenGL() override;
	void resized() override;

	// Safe to call from a non-OpenGL thread. The actual snapshot transfer and
	// history update happen on the OpenGL render thread.
	void refreshData();

	void setXAxis(bool logAxis);
	void setHorizontalMode(bool horizontal);
	void setPitchColourMode(bool enabled);
	void setConcertAHz(float frequencyHz);

private:
	std::shared_ptr<juce::OpenGLTexture> createColorLookupTexture();
	std::shared_ptr<OpenGLFloatTexture> createDataTexture(int width, int height, float initialValue);
	void publishStatus(juce::String statusText);
	void releaseOpenGLResources();
	int pullAvailableSpectra();

	std::weak_ptr<Spectrogram> spectrogram_;

	GLuint vertexBuffer_ { 0 };
	GLuint elements_ { 0 };
	std::shared_ptr<juce::OpenGLTexture> textureLUT_;
	std::shared_ptr<OpenGLFloatTexture> spectrumData_;
	std::shared_ptr<OpenGLFloatTexture> spectrumHistory_;

	std::unique_ptr<juce::OpenGLShaderProgram> shader_;
	std::unique_ptr<juce::OpenGLShaderProgram::Attribute> position_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> resolution_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> audioSampleData_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> lutTexture_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> waterfallTexture_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> waterfallStartUniform_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> waterfallSpanUniform_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> logXAxis_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uUpperHalfPercentage_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uHorizontal_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uPitchColourMode_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uSampleRate_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uConcertAHz_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uMinimumFrequencyHz_;
	std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uSpectrumTexelWidth_;

	std::vector<GLfloat> fftData_;
	std::vector<GLfloat> pendingSpectra_;
	std::array<int, 32> pendingTextureRows_ {};
	int waterfallPosition_ { 0 };
	std::uint64_t lastSequence_ { 0 };
	std::atomic<bool> refreshRequested_ { true };
	std::atomic<bool> xLogAxis_ { true };
	std::atomic<bool> horizontal_ { false };
	std::atomic<bool> pitchColourMode_ { false };
	std::atomic<float> concertAHz_ { 440.0f };
	float upperHalfPercentage_ { 0.618f };
	bool openGLReady_ { false };

	juce::Label statusLabel_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramWidget)
};

// Temporary source compatibility for the original misspelled public name.
using SpectogramWidget = SpectrogramWidget;
