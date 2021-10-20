#version 410

in vec4 fragNormal;
in vec2 fragTexCoord;
in vec4 vColour;
uniform vec4 baseColour;

out vec4 fragColour;

void main()
{
	// set colour to baseColour
	fragColour = baseColour + vColour;
}
