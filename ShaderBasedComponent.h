/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

#include "OpenGLHelpers.h"

class ShaderBasedComponent : public Component, public OpenGLRenderer {
public:
	ShaderBasedComponent();
	virtual ~ShaderBasedComponent();

	void setContinuousRedrawing(bool run);
	bool isRunning() const;

	static String loadShader(String const &filename);
	static std::shared_ptr<OpenGLShaderProgram::Uniform> createUniform(OpenGLContext& openGLContext, OpenGLShaderProgram& shaderProgram, const char* uniformName);

	template <typename T>
	static void setUniform(std::shared_ptr<OpenGLShaderProgram::Uniform> uniform, T const &value) {
		if (uniform) {
			uniform->set(value);
			JUCE_CHECK_OPENGL_ERROR
		}
	}

protected:
	OpenGLContext context_;
	bool isRunning_;
};
