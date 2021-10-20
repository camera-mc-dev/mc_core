#version 410

in vec4 fragNormal;
in vec2 fragTexCoord;

uniform vec4 baseColour;

out vec4 fragColour;

void main()
{
	// colour is set from the baseColour, but alpha is modified
	// by the y texture coordinate so that it gets more transparent
	// as we move away. The 1.57 multiplier is just so that alpha
	// gets to 0 at round about -1, and 1.
	float a = cos(1.57 * fragTexCoord.y );
	fragColour = vec4(baseColour.rgb, a);
}
