/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#pragma once

#include "JuceHeader.h"

class OpenGLFloatTexture
{
public:
	OpenGLFloatTexture();
	~OpenGLFloatTexture();

	void create(int w, int h, const GLfloat *pixels);
	void load(const GLfloat * data, int width, int height, int row = 0);

	void release();
	void bind() const;
	void unbind() const;

	static bool isValidSize(int width, int height);

	GLuint getTextureID() const noexcept;
	int getWidth() const noexcept;
	int getHeight() const noexcept;

private:
	GLuint textureID_;
	int width_;
	int height_;
	OpenGLContext* context_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLFloatTexture)
};
