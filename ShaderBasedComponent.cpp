/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ShaderBasedComponent.h"

ShaderBasedComponent::ShaderBasedComponent()
{
#if JUCE_MAJOR_VERSION >= 9
	context_.setPreferredVersion({ 3, 2 });
#else
	context_.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
#endif
	context_.setRenderer(this);
	context_.setContinuousRepainting(false);
	context_.attachTo(*this);
}

ShaderBasedComponent::~ShaderBasedComponent()
{
	setContinuousRedrawing(false);
	context_.detach();
	context_.setRenderer(nullptr);
}

void ShaderBasedComponent::setContinuousRedrawing(bool run)
{
	context_.setContinuousRepainting(run);
	isRunning_.store(run, std::memory_order_relaxed);
}

bool ShaderBasedComponent::isRunning() const
{
	return isRunning_.load(std::memory_order_relaxed);
}

juce::String ShaderBasedComponent::loadShader(juce::String const& filename)
{
	juce::File fileToRead = juce::File::getCurrentWorkingDirectory().getChildFile(filename);
	if (!fileToRead.existsAsFile()) {
		jassert(false);
		return "";
	}
	return fileToRead.loadFileAsString();
}

std::shared_ptr<juce::OpenGLShaderProgram::Uniform> ShaderBasedComponent::createUniform(juce::OpenGLContext& openGLContext, juce::OpenGLShaderProgram& shaderProgram,
    const char* uniformName)
{
	if (openGLContext.extensions.glGetUniformLocation(shaderProgram.getProgramID(), uniformName) < 0)
		return nullptr;

	return std::make_shared<juce::OpenGLShaderProgram::Uniform>(shaderProgram, uniformName);
}

