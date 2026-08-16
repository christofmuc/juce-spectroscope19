/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "SpectrogramWidget.h"

#include "BinaryResources.h"
#include "FrequencyAxis.h"
#include "OpenGLHelpers.h"
#include "TrackedNoteDisplay.h"
#include "WaterfallTimeline.h"

#include <algorithm>
#include <cmath>

using namespace juce;
using namespace juce::gl;

namespace {
constexpr int waterfallRows = 512;
constexpr int maximumRowsPerRefresh = 32;

#if JUCE_DEBUG
void assertTextureBound(OpenGLContext& context, GLenum textureUnit, GLuint expectedTexture)
{
	context.extensions.glActiveTexture(textureUnit);
	GLint actualTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &actualTexture);
	jassert(static_cast<GLuint>(actualTexture) == expectedTexture);
}
#endif
}

class SpectrogramWidget::TrackedNotesOverlay final : public Component, private Timer {
public:
	TrackedNotesOverlay()
	{
		setInterceptsMouseClicks(false, false);
	}

	void setNotes(std::array<spectroscope::TrackedPitch, 6> newNotes, int newNoteCount,
		double newSampleRate, double newMinimumFrequencyHz, std::uint64_t sequence)
	{
		const auto validNoteCount = jlimit(0, static_cast<int>(newNotes.size()), newNoteCount);
		noteDisplay_.update(
			newNotes.data(), validNoteCount, Time::getMillisecondCounterHiRes());
		noteHistory_.update(newNotes.data(), validNoteCount, sequence);
		sampleRate_ = newSampleRate;
		minimumFrequencyHz_ = newMinimumFrequencyHz;
		startTimerHz(30);
		repaint();
	}

	void clearNotes()
	{
		noteDisplay_.clear();
		noteHistory_.clear();
		latestSequence_.store(0, std::memory_order_relaxed);
		stopTimer();
		repaint();
	}

	void setAxisMode(bool logarithmic, bool horizontal)
	{
		logarithmic_ = logarithmic;
		horizontal_ = horizontal;
		startTimerHz(30);
		repaint();
	}

	void advanceLatestSequence(std::uint64_t sequence) noexcept
	{
		latestSequence_.store(sequence, std::memory_order_relaxed);
	}

	void paint(Graphics& graphics) override
	{
		if (sampleRate_ <= 0.0)
			return;
		if (horizontal_) {
			paintHorizontalHistory(graphics);
			return;
		}

		std::array<spectroscope::TrackedNoteDisplay::Entry,
			spectroscope::TrackedNoteDisplay::capacity> entries {};
		const auto entryCount = noteDisplay_.visibleEntries(Time::getMillisecondCounterHiRes(),
			entries.data(), static_cast<int>(entries.size()));
		for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
			const auto& entry = entries[static_cast<std::size_t>(entryIndex)];
			const auto& note = entry.note;
			const auto axisPosition = spectroscope::frequency_axis::normalisedPosition(
				note.frequencyHz, sampleRate_, minimumFrequencyHz_, logarithmic_);
			const auto noteColour = colourForMidiNote(note.midiNote);
			paintVerticalAnnotation(graphics, note, axisPosition, noteColour, entry.opacity);
		}
	}

