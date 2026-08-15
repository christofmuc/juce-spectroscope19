/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#version 150

uniform vec2  resolution;
uniform int xAxisLog;
uniform int horizontalMode;
uniform int pitchColourMode;

uniform float waterfallStartPosition;
uniform float waterfallHistorySpan;
uniform float upperHalfPercentage;
uniform float sampleRate;
uniform float concertAHz;
uniform float minimumFrequencyHz;
uniform float spectrumTexelWidth;
uniform sampler2D audioSampleData;
uniform sampler2D lutTexture; 
uniform sampler2D waterfall; 
uniform sampler2D pitchClassData;
uniform sampler2D pitchClassHistory;

out vec4 fragmentColour;

float frequencyPosition(float axisPosition) {
	float normalisedPosition = clamp(axisPosition, 0.0f, 1.0f);
	if (xAxisLog == 0)
		return normalisedPosition;

	float nyquist = sampleRate * 0.5f;
	if (nyquist <= 0.0f)
		return normalisedPosition;

	float minimumFrequency = clamp(minimumFrequencyHz, 0.001f, nyquist);
	float frequency = minimumFrequency * pow(nyquist / minimumFrequency, normalisedPosition);
	return frequency / nyquist;
}

vec3 hsvToRgb(vec3 hsv) {
	vec3 rgb = clamp(abs(mod(hsv.x * 6.0f + vec3(0.0f, 4.0f, 2.0f), 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
	return hsv.z * mix(vec3(1.0f), rgb, hsv.y);
}

float spectralSalience(sampler2D sourceTexture, vec2 texturePosition) {
	float offset = max(spectrumTexelWidth, 0.000001f);
	float centre = texture(sourceTexture, texturePosition).r;
	float lower = texture(sourceTexture, vec2(max(texturePosition.x - offset, 0.0f), texturePosition.y)).r;
	float upper = texture(sourceTexture, vec2(min(texturePosition.x + offset, 1.0f), texturePosition.y)).r;
	float prominenceDb = centre - max(lower, upper);
	return smoothstep(0.0f, 6.0f, prominenceDb);
}

float pitchClassPosition(float frequencyPosition) {
	float frequency = frequencyPosition * sampleRate * 0.5f;
	if (frequency <= 0.0f || concertAHz <= 0.0f)
		return 0.0f;
	return fract(log(frequency / concertAHz) / log(2.0f));
}

float trackedPitchConfidence(sampler2D sourceTexture, float frequencyPosition, float historyPosition) {
	return texture(sourceTexture, vec2(pitchClassPosition(frequencyPosition), historyPosition)).r;
}

vec4 spectrumColour(float decibels, float frequencyPosition, float salience, float pitchConfidence) {
	float linearIntensity = clamp(1.0f + decibels / 100.0f, 0.0f, 1.0f);
	float intensity = pow(linearIntensity, 1.7f);
	if (pitchColourMode == 0)
		return texture(lutTexture, vec2(linearIntensity, 0.0f));

	float frequency = frequencyPosition * sampleRate * 0.5f;
	if (frequency <= 0.0f || concertAHz <= 0.0f)
		return vec4(vec3(intensity), 1.0f);

	float fractionalMidiNote = 69.0f + 12.0f * log(frequency / concertAHz) / log(2.0f);
	float nearestMidiNote = floor(fractionalMidiNote + 0.5f);
	float centsFromNote = abs(100.0f * (fractionalMidiNote - nearestMidiNote));

	// Multiplication by seven maps chromatic pitch classes onto circle-of-fifths order:
	// C, G, D, A, E, B, F#, C#, G#, D#, A#, F.
	float pitchClass = mod(nearestMidiNote, 12.0f);
	float fifthIndex = mod(pitchClass * 7.0f, 12.0f);
	float hue = fifthIndex / 12.0f;

	// Colour falls continuously from a tempered note centre to neutral grey at
	// the midpoint between notes. The peak position shows whether it is flat or sharp.
	float tuningSaturation = clamp(1.0f - centsFromNote / 50.0f, 0.0f, 1.0f);
	float amplitudeConfidence = smoothstep(0.08f, 0.42f, linearIntensity);
	// Real instruments spread their energy over harmonics and seldom reach the
	// confidence of a stationary sine. Treat tracking as soft evidence and use a
	// square-root response so medium-confidence notes remain clearly visible.
	float trackedConfidence = smoothstep(0.01f, 0.30f, pitchConfidence);
	float spectralPeakConfidence = mix(0.55f, 1.0f, salience);
	float colourEvidence = trackedConfidence * spectralPeakConfidence * amplitudeConfidence;
	float saturation = tuningSaturation * sqrt(colourEvidence);
	return vec4(hsvToRgb(vec3(hue, saturation, intensity)), 1.0f);
}

void main()
{
	float y = gl_FragCoord.y / resolution.y;

	if (horizontalMode == 1) {
		// Horizontal Mode
		float x = gl_FragCoord.x / resolution.x;
		float frequency = frequencyPosition(y);
		float historyPosition = waterfallStartPosition + x * waterfallHistorySpan;
		vec2 texturePosition = vec2(frequency, historyPosition);
		float value = texture(waterfall, texturePosition).r;
		float pitchConfidence = trackedPitchConfidence(
			pitchClassHistory, frequency, historyPosition);
		fragmentColour = spectrumColour(
			value, frequency, spectralSalience(waterfall, texturePosition), pitchConfidence);
	} else {
		// Vertical Mode
		float x = frequencyPosition(gl_FragCoord.x / resolution.x);

		vec2 spectrumPosition = vec2(x, 0.0f);
		float amplitude = texture(audioSampleData, spectrumPosition).r;
		float amplitudeNormalised = clamp(1.0f + amplitude / 100.0f, 0.0f, 1.0f);
		if (y > upperHalfPercentage) {
			// upper half of screen shows curve
			if ((y-upperHalfPercentage)/(1-upperHalfPercentage) < amplitudeNormalised)  {
				float pitchConfidence = trackedPitchConfidence(pitchClassData, x, 0.0f);
				fragmentColour = spectrumColour(
					amplitude, x, spectralSalience(audioSampleData, spectrumPosition), pitchConfidence);
			}
			else {
				fragmentColour = vec4 (0.0, 0.0, 0.0, 1.0);
			}
		} else {
			// lower half shows history
			float historyProgress = y / upperHalfPercentage;
			float historyPosition = waterfallStartPosition + historyProgress * waterfallHistorySpan;
			vec2 texturePosition = vec2(x, historyPosition);
			float value = texture(waterfall, texturePosition).r;
			float pitchConfidence = trackedPitchConfidence(
				pitchClassHistory, x, historyPosition);
			fragmentColour = spectrumColour(
				value, x, spectralSalience(waterfall, texturePosition), pitchConfidence);
		}
	}
}
