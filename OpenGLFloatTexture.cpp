/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include <GL/glew.h>

#include "OpenGLFloatTexture.h"
#include "OpenGLHelpers.h"

OpenGLFloatTexture::OpenGLFloatTexture()
	: textureID_(0), width_(0), height_(0), context_(nullptr)
{
}

OpenGLFloatTexture::~OpenGLFloatTexture()
{
	release();
}

bool OpenGLFloatTexture::isValidSize(int width, int height)
{
	return isPowerOfTwo(width) && isPowerOfTwo(height);
}

void OpenGLFloatTexture::load(const GLfloat *data, int width, int height, int row) 
{
	bind();
	JUCE_CHECK_OPENGL_ERROR
	
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, width, height, GL_RED, GL_FLOAT, data);
	JUCE_CHECK_OPENGL_ERROR
}

void OpenGLFloatTexture::create(const int w, const int h, const GLfloat * pixels)
{
	context_ = OpenGLContext::getCurrentContext();
	jassert(context_ != nullptr);

	if (textureID_ == 0)
	{
		JUCE_CHECK_OPENGL_ERROR
		glGenTextures(1, &textureID_);
		glBindTexture(GL_TEXTURE_2D, textureID_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_RED };
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		JUCE_CHECK_OPENGL_ERROR
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, textureID_);
		JUCE_CHECK_OPENGL_ERROR;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	JUCE_CHECK_OPENGL_ERROR

	width_ = getAllowedTextureSize(w);
	height_ = getAllowedTextureSize(h);

	if (width_ != w || height_ != h)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width_, height_, 0, GL_RED, GL_FLOAT, nullptr);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_FLOAT, pixels);
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, pixels);
	}

	JUCE_CHECK_OPENGL_ERROR
}

void OpenGLFloatTexture::release()
{
	if (textureID_ != 0)
	{
		jassert(OpenGLContext::getCurrentContext() == currentContext);
		if (context_ == OpenGLContext::getCurrentContext())
		{
			glDeleteTextures(1, &textureID_);
			textureID_ = 0;
			width_ = 0;
			height_ = 0;
		}
	}
}

void OpenGLFloatTexture::bind() const
{
	glBindTexture(GL_TEXTURE_2D, textureID_);
}

void OpenGLFloatTexture::unbind() const
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint OpenGLFloatTexture::getTextureID() const noexcept
{
	return textureID_;
}

int OpenGLFloatTexture::getWidth() const noexcept
{
	return width_;
}

int OpenGLFloatTexture::getHeight() const noexcept
{
	return height_;
}

