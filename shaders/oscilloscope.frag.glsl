/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#version 130

uniform vec2  resolution;
uniform int xAxisLog;
uniform int horizontalMode;

uniform float waterfallPosition;
uniform float upperHalfPercentage;
uniform sampler2D audioSampleData;
uniform sampler2D lutTexture; 
uniform sampler2D waterfall; 

float logXAxis(float x) {
	return 1.0f - exp(log(1.0f - x / resolution.x) * 0.2f);
}

float linearXAxis(float x) {
	return x / resolution.x;
}

void main()
{
	float y = gl_FragCoord.y / resolution.y;

	if (horizontalMode == 1) {
		// Horizontal Mode
		float x = linearXAxis(gl_FragCoord.x);
		if (xAxisLog == 1) {
		  y = 	1.0f - exp(log(1.0f - y) * 0.2f);
		}
		float value = texture(waterfall, vec2(y, (x + waterfallPosition))).r;
		value = 1.0 + value / 100.0;
		gl_FragColor = texture(lutTexture, vec2(value, 0));
	} else {
		// Vertical Mode
		float x;
		if (xAxisLog == 1) {
			x = logXAxis(gl_FragCoord.x);
		} else {
			x = linearXAxis(gl_FragCoord.x);
		}

		float amplitude = texture(audioSampleData, vec2(x, 0.0)).r;
		amplitude = 1 + amplitude / 100.0;
		if (y > upperHalfPercentage) {
			// upper half of screen shows curve
			if ((y-upperHalfPercentage)/(1-upperHalfPercentage) < amplitude)  {
				gl_FragColor = texture(lutTexture, vec2(amplitude, 0));
			}
			else {
				gl_FragColor = vec4 (0.0, 0.0, 0.0, 1.0);
			}
		} else {
			// lower half shows history
			//float value = texture(waterfall, vec2(x, waterfallPosition)).r;
			float value = texture(waterfall, vec2(x, (waterfallPosition - (1-y/upperHalfPercentage)))).r;
			value = 1 + value / 100.0;
			gl_FragColor = texture(lutTexture, vec2(value, 0));
		}
	}
}
