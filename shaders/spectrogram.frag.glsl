/*
   Copyright (c) 2019 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#version 330 core 

uniform vec2  resolution;

uniform float waterfallPosition;
uniform float upperHalfPercentage;
uniform sampler2D audioSampleData;
uniform sampler2D lutTexture; 
uniform sampler2D waterfall; 

float linearXAxis(float x) {
	return x / resolution.x;
}

void main()
{
    float y = gl_FragCoord.y / resolution.y;

	float x;
	x = 1 - linearXAxis(gl_FragCoord.x);

	float amplitude = texture(audioSampleData, vec2(x, 0.0)).r;
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
		gl_FragColor = texture(lutTexture, vec2(value, 0));
	}
};
