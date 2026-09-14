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
	float normalisedPosition = clamp(axisPosition, 0.0, 1.0);
	if (xAxisLog == 0)
		return normalisedPosition;

	float nyquist = sampleRate * 0.5;
	if (nyquist <= 0.0)
		return normalisedPosition;

	float minimumFrequency = clamp(minimumFrequencyHz, 0.001, nyquist);
	float frequency = minimumFrequency * pow(nyquist / minimumFrequency, normalisedPosition);
	return frequency / nyquist;
}

vec3 hsvToRgb(vec3 hsv) {
	vec3 rgb = clamp(abs(mod(hsv.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
	return hsv.z * mix(vec3(1.0), rgb, hsv.y);
}

float spectralSalience(sampler2D sourceTexture, vec2 texturePosition) {
	float offset = max(spectrumTexelWidth, 0.000001);
	float centre = texture(sourceTexture, texturePosition).r;
	float lower = texture(sourceTexture, vec2(max(texturePosition.x - offset, 0.0), texturePosition.y)).r;
	float upper = texture(sourceTexture, vec2(min(texturePosition.x + offset, 1.0), texturePosition.y)).r;
	float prominenceDb = centre - max(lower, upper);
	return smoothstep(0.0, 6.0, prominenceDb);
}

float trackedPitchPosition(float frequencyPosition) {
	float frequency = frequencyPosition * sampleRate * 0.5;
	if (frequency <= 0.0 || concertAHz <= 0.0)
		return -1.0;
	float lowestTrackedFrequency = concertAHz / 8.0;
	return log(frequency / lowestTrackedFrequency) / log(2.0) / 6.0;
}

float trackedPitchConfidence(sampler2D sourceTexture, float frequencyPosition, float historyPosition) {
	float trackedPosition = trackedPitchPosition(frequencyPosition);
	if (trackedPosition < 0.0 || trackedPosition >= 1.0)
		return 0.0;
	return texture(sourceTexture, vec2(trackedPosition, historyPosition)).r;
}

vec4 spectrumColour(float decibels, float frequencyPosition, float salience, float pitchConfidence) {
	float linearIntensity = clamp(1.0 + decibels / 100.0, 0.0, 1.0);
	float intensity = pow(linearIntensity, 1.7);
	if (pitchColourMode == 0)
		return texture(lutTexture, vec2(linearIntensity, 0.0));

	float frequency = frequencyPosition * sampleRate * 0.5;
	if (frequency <= 0.0 || concertAHz <= 0.0)
		return vec4(vec3(intensity), 1.0);

	float fractionalMidiNote = 69.0 + 12.0 * log(frequency / concertAHz) / log(2.0);
	float nearestMidiNote = floor(fractionalMidiNote + 0.5);
	float centsFromNote = abs(100.0 * (fractionalMidiNote - nearestMidiNote));

	// Multiplication by seven maps chromatic pitch classes onto circle-of-fifths order:
	// C, G, D, A, E, B, F#, C#, G#, D#, A#, F.
	float pitchClass = mod(nearestMidiNote, 12.0);
	float fifthIndex = mod(pitchClass * 7.0, 12.0);
	float hue = fifthIndex / 12.0;

	// Colour falls continuously from a tempered note centre to neutral grey at
	// the midpoint between notes. The peak position shows whether it is flat or sharp.
	float tuningSaturation = clamp(1.0 - centsFromNote / 50.0, 0.0, 1.0);
	float amplitudeConfidence = smoothstep(0.08, 0.42, linearIntensity);
	// Real instruments spread their energy over harmonics and seldom reach the
	// confidence of a stationary sine. Treat tracking as soft evidence and use a
	// square-root response so medium-confidence notes remain clearly visible.
	float trackedConfidence = smoothstep(0.01, 0.30, pitchConfidence);
	float spectralPeakConfidence = mix(0.55, 1.0, salience);
	float colourEvidence = trackedConfidence * spectralPeakConfidence * amplitudeConfidence;
	float saturation = tuningSaturation * sqrt(colourEvidence);
	return vec4(hsvToRgb(vec3(hue, saturation, intensity)), 1.0);
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

		vec2 spectrumPosition = vec2(x, 0.0);
		float amplitude = texture(audioSampleData, spectrumPosition).r;
		float amplitudeNormalised = clamp(1.0 + amplitude / 100.0, 0.0, 1.0);
		if (y > upperHalfPercentage) {
			// upper half of screen shows curve
			if ((y-upperHalfPercentage)/(1-upperHalfPercentage) < amplitudeNormalised)  {
				float pitchConfidence = trackedPitchConfidence(pitchClassData, x, 0.0);
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