private:
	static Colour colourForMidiNote(int midiNote)
	{
		const auto pitchClass = ((midiNote % 12) + 12) % 12;
		const auto fifthIndex = (pitchClass * 7) % 12;
		return Colour::fromHSV(static_cast<float>(fifthIndex) / 12.0f, 0.72f, 1.0f, 1.0f);
	}

	static String noteName(const spectroscope::TrackedPitch& note)
	{
		return MidiMessage::getMidiNoteName(note.midiNote, true, true, 4);
	}

	static String centsText(const spectroscope::TrackedPitch& note)
	{
		const auto cents = std::abs(note.cents) < 0.05f ? 0.0f : note.cents;
		return String(cents >= 0.0f ? "+" : "") + String(cents, 1) + " ct";
	}

	static String confidenceText(const spectroscope::TrackedPitch& note)
	{
		return String(5 * roundToInt(note.confidence * 20.0f)) + "%";
	}

	static void drawCentredText(Graphics& graphics, const String& text,
		Rectangle<float> bounds, Colour colour, float opacity)
	{
		graphics.setColour(colour.withMultipliedAlpha(opacity));
		graphics.drawFittedText(text, bounds.toNearestInt(), Justification::centred, 1, 0.75f);
	}

	void paintVerticalAnnotation(Graphics& graphics, const spectroscope::TrackedPitch& note,
		float axisPosition, Colour noteColour, float opacity) const
	{
		constexpr float annotationWidth = 48.0f;
		constexpr float annotationHeight = 51.0f;
		const auto anchorX = axisPosition * static_cast<float>(getWidth());
		const auto centreX = jlimit(annotationWidth * 0.5f,
			static_cast<float>(getWidth()) - annotationWidth * 0.5f, anchorX);
		auto background = Rectangle<float>(
			centreX - annotationWidth * 0.5f, 2.0f, annotationWidth, annotationHeight);
		graphics.setColour(Colours::black.withAlpha(0.68f * opacity));
		graphics.fillRoundedRectangle(background, 3.0f);
		graphics.setColour(noteColour.withAlpha(0.8f * opacity));
		graphics.drawLine(anchorX, background.getBottom(), anchorX,
			jmin(background.getBottom() + 12.0f, static_cast<float>(getHeight())), 1.25f);

		graphics.setFont(13.0f);
		drawCentredText(graphics, noteName(note), background.removeFromTop(17.0f),
			noteColour, opacity);
		graphics.setFont(11.0f);
		drawCentredText(graphics, centsText(note), background.removeFromTop(17.0f),
			Colours::white, opacity);
		drawCentredText(graphics, confidenceText(note), background.removeFromTop(17.0f),
			Colours::lightgrey, opacity);
	}

	void paintHorizontalHistory(Graphics& graphics) const
	{
		std::array<spectroscope::TrackedNoteHistory::Entry,
			spectroscope::TrackedNoteHistory::capacity> entries {};
		const auto entryCount = noteHistory_.visibleEntries(
			latestSequence_.load(std::memory_order_relaxed), waterfallRows,
			entries.data(), static_cast<int>(entries.size()));
		for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
			const auto& entry = entries[static_cast<std::size_t>(entryIndex)];
			const auto frequencyPosition = spectroscope::frequency_axis::normalisedPosition(
				entry.note.frequencyHz, sampleRate_, minimumFrequencyHz_, logarithmic_);
			paintHorizontalAnnotation(graphics, entry.note, frequencyPosition,
				entry.historyPosition, colourForMidiNote(entry.note.midiNote), entry.opacity);
		}
	}

	void paintHorizontalAnnotation(Graphics& graphics, const spectroscope::TrackedPitch& note,
		float frequencyPosition, float historyPosition, Colour noteColour, float opacity) const
	{
		constexpr float annotationWidth = 126.0f;
		constexpr float annotationHeight = 19.0f;
		const auto screenPosition = spectroscope::frequency_axis::horizontalScreenPosition(
			frequencyPosition);
		const auto anchorY = screenPosition * static_cast<float>(getHeight());
		const auto anchorX = historyPosition * static_cast<float>(getWidth());
		const auto centreY = jlimit(annotationHeight * 0.5f,
			static_cast<float>(getHeight()) - annotationHeight * 0.5f, anchorY);
		auto background = Rectangle<float>(anchorX - annotationWidth,
			centreY - annotationHeight * 0.5f,
			annotationWidth, annotationHeight);
		graphics.setColour(Colours::black.withAlpha(0.68f * opacity));
		graphics.fillRoundedRectangle(background, 3.0f);
		graphics.setColour(noteColour.withAlpha(0.8f * opacity));
		const auto markerX = jlimit(0.0f, static_cast<float>(getWidth() - 1), anchorX);
		graphics.drawLine(markerX, centreY - annotationHeight * 0.5f,
			markerX, centreY + annotationHeight * 0.5f, 1.25f);

		graphics.setFont(11.0f);
		drawCentredText(graphics, noteName(note), background.removeFromLeft(34.0f),
			noteColour, opacity);
		drawCentredText(graphics, centsText(note), background.removeFromLeft(54.0f),
			Colours::white, opacity);
		drawCentredText(graphics, confidenceText(note), background, Colours::lightgrey, opacity);
	}

	void timerCallback() override
	{
		auto hasVisibleEntries = false;
		if (horizontal_) {
			std::array<spectroscope::TrackedNoteHistory::Entry,
				spectroscope::TrackedNoteHistory::capacity> entries {};
			hasVisibleEntries = noteHistory_.visibleEntries(
				latestSequence_.load(std::memory_order_relaxed), waterfallRows,
				entries.data(), static_cast<int>(entries.size())) > 0;
		} else {
			std::array<spectroscope::TrackedNoteDisplay::Entry,
				spectroscope::TrackedNoteDisplay::capacity> entries {};
			hasVisibleEntries = noteDisplay_.visibleEntries(Time::getMillisecondCounterHiRes(),
				entries.data(), static_cast<int>(entries.size())) > 0;
		}
		if (!hasVisibleEntries)
			stopTimer();
		repaint();
	}

	spectroscope::TrackedNoteDisplay noteDisplay_;
	spectroscope::TrackedNoteHistory noteHistory_;
	std::atomic<std::uint64_t> latestSequence_ { 0 };
	double sampleRate_ { 0.0 };
	double minimumFrequencyHz_ { 1.0 };
	bool logarithmic_ { true };
	bool horizontal_ { false };
};

