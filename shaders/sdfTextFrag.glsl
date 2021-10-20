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

	// output colour is the user supplied colour,
	// but the alpha is set to 0 where there's no text...
	// i.e. where v > 0.5
	fragColor = baseColour;

	if( v > 0.499 )
	{
		fragColor.a = 0.0;
	}

	// do some anti-aliasing.
	fragColor.a *= smoothstep(0.25, 0.75, fragColor.a);

}
