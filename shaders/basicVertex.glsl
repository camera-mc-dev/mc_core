#version 410 core

in vec4 vertexCoord;
in vec4 normal;
in vec2 texCoord;
in vec4 vertexColour;

uniform mat4 projMat;
uniform mat4 modelMat;

out vec4 fragNormal;
out vec2 fragTexCoord;
out vec4 vColour;

void main()
{
	gl_Position  = projMat * modelMat * vertexCoord;
	fragNormal   = modelMat * normal;
	fragTexCoord = texCoord;
	vColour      = vertexColour;
}
