/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include <GL/glew.h>

#ifdef WIN32
#include <GL/wglew.h>
#endif

#include "SpectrogramWidget.h"

#include "OpenGLHelpers.h"

#include "BinaryResources.h"

SpectogramWidget::SpectogramWidget(std::weak_ptr<Spectrogram> spectrogram) :
	spectrogram_(spectrogram)
{
	// Setup GUI Overlay Label: Status of Shaders, compiler errors, etc.
	addAndMakeVisible(statusLabel_);
	statusLabel_.setJustificationType(Justification::topLeft);
	statusLabel_.setFont(Font(14.0f));

	if (!spectrogram_.expired()) {
		fftData_.resize(spectrogram_.lock()->fftSize() / 2 * 512); // History of the last 512 FFTs
	}
	else {
		// Not so good, where is your spectrogram instance gone?
		jassert(false);
	}
}

void SpectogramWidget::newOpenGLContextCreated()
{
	static bool glewInitialized = false;
	if (!glewInitialized) {
		GLenum err = glewInit();
		ignoreUnused(err);
		glewInitialized = true;
	}

	std::string vertexShader;
	std::string fragmentShader;

	if (GLEW_VERSION_3_3) {
		vertexShader = std::string((const char *)oscilloscope_vert_glsl, oscilloscope_vert_glsl_size);
		fragmentShader = std::string((const char *)oscilloscope_frag_glsl, oscilloscope_frag_glsl_size);
	}
	else {
		std::cerr << "System does not support OpenGL 3.3, fatal!" << std::endl;
		exit(-1);
	}

#ifdef WIN32
	// Try to turn on VSync, if you are on Windows and your driver supports it! I had to update my NVidia driver
	if (WGLEW_EXT_swap_control) {
		//wglSwapIntervalEXT(1);
		JUCE_CHECK_OPENGL_ERROR
	}
#endif
	bool worked = context_.setSwapInterval(1);
	jassert(worked);
	ignoreUnused(worked);

	shader_ = std::make_unique<OpenGLShaderProgram>(context_);

	String statusText;
	if (shader_->addVertexShader(vertexShader)
		&& shader_->addFragmentShader(fragmentShader)
		&& shader_->link())
	{
		shader_->use();

		resolution_ = createUniform(context_, *shader_, "resolution");
		waterfallUniform_ = createUniform(context_, *shader_, "waterfallPosition");
		uUpperHalfPercentage_ = createUniform(context_, *shader_, "upperHalfPercentage");
		audioSampleData_ = createUniform(context_, *shader_, "audioSampleData");
		waterfallTexture_ = createUniform(context_, *shader_, "waterfall");
		lutTexture_ = createUniform(context_, *shader_, "lutTexture");
		logXAxis_ = createUniform(context_, *shader_, "xAxisLog");

		textureLUT_ = createColorLookupTexture();
		if (!spectrogram_.expired()) {
			spectrumData_ = createDataTexture(spectrogram_.lock()->fftSize() / 2, 1);
			spectrumHistory_ = createDataTexture(spectrogram_.lock()->fftSize() / 2, 512);
		}
		JUCE_CHECK_OPENGL_ERROR

			statusText = "GLSL: v" + String(OpenGLShaderProgram::getLanguageVersion(), 2);
	}
	else
	{
		statusText = shader_->getLastError();
	}

	context_.extensions.glGenBuffers(1, &vertexBuffer_);
	context_.extensions.glGenBuffers(1, &elements_);

	MessageManager::callAsync([this, statusText]() {
		statusLabel_.setText(statusText, dontSendNotification);
	});
}

void SpectogramWidget::openGLContextClosing()
{
	if (textureLUT_) textureLUT_->release();
	if (spectrumData_) spectrumData_->release();
	if (spectrumHistory_) spectrumHistory_->release();
	shader_->release();
}

std::shared_ptr<OpenGLTexture> SpectogramWidget::createColorLookupTexture() {
	std::shared_ptr<OpenGLTexture> texture = std::make_shared<OpenGLTexture>();
	PixelARGB pixels[256];
	pixels[0] = PixelARGB(255, 0, 0, 0);
	pixels[255] = PixelARGB(255, 255, 255, 0);
	for (int i = 1; i < 32; i++) {
		pixels[i] = PixelARGB(255, 255, (uint8)(255 - i * 8), 0);
		pixels[255 - i] = PixelARGB(255, 255, (uint8)(255 - i * 8), 0);
	}
	for (int i = 0; i < 96; i++) {
		pixels[32 + i] = PixelARGB(255, (uint8)(255 - (i * 4 / 3)), 0, (uint8)(i * 4 / 3));
		pixels[223 - i] = PixelARGB(255, (uint8)(255 - (i * 4 / 3)), 0, (uint8)(i * 4 / 3));
	}
	for (int i = 0; i < 256; i++) {
		//pixels[i] = PixelARGB(255, i, i, i);
	}
	pixels[0] = PixelARGB(255, 0, 0, 0);
	pixels[255] = PixelARGB(255, 255, 255, 0);
	for (int i = 1; i < 64; i++) {
		pixels[255 - i] = PixelARGB(255, 255, (uint8)(255 - i * 4), 0);
	}
	for (int i = 0; i < 192; i++) {
		pixels[191 - i] = PixelARGB(255, (uint8)(128 - (i * 2 / 3)), 0, (uint8)(i * 2 / 3));
	}
	texture->bind();
	JUCE_CHECK_OPENGL_ERROR
		texture->loadARGB(pixels, 256, 1);
	JUCE_CHECK_OPENGL_ERROR
		texture->unbind();
	JUCE_CHECK_OPENGL_ERROR
		return texture;
}

