/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "Spectrogram.h"

#include "OpenGLFloatTexture.h"
#include "ShaderBasedComponent.h"

class SpectogramWidget : public ShaderBasedComponent
{
public:
	SpectogramWidget(Spectrogram &spectrogram);

	// OpenGLRenderer interface
	void newOpenGLContextCreated() override;
	void openGLContextClosing() override;
	void renderOpenGL() override;

	// Override for layout
	void resized() override;

	// Call this to read the next spectrogram
	void refreshData();

	// Settings
	void setXAxis(bool logAxis);

private:
	std::shared_ptr<OpenGLTexture> createColorLookupTexture();
	std::shared_ptr<OpenGLFloatTexture> createDataTexture(int w, int h);

	Spectrogram &spectrogram_;

	GLuint vertexBuffer_, elements_;
	std::shared_ptr<OpenGLTexture> textureLUT_;
	std::shared_ptr<OpenGLFloatTexture> spectrumData_;
	std::shared_ptr<OpenGLFloatTexture> spectrumHistory_;

	std::unique_ptr<OpenGLShaderProgram> shader_;
	std::shared_ptr<OpenGLShaderProgram::Uniform> resolution_, audioSampleData_, lutTexture_, waterfallTexture_, waterfallUniform_, logXAxis_, uUpperHalfPercentage_;

	std::vector<GLfloat> fftData_; // Current line from the spectrogram
	int waterfallPosition = 0;
	int xLogAxis_ = 1;
	float upperHalfPercentage_ = 0.618f;

	Label statusLabel_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectogramWidget)
};
