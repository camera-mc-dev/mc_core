#version 410 core

in vec4 vertexCoord;
in vec4 vertexColour;

uniform mat4 projMat;
uniform mat4 modelMat;

uniform float pointSize = 5.0;

out vec4 vColour;

void main()
{
	gl_Position  = projMat * modelMat * vertexCoord;
	gl_PointSize = pointSize;
	vColour      = vertexColour;
}