SpectrogramWidget::SpectrogramWidget(std::weak_ptr<Spectrogram> spectrogram)
	: spectrogram_(std::move(spectrogram))
{
	addAndMakeVisible(statusLabel_);
	statusLabel_.setJustificationType(Justification::topLeft);
	trackedNotesOverlay_ = std::make_unique<TrackedNotesOverlay>();
	addChildComponent(*trackedNotesOverlay_);

	if (const auto analyzer = spectrogram_.lock()) {
		fftData_.resize(static_cast<size_t>(analyzer->spectrumSize() * waterfallRows), analyzer->floorDb());
		pendingSpectra_.resize(
			static_cast<size_t>(analyzer->spectrumSize() * maximumRowsPerRefresh), analyzer->floorDb());
		pitchClassDataHistory_.resize(
			static_cast<size_t>(analyzer->pitchClassSize() * waterfallRows), 0.0f);
		pendingPitchClasses_.resize(
			static_cast<size_t>(analyzer->pitchClassSize() * maximumRowsPerRefresh), 0.0f);
	} else {
		statusLabel_.setText("Spectrum analyzer unavailable", dontSendNotification);
	}
}

SpectrogramWidget::~SpectrogramWidget()
{
	// JUCE invokes openGLContextClosing() from detach(), so this must happen
	// before destruction falls through to ShaderBasedComponent.
	shutdownOpenGL();
}

