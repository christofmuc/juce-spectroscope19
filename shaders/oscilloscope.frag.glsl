/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#version 150

uniform vec2  resolution;
uniform int xAxisLog;
uniform int horizontalMode;
uniform int pitchColourMode;

uniform float waterfallPosition;
uniform float upperHalfPercentage;
uniform float sampleRate;
uniform float concertAHz;
uniform float minimumFrequencyHz;
uniform sampler2D audioSampleData;
uniform sampler2D lutTexture; 
uniform sampler2D waterfall; 

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

vec4 spectrumColour(float decibels, float frequencyPosition) {
	float intensity = clamp(1.0f + decibels / 100.0f, 0.0f, 1.0f);
	if (pitchColourMode == 0)
		return texture(lutTexture, vec2(intensity, 0.0f));

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
	float saturation = clamp(1.0f - centsFromNote / 50.0f, 0.0f, 1.0f);
	return vec4(hsvToRgb(vec3(hue, saturation, intensity)), 1.0f);
}

void main()
{
	float y = gl_FragCoord.y / resolution.y;

	if (horizontalMode == 1) {
		// Horizontal Mode
		float x = gl_FragCoord.x / resolution.x;
		float frequency = frequencyPosition(y);
		float value = texture(waterfall, vec2(frequency, (x + waterfallPosition))).r;
		fragmentColour = spectrumColour(value, frequency);
	} else {
		// Vertical Mode
		float x = frequencyPosition(gl_FragCoord.x / resolution.x);

		float amplitude = texture(audioSampleData, vec2(x, 0.0)).r;
		float amplitudeNormalised = clamp(1.0f + amplitude / 100.0f, 0.0f, 1.0f);
		if (y > upperHalfPercentage) {
			// upper half of screen shows curve
			if ((y-upperHalfPercentage)/(1-upperHalfPercentage) < amplitudeNormalised)  {
				fragmentColour = spectrumColour(amplitude, x);
			}
			else {
				fragmentColour = vec4 (0.0, 0.0, 0.0, 1.0);
			}
		} else {
			// lower half shows history
			//float value = texture(waterfall, vec2(x, waterfallPosition)).r;
			float value = texture(waterfall, vec2(x, (waterfallPosition - (1-y/upperHalfPercentage)))).r;
			fragmentColour = spectrumColour(value, x);
		}
	}
}
