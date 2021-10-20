#version 410

in vec4 fragNormal;
in vec2 fragTexCoord;
in vec4 vColour;

uniform sampler2D tex0;
uniform vec4 baseColour;

out vec4 fragColor;

void main()
{
	// get the texture colour
	vec4 texCol = texture(tex0, fragTexCoord );

	// we can create a simple directional light,
	// whose direction is fixed relative to the camera.
	// (all normals etc are by now in camera coordinates).
	vec4 lightDir = vec4(-0.70, -0.70, 0.1415, 0.0);

	float ambient = 0.1; // always a little bit of lighting

	// dot produce of two vectors gives angle by
	// a.b = |a||b|cos(ang)
	// => cos(ang) = a.b / (|a||b|)
	// but lightDir is normalised, and we assume normals are too, so
	// cos(ang) = a.b
	// cos(0) = 1, cos(90) = 0, we want brightest when angle near 90,
	// so...
	float angular = 0.9 *  max(0, -dot(lightDir, fragNormal) );

	float lighting = 0.8*angular + ambient;
	fragColor = lighting * texCol * (baseColour+vColour);
}