void SpectrogramWidget::newOpenGLContextCreated()
{
	releaseOpenGLResources();
	openGLReady_ = false;

	const std::string vertexShader(
		reinterpret_cast<const char*>(oscilloscope_vert_glsl), oscilloscope_vert_glsl_size);
	const std::string fragmentShader(
		reinterpret_cast<const char*>(oscilloscope_frag_glsl), oscilloscope_frag_glsl_size);

	if (!context_.setSwapInterval(1)) {
		DBG("The OpenGL driver did not accept the requested swap interval");
	}

	shader_ = std::make_unique<OpenGLShaderProgram>(context_);
	if (!shader_->addVertexShader(vertexShader)
		|| !shader_->addFragmentShader(fragmentShader)
		|| !shader_->link()) {
		publishStatus("Spectrogram shader error: " + shader_->getLastError());
		return;
	}

	shader_->use();
	position_ = std::make_unique<OpenGLShaderProgram::Attribute>(*shader_, "position");
	resolution_ = createUniform(context_, *shader_, "resolution");
	waterfallStartUniform_ = createUniform(context_, *shader_, "waterfallStartPosition");
	waterfallSpanUniform_ = createUniform(context_, *shader_, "waterfallHistorySpan");
	uUpperHalfPercentage_ = createUniform(context_, *shader_, "upperHalfPercentage");
	audioSampleData_ = createUniform(context_, *shader_, "audioSampleData");
	waterfallTexture_ = createUniform(context_, *shader_, "waterfall");
	pitchClassDataUniform_ = createUniform(context_, *shader_, "pitchClassData");
	pitchClassHistoryUniform_ = createUniform(context_, *shader_, "pitchClassHistory");
	lutTexture_ = createUniform(context_, *shader_, "lutTexture");
	logXAxis_ = createUniform(context_, *shader_, "xAxisLog");
	uHorizontal_ = createUniform(context_, *shader_, "horizontalMode");
	uPitchColourMode_ = createUniform(context_, *shader_, "pitchColourMode");
	uSampleRate_ = createUniform(context_, *shader_, "sampleRate");
	uConcertAHz_ = createUniform(context_, *shader_, "concertAHz");
	uMinimumFrequencyHz_ = createUniform(context_, *shader_, "minimumFrequencyHz");
	uSpectrumTexelWidth_ = createUniform(context_, *shader_, "spectrumTexelWidth");

	const auto analyzer = spectrogram_.lock();
	const auto invalidAttribute = position_ == nullptr
		|| position_->attributeID == static_cast<GLuint>(-1);
	const auto missingUniform = resolution_ == nullptr || waterfallStartUniform_ == nullptr
		|| waterfallSpanUniform_ == nullptr
		|| uUpperHalfPercentage_ == nullptr || audioSampleData_ == nullptr
		|| waterfallTexture_ == nullptr || pitchClassDataUniform_ == nullptr
		|| pitchClassHistoryUniform_ == nullptr || lutTexture_ == nullptr
		|| logXAxis_ == nullptr || uHorizontal_ == nullptr
		|| uPitchColourMode_ == nullptr || uSampleRate_ == nullptr || uConcertAHz_ == nullptr
		|| uMinimumFrequencyHz_ == nullptr || uSpectrumTexelWidth_ == nullptr;
	if (analyzer == nullptr || invalidAttribute || missingUniform) {
		publishStatus(analyzer == nullptr ? "Spectrum analyzer unavailable"
			: "Spectrogram shader interface is incomplete");
		return;
	}

	textureLUT_ = createColorLookupTexture();
	spectrumData_ = createDataTexture(analyzer->spectrumSize(), 1, analyzer->floorDb());
	spectrumHistory_ = createDataTexture(analyzer->spectrumSize(), waterfallRows, analyzer->floorDb());
	pitchClassData_ = createDataTexture(analyzer->pitchClassSize(), 1, 0.0f);
	pitchClassHistory_ = createDataTexture(analyzer->pitchClassSize(), waterfallRows, 0.0f);

	context_.extensions.glGenBuffers(1, &vertexBuffer_);
	context_.extensions.glGenBuffers(1, &elements_);
	openGLReady_ = textureLUT_ != nullptr && spectrumData_ != nullptr && spectrumHistory_ != nullptr
		&& pitchClassData_ != nullptr && pitchClassHistory_ != nullptr
		&& vertexBuffer_ != 0 && elements_ != 0;

	if (openGLReady_.load(std::memory_order_acquire))
		publishStatus("GLSL: v" + String(OpenGLShaderProgram::getLanguageVersion(), 2));
	else
		publishStatus("Unable to initialize spectrogram OpenGL resources");
	refreshRequested_.store(true, std::memory_order_release);
}

void SpectrogramWidget::openGLContextClosing()
{
	releaseOpenGLResources();
}

