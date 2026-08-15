/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/


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
	return juce::isPowerOfTwo(width) && juce::isPowerOfTwo(height);
}

void OpenGLFloatTexture::load(const GLfloat *data, int width, int height, int row) 
{
	jassert(data != nullptr);
	jassert(textureID_ != 0);
	jassert(width > 0 && height > 0);
	jassert(row >= 0 && row + height <= height_);
	if (data == nullptr || textureID_ == 0 || width <= 0 || height <= 0 || row < 0 || row + height > height_)
		return;

	bind();
	JUCE_CHECK_OPENGL_ERROR
	
	juce::gl::glTexSubImage2D(juce::gl::GL_TEXTURE_2D, 0, 0, row, width, height, juce::gl::GL_RED, juce::gl::GL_FLOAT, data);
	JUCE_CHECK_OPENGL_ERROR
}

void OpenGLFloatTexture::create(const int w, const int h, const GLfloat * pixels)
{
	jassert(w > 0 && h > 0);
	if (w <= 0 || h <= 0)
		return;

	context_ = juce::OpenGLContext::getCurrentContext();
	jassert(context_ != nullptr);

	if (textureID_ == 0)
	{
		JUCE_CHECK_OPENGL_ERROR
		juce::gl::glGenTextures(1, &textureID_);
		juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, textureID_);
		juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
		juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
		GLint swizzleMask[] = { juce::gl::GL_RED, juce::gl::GL_RED, juce::gl::GL_RED, juce::gl::GL_RED };
		juce::gl::glTexParameteriv(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		// Frequency must not wrap between Nyquist and DC. Only the time/history
		// dimension is circular.
		juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
		juce::gl::glTexParameteri(juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_REPEAT);
		JUCE_CHECK_OPENGL_ERROR
	}
	else
	{
		juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, textureID_);
		JUCE_CHECK_OPENGL_ERROR;
	}

	juce::gl::glPixelStorei(juce::gl::GL_UNPACK_ALIGNMENT, 1);
	JUCE_CHECK_OPENGL_ERROR

	width_ = getAllowedTextureSize(w);
	height_ = getAllowedTextureSize(h);

	if (width_ != w || height_ != h)
	{
		juce::gl::glTexImage2D(juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_R32F, width_, height_, 0, juce::gl::GL_RED, juce::gl::GL_FLOAT, nullptr);
		juce::gl::glTexSubImage2D(juce::gl::GL_TEXTURE_2D, 0, 0, 0, w, h, juce::gl::GL_RED, juce::gl::GL_FLOAT, pixels);
	}
	else
	{
		juce::gl::glTexImage2D(juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_R32F, w, h, 0, juce::gl::GL_RED, juce::gl::GL_FLOAT, pixels);
	}

	JUCE_CHECK_OPENGL_ERROR
}

void OpenGLFloatTexture::release()
{
	if (textureID_ != 0)
	{
		if (context_ == juce::OpenGLContext::getCurrentContext())
		{
			juce::gl::glDeleteTextures(1, &textureID_);
			textureID_ = 0;
			width_ = 0;
			height_ = 0;
		}
		else
		{
			DBG("OpenGLFloatTexture must be released while its owning context is current");
		}
	}
}

void OpenGLFloatTexture::bind() const
{
	juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, textureID_);
}

void OpenGLFloatTexture::unbind() const
{
	juce::gl::glBindTexture(juce::gl::GL_TEXTURE_2D, 0);
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

