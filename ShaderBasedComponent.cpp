/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "ShaderBasedComponent.h"

ShaderBasedComponent::ShaderBasedComponent() : isRunning_(false)
{
	context_.setOpenGLVersionRequired(OpenGLContext::OpenGLVersion::openGL3_2);
	context_.setRenderer(this);
	context_.attachTo(*this);
}

ShaderBasedComponent::~ShaderBasedComponent()
{
	setContinuousRedrawing(false);
	context_.detach();
}

void ShaderBasedComponent::setContinuousRedrawing(bool run)
{
	context_.setContinuousRepainting(run);
	isRunning_ = run;
}

bool ShaderBasedComponent::isRunning() const
{
	return isRunning_;
}

String ShaderBasedComponent::loadShader(String const &filename) {
	File fileToRead = File::getCurrentWorkingDirectory().getChildFile(filename);
	if (!fileToRead.existsAsFile()) {
		jassert(false);
		return "";
	}
	return fileToRead.loadFileAsString();
}

std::shared_ptr<OpenGLShaderProgram::Uniform> ShaderBasedComponent::createUniform(OpenGLContext& openGLContext, OpenGLShaderProgram& shaderProgram, const char* uniformName)
{
	if (openGLContext.extensions.glGetUniformLocation(shaderProgram.getProgramID(), uniformName) < 0)
		return nullptr;

	return std::make_shared<OpenGLShaderProgram::Uniform>(shaderProgram, uniformName);
}