std::shared_ptr<OpenGLTexture> SpectrogramWidget::createColorLookupTexture()
{
	auto texture = std::make_shared<OpenGLTexture>();
	PixelARGB pixels[256] {};
	pixels[0] = PixelARGB(255, 0, 0, 0);
	pixels[255] = PixelARGB(255, 255, 255, 0);
	for (int i = 1; i < 64; ++i)
		pixels[255 - i] = PixelARGB(255, 255, static_cast<uint8>(255 - i * 4), 0);
	for (int i = 0; i < 192; ++i)
		pixels[191 - i] = PixelARGB(255, static_cast<uint8>(128 - i * 2 / 3), 0, static_cast<uint8>(i * 2 / 3));

	texture->bind();
	texture->loadARGB(pixels, 256, 1);
	texture->unbind();
	return texture;
}

std::shared_ptr<OpenGLFloatTexture> SpectrogramWidget::createDataTexture(
	int width, int height, float initialValue)
{
	auto texture = std::make_shared<OpenGLFloatTexture>();
	std::vector<GLfloat> emptyPixels(static_cast<size_t>(width * height), initialValue);
	texture->create(width, height, emptyPixels.data());
	return texture;
}

void SpectrogramWidget::renderOpenGL()
{
	jassert(OpenGLHelpers::isContextActive());

	const auto renderingScale = static_cast<float>(context_.getRenderingScale());
	glViewport(0, 0, roundToInt(renderingScale * static_cast<float>(getWidth())),
		roundToInt(renderingScale * static_cast<float>(getHeight())));
	OpenGLHelpers::clear(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

	if (!openGLReady_.load(std::memory_order_acquire) || shader_ == nullptr || position_ == nullptr)
		return;

	int spectraUpdated = 0;
	const auto refreshWasRequested = refreshRequested_.exchange(false, std::memory_order_acq_rel);
	if (isRunning() || refreshWasRequested)
		spectraUpdated = pullAvailableFrames();

	shader_->use();
	resolution_->set(renderingScale * static_cast<float>(getWidth()), renderingScale * static_cast<float>(getHeight()));
	setUniform(lutTexture_, 0);
	setUniform(logXAxis_, xLogAxis_.load(std::memory_order_relaxed) ? 1 : 0);
	setUniform(uHorizontal_, horizontal_.load(std::memory_order_relaxed) ? 1 : 0);
	setUniform(uPitchColourMode_, pitchColourMode_.load(std::memory_order_relaxed) ? 1 : 0);
	setUniform(waterfallStartUniform_,
		spectroscope::waterfall::oldestRowCentre(waterfallPosition_, waterfallRows));
	setUniform(waterfallSpanUniform_, spectroscope::waterfall::historySpan(waterfallRows));
	setUniform(uUpperHalfPercentage_, upperHalfPercentage_);
	setUniform(audioSampleData_, 1);
	setUniform(waterfallTexture_, 2);
	setUniform(pitchClassDataUniform_, 3);
	setUniform(pitchClassHistoryUniform_, 4);
	setUniform(uConcertAHz_, concertAHz_.load(std::memory_order_relaxed));
	if (const auto analyzer = spectrogram_.lock()) {
		setUniform(uSampleRate_, static_cast<float>(analyzer->sampleRate()));
		setUniform(uMinimumFrequencyHz_, static_cast<float>(
			analyzer->sampleRate() / static_cast<double>(analyzer->fftSize())));
		setUniform(uSpectrumTexelWidth_, 1.0f / static_cast<float>(analyzer->spectrumSize()));
	} else {
		setUniform(uSampleRate_, 0.0f);
		setUniform(uMinimumFrequencyHz_, 1.0f);
		setUniform(uSpectrumTexelWidth_, 1.0f);
	}

	if (const auto analyzer = spectrogram_.lock()) {
		if (spectraUpdated > 0) {
			const auto latestRowOffset = static_cast<size_t>(waterfallPosition_ * analyzer->spectrumSize());
			const auto latestPitchRowOffset = static_cast<size_t>(
				waterfallPosition_ * analyzer->pitchClassSize());
			context_.extensions.glActiveTexture(GL_TEXTURE1);
			spectrumData_->load(fftData_.data() + latestRowOffset, analyzer->spectrumSize(), 1);
			context_.extensions.glActiveTexture(GL_TEXTURE3);
			pitchClassData_->load(
				pitchClassDataHistory_.data() + latestPitchRowOffset, analyzer->pitchClassSize(), 1);

			context_.extensions.glActiveTexture(GL_TEXTURE2);
			for (int pendingRow = 0; pendingRow < spectraUpdated; ++pendingRow) {
				const auto textureRow = pendingTextureRows_[static_cast<size_t>(pendingRow)];
				const auto rowOffset = static_cast<size_t>(textureRow * analyzer->spectrumSize());
				const auto* rowData = fftData_.data() + rowOffset;
				spectrumHistory_->load(rowData, analyzer->spectrumSize(), 1, textureRow);
			}
			context_.extensions.glActiveTexture(GL_TEXTURE4);
			for (int pendingRow = 0; pendingRow < spectraUpdated; ++pendingRow) {
				const auto textureRow = pendingTextureRows_[static_cast<size_t>(pendingRow)];
				const auto rowOffset = static_cast<size_t>(
					textureRow * analyzer->pitchClassSize());
				const auto* rowData = pitchClassDataHistory_.data() + rowOffset;
				pitchClassHistory_->load(rowData, analyzer->pitchClassSize(), 1, textureRow);
			}
		}
	}

	// Texture uploads bind on the currently active unit. Re-establish every
	// sampler binding after uploads so the one-row spectrum can never replace
	// the waterfall texture on unit 2.
	context_.extensions.glActiveTexture(GL_TEXTURE0);
	textureLUT_->bind();
	context_.extensions.glActiveTexture(GL_TEXTURE1);
	spectrumData_->bind();
	context_.extensions.glActiveTexture(GL_TEXTURE2);
	spectrumHistory_->bind();
	context_.extensions.glActiveTexture(GL_TEXTURE3);
	pitchClassData_->bind();
	context_.extensions.glActiveTexture(GL_TEXTURE4);
	pitchClassHistory_->bind();

#if JUCE_DEBUG
	assertTextureBound(context_, GL_TEXTURE0, textureLUT_->getTextureID());
	assertTextureBound(context_, GL_TEXTURE1, spectrumData_->getTextureID());
	assertTextureBound(context_, GL_TEXTURE2, spectrumHistory_->getTextureID());
	assertTextureBound(context_, GL_TEXTURE3, pitchClassData_->getTextureID());
	assertTextureBound(context_, GL_TEXTURE4, pitchClassHistory_->getTextureID());
#endif

	const GLfloat vertices[] = {
		1.0f, 1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f
	};
	const GLuint indices[] = { 0, 1, 3, 1, 2, 3 };

	context_.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
	context_.extensions.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	context_.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elements_);
	context_.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STREAM_DRAW);
	context_.extensions.glVertexAttribPointer(
		position_->attributeID, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
	context_.extensions.glEnableVertexAttribArray(position_->attributeID);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	context_.extensions.glDisableVertexAttribArray(position_->attributeID);
	context_.extensions.glBindBuffer(GL_ARRAY_BUFFER, 0);
	context_.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	context_.extensions.glActiveTexture(GL_TEXTURE0);
	textureLUT_->unbind();
	context_.extensions.glActiveTexture(GL_TEXTURE1);
	spectrumData_->unbind();
	context_.extensions.glActiveTexture(GL_TEXTURE2);
	spectrumHistory_->unbind();
	context_.extensions.glActiveTexture(GL_TEXTURE3);
	pitchClassData_->unbind();
	context_.extensions.glActiveTexture(GL_TEXTURE4);
	pitchClassHistory_->unbind();
}

