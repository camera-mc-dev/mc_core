#version 410

in vec4 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D tex0;
uniform vec4 baseColour;

out vec4 fragColour;

void main()
{
	// get and set fragment colour from the texture, modified by the baseColour
	vec4 texColour = texture(tex0, fragTexCoord );
	fragColour = baseColour * texColour;
}