std::shared_ptr<OpenGLFloatTexture> SpectogramWidget::createDataTexture(int w, int h) {
	auto texture = std::make_shared<OpenGLFloatTexture>();
	texture->bind();
	JUCE_CHECK_OPENGL_ERROR
		GLfloat *emptyPixels = new GLfloat[w * h];
	texture->create(w, h, emptyPixels);
	delete emptyPixels;
	texture->unbind();
	JUCE_CHECK_OPENGL_ERROR
		return texture;
}

void SpectogramWidget::renderOpenGL()
{
	jassert(OpenGLHelpers::isContextActive());

	auto renderingScale = (float)context_.getRenderingScale();
	glViewport(0, 0, roundToInt(renderingScale * getWidth()), roundToInt(renderingScale * getHeight()));

	OpenGLHelpers::clear(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);

	shader_->use();

	// Setup the Uniforms for use in the Shader
	resolution_->set((GLfloat)renderingScale * getWidth(), (GLfloat)renderingScale * getHeight());
	JUCE_CHECK_OPENGL_ERROR

		setUniform(lutTexture_, 0);
	setUniform(logXAxis_, xLogAxis_);
	setUniform(waterfallUniform_, waterfallPosition / 512.0f);
	setUniform(uUpperHalfPercentage_, upperHalfPercentage_);
	setUniform(audioSampleData_, 1);
	setUniform(waterfallTexture_, 2);

	// This will crash when the driver doesn't have this function. Well, we won't render anything then anyway, so what?
	context_.extensions.glActiveTexture(GL_TEXTURE0);
	textureLUT_->bind();
	JUCE_CHECK_OPENGL_ERROR

		context_.extensions.glActiveTexture(GL_TEXTURE1);
	spectrumData_->bind();
	JUCE_CHECK_OPENGL_ERROR

		context_.extensions.glActiveTexture(GL_TEXTURE2);
	spectrumHistory_->bind();
	JUCE_CHECK_OPENGL_ERROR

		if (!spectrogram_.expired() && fftData_.size() >= spectrogram_.lock()->fftSize() / 2) {
			spectrumData_->load(fftData_.data() + waterfallPosition * spectrogram_.lock()->fftSize() / 2, spectrogram_.lock()->fftSize() / 2, 1);
			spectrumHistory_->load(fftData_.data(), spectrogram_.lock()->fftSize() / 2, 512);
		}

	// Read a block that is big enough so we can fill our viewport with a triggered wave of the latest acquired audio
	// Define Vertices for a Square (the view plane)
	GLfloat vertices[] = {
		1.0f,   1.0f,  0.0f,  // Top Right
		1.0f,  -1.0f,  0.0f,  // Bottom Right
		-1.0f, -1.0f,  0.0f,  // Bottom Left
		-1.0f,  1.0f,  0.0f   // Top Left
	};
	// Define Which Vertex Indexes Make the Square
	GLuint indices[] = {  // Note that we start from 0!
		0, 1, 3,   // First Triangle
		1, 2, 3    // Second Triangle
	};

	// VBO (Vertex Buffer Object) - Bind and Write to Buffer
	context_.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
	context_.extensions.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

	// EBO (Element Buffer Object) - Bind and Write to Buffer
	context_.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_);
	context_.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STREAM_DRAW);

	// Setup Vertex Attributes
	context_.extensions.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	context_.extensions.glEnableVertexAttribArray(0);

	// Draw Vertices
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // For EBO's (Element Buffer Objects) (Indices)

	// Reset the element buffers so child Components draw correctly
	context_.extensions.glBindBuffer(GL_ARRAY_BUFFER, 0);
	context_.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	if (textureLUT_) textureLUT_->unbind();
	if (spectrumData_) spectrumData_->unbind();
	if (spectrumHistory_) spectrumHistory_->unbind();
	JUCE_CHECK_OPENGL_ERROR
}

void SpectogramWidget::resized()
{
	statusLabel_.setBounds(getLocalBounds().reduced(4).removeFromTop(75));
}

void SpectogramWidget::refreshData()
{
	// Don't call this too early when no OpenGL context has been initialized
	if (spectrumData_ && spectrumHistory_ && !spectrogram_.expired()) {
		waterfallPosition = (waterfallPosition + 1) % 512;
		spectrogram_.lock()->getData(fftData_.data() + spectrogram_.lock()->fftSize() / 2 * waterfallPosition);
	}
}

void SpectogramWidget::setXAxis(bool logAxis)
{
	xLogAxis_ = logAxis ? 1 : 0;
}

