#version 410 core

// these come in, but as this is just for thick lines, these are not needed. :)
in vec4 fragNormal;
in vec2 fragTexCoord;



void main()
{
	// As I understand it, we'll get the two vertices as
	// gl_in[0] and gl_in[1]
	
	// we want to output 4 vertices a little bit offset from
	// the input. Now, we should be in normalized device coords,
	// which doesn't really affect how we do this.
	
	// first off, what is the normal to the line?
	vec4 d = gl_in[1] - gl_in[0];
	
	vec4 n;
	n.x = -d.y;
	n.y =  d.x;
	n.z =  0.0;
	n.w =  0.0;
	
	vec4 nn = normalize(n);
	
	// we get four vertices
	vec4 a,b,c,d;
	
	a = gl_in[0] + 0.1 * nn;
	b = gl_in[1] + 0.1 * nn;
	c = gl_in[1] - 0.1 * nn;
	d = gl_in[0] - 0.1 * nn;
	
	// and we need to output two triangles...
	gl_Position = a;
	EmitVertex();
	
	gl_Position = b;
	EmitVertex();
	
	gl_Position = c;
	EmitVertex();
	
	EndPrimitive();
	
	
	
	
	gl_Position = a;
	EmitVertex();
	
	gl_Position = c;
	EmitVertex();
	
	gl_Position = d;
	EmitVertex();
	
	EndPrimitive();
}
