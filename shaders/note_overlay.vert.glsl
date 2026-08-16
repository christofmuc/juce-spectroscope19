#version 150

in vec2 position;
in vec2 textureCoordinate;

out vec2 atlasCoordinate;

void main()
{
	atlasCoordinate = textureCoordinate;
	gl_Position = vec4(position, 0.0, 1.0);
}