void SpectrogramWidget::resized()
{
	statusLabel_.setBounds(getLocalBounds().reduced(4).removeFromTop(75));
	trackedNotesOverlay_->setBounds(getLocalBounds());
	context_.triggerRepaint();
}

void SpectrogramWidget::refreshData()
{
	refreshRequested_.store(true, std::memory_order_release);
	context_.triggerRepaint();
}

void SpectrogramWidget::setXAxis(bool logAxis)
{
	xLogAxis_.store(logAxis, std::memory_order_relaxed);
	trackedNotesOverlay_->setAxisMode(logAxis, horizontal_.load(std::memory_order_relaxed));
	context_.triggerRepaint();
}

void SpectrogramWidget::setHorizontalMode(bool horizontal)
{
	horizontal_.store(horizontal, std::memory_order_relaxed);
	trackedNotesOverlay_->setAxisMode(xLogAxis_.load(std::memory_order_relaxed), horizontal);
	context_.triggerRepaint();
}

void SpectrogramWidget::setPitchColourMode(bool enabled)
{
	pitchColourMode_.store(enabled, std::memory_order_relaxed);
	context_.triggerRepaint();
}

void SpectrogramWidget::setTrackedNoteOverlayEnabled(bool enabled)
{
	trackedNoteOverlayEnabled_.store(enabled, std::memory_order_relaxed);
	trackedNotesOverlay_->setVisible(enabled);
	if (!enabled)
		trackedNotesOverlay_->clearNotes();
	refreshData();
}

