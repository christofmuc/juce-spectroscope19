/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include <juce_core/juce_core.h>

#include <atomic>

#include "OpenGLHelpers.h"

class ShaderBasedComponent : public juce::Component, public juce::OpenGLRenderer {
public:
	ShaderBasedComponent();
	~ShaderBasedComponent() override;

	void setContinuousRedrawing(bool run);
	bool isRunning() const;

	static juce::String loadShader(juce::String const& filename);
	static std::shared_ptr<juce::OpenGLShaderProgram::Uniform> createUniform(juce::OpenGLContext& openGLContext, juce::OpenGLShaderProgram& shaderProgram,
	    const char* uniformName);

	template <typename T> static void setUniform(std::shared_ptr<juce::OpenGLShaderProgram::Uniform> uniform, T const& value)
	{
		if (uniform) {
			uniform->set(value);
			JUCE_CHECK_OPENGL_ERROR
		}
	}

protected:
	juce::OpenGLContext context_;
	std::atomic<bool> isRunning_ { false };
};
