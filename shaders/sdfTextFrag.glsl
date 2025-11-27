#version 410

in vec4 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D tex0;
uniform vec4 baseColour;

out vec4 fragColor;

void main()
{
	// the texture is single channel, so get the R value.
	float v = texture( tex0, fragTexCoord ).r;
	
	// output colour is the user supplied colour...
	fragColor = baseColour;
	
	
	// but alpha goes black outside of text.
	// outside of text means v > 0.5
	// but have a bit of smoothing between inside and outside.
	fragColor.a = smoothstep( 0.489, 0.503, 1.0-v);
	
}