void SpectrogramWidget::setPitchTrackingPreset(PitchTracker::Preset preset)
{
	if (const auto analyzer = spectrogram_.lock())
		analyzer->setPitchTrackingPreset(preset);
	refreshData();
}

void SpectrogramWidget::setConcertAHz(float frequencyHz)
{
	const auto clampedFrequency = juce::jlimit(400.0f, 480.0f, frequencyHz);
	concertAHz_.store(clampedFrequency, std::memory_order_relaxed);
	if (const auto analyzer = spectrogram_.lock())
		analyzer->setConcertAHz(clampedFrequency);
	context_.triggerRepaint();
}

bool SpectrogramWidget::isOpenGLReady() const noexcept
{
	return openGLReady_.load(std::memory_order_acquire);
}

void SpectrogramWidget::publishStatus(String statusText)
{
	Component::SafePointer<SpectrogramWidget> safeThis(this);
	MessageManager::callAsync([safeThis, statusTextToPublish = std::move(statusText)]() mutable {
		if (safeThis != nullptr)
			safeThis->statusLabel_.setText(std::move(statusTextToPublish), dontSendNotification);
	});
}

void SpectrogramWidget::publishTrackedNotes(
	std::array<spectroscope::TrackedPitch, 6> notes, int noteCount,
	double sampleRate, double minimumFrequencyHz, std::uint64_t sequence)
{
	Component::SafePointer<SpectrogramWidget> safeThis(this);
	MessageManager::callAsync([safeThis, notesToPublish = std::move(notes), noteCount,
		sampleRate, minimumFrequencyHz, sequence]() mutable {
		if (safeThis != nullptr
			&& safeThis->trackedNoteOverlayEnabled_.load(std::memory_order_relaxed)) {
			safeThis->trackedNotesOverlay_->setNotes(
				std::move(notesToPublish), noteCount, sampleRate, minimumFrequencyHz, sequence);
		}
	});
}

void SpectrogramWidget::updateTrackedNoteOverlay(const Spectrogram& analyzer)
{
	if (!trackedNoteOverlayEnabled_.load(std::memory_order_relaxed))
		return;

	const auto nowMs = Time::getMillisecondCounterHiRes();
	if (nowMs < nextTrackedNoteUpdateMs_)
		return;
	nextTrackedNoteUpdateMs_ = nowMs + 100.0;

	constexpr int maximumDisplayedNotes = 6;
	std::array<spectroscope::TrackedPitch, maximumDisplayedNotes> notes {};
	const auto rowOffset = static_cast<size_t>(waterfallPosition_ * analyzer.pitchClassSize());
	const auto noteCount = spectroscope::extractTrackedPitches(
		pitchClassDataHistory_.data() + rowOffset, analyzer.pitchClassSize(),
		concertAHz_.load(std::memory_order_relaxed), notes.data(), maximumDisplayedNotes);

	publishTrackedNotes(std::move(notes), noteCount, analyzer.sampleRate(),
		analyzer.sampleRate() / static_cast<double>(analyzer.fftSize()), lastSequence_);
}

