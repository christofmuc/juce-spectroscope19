#version 150

uniform sampler2D noteAtlas;

in vec2 atlasCoordinate;
out vec4 fragmentColour;

void main()
{
	fragmentColour = texture(noteAtlas, atlasCoordinate);
}