void SpectrogramWidget::releaseOpenGLResources()
{
	openGLReady_ = false;
	if (vertexBuffer_ != 0) {
		context_.extensions.glDeleteBuffers(1, &vertexBuffer_);
		vertexBuffer_ = 0;
	}
	if (elements_ != 0) {
		context_.extensions.glDeleteBuffers(1, &elements_);
		elements_ = 0;
	}

	if (textureLUT_ != nullptr)
		textureLUT_->release();
	if (spectrumData_ != nullptr)
		spectrumData_->release();
	if (spectrumHistory_ != nullptr)
		spectrumHistory_->release();
	if (pitchClassData_ != nullptr)
		pitchClassData_->release();
	if (pitchClassHistory_ != nullptr)
		pitchClassHistory_->release();
	textureLUT_.reset();
	spectrumData_.reset();
	spectrumHistory_.reset();
	pitchClassData_.reset();
	pitchClassHistory_.reset();

	position_.reset();
	resolution_.reset();
	audioSampleData_.reset();
	lutTexture_.reset();
	waterfallTexture_.reset();
	pitchClassDataUniform_.reset();
	pitchClassHistoryUniform_.reset();
	waterfallStartUniform_.reset();
	waterfallSpanUniform_.reset();
	logXAxis_.reset();
	uUpperHalfPercentage_.reset();
	uHorizontal_.reset();
	uPitchColourMode_.reset();
	uSampleRate_.reset();
	uConcertAHz_.reset();
	uMinimumFrequencyHz_.reset();
	uSpectrumTexelWidth_.reset();
	shader_.reset();
}

int SpectrogramWidget::pullAvailableFrames()
{
	const auto analyzer = spectrogram_.lock();
	if (analyzer == nullptr)
		return 0;

	const auto currentSequence = analyzer->sequence();
	if (currentSequence < lastSequence_)
		lastSequence_ = 0;
	if (currentSequence == lastSequence_)
		return 0;

	std::uint64_t copiedSequence = 0;
	const auto copiedRows = analyzer->copyAnalysisFramesAfter(lastSequence_,
		pendingSpectra_.data(), static_cast<int>(pendingSpectra_.size()),
		pendingPitchClasses_.data(), static_cast<int>(pendingPitchClasses_.size()),
		&copiedSequence);
	if (copiedRows <= 0)
		return 0;

	for (int pendingRow = 0; pendingRow < copiedRows; ++pendingRow) {
		waterfallPosition_ = spectroscope::waterfall::nextRow(waterfallPosition_, waterfallRows);
		pendingTextureRows_[static_cast<size_t>(pendingRow)] = waterfallPosition_;
		const auto sourceOffset = static_cast<size_t>(pendingRow * analyzer->spectrumSize());
		const auto destinationOffset = static_cast<size_t>(waterfallPosition_ * analyzer->spectrumSize());
		std::copy_n(pendingSpectra_.data() + sourceOffset, analyzer->spectrumSize(),
			fftData_.data() + destinationOffset);
		const auto sourcePitchOffset = static_cast<size_t>(pendingRow * analyzer->pitchClassSize());
		const auto destinationPitchOffset = static_cast<size_t>(
			waterfallPosition_ * analyzer->pitchClassSize());
		std::copy_n(pendingPitchClasses_.data() + sourcePitchOffset, analyzer->pitchClassSize(),
			pitchClassDataHistory_.data() + destinationPitchOffset);
	}

	lastSequence_ = copiedSequence;
	trackedNotesOverlay_->advanceLatestSequence(copiedSequence);
	updateTrackedNoteOverlay(*analyzer);
	return copiedRows;
}
