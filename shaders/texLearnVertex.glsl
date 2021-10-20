#version 130

in vec4 vertexCoord;
in vec4 normal;
in vec2 texCoord;
in vec4 vertexColour;

uniform mat4 projMat;
uniform mat4 modelMat;

out vec4 vColour;

void main()
{
	//
	// What we want is a little weird, but not hard to understand.
	// We're rendering to an image T, which is the same shape as a texture map
	// we want to use to render some model - the model that we're projecting through
	// this shader now, as it happens. We want to learn what colours should be in the
	// texture map.
	//
	// We already have an image. We want to learn a texture map such that when the
	// model gets projected onto that image, it would basically not change the image.
	// 
	// If we take the model, and project it *as if* we were projecting it to the final
	// image, then we will know *where* in that image each vertex got projected to.
	// 
	// Knowing where each vertex got projected, we should then be able to look at the
	// image, and see what colour was there. But what about the area of all of the 
	// faces between those vertices?
	//
	// So what we do here is the initial projection *as if* we were projecting 
	// to the image...
	//
	vec4 pos  = projMat * modelMat * vertexCoord;
	
	//
	// pos is now in normalised device coordinates, -1 -> 1 on each axis (x,y,z),
	// The w coordinate is the original depth (z), preserved for homogeneous division.
	//
	// We're actually going to change (x,y) to texture coordinates, but those texture coordinates
	// which are (0->1) on each axis need to become NDC (-1 -> 1 )
	//
	// We preserve the z depth so that depth buffering will work as if it were the more normal
	// projection to the image rather than to the texture. We also preserve the w coordinate so
	// that perspective correct interpolation should still happen.
	//
	gl_Position.x  = (texCoord.x * 2 - 1) * pos.w;
	gl_Position.y  = (texCoord.y * 2 - 1) * pos.w;
	gl_Position.z  = pos.z;
	gl_Position.w  = pos.w;
	
	//
	// We've done the projection through the matrix, but that has not yet done the homogeneous division.
	// We want to do the homogeneous division to take things to normalised device coordinates,
	// where x,y,z are all values in the range -1 -> 1.  Actually, we'll do that division and then
	// renormalise so that everything is between 0->1.
	//
	// Thus, r becomes the normalised x coordinate,
	//       g becomes the normalised y coordinate,
	//       b becomes the normalised z coordinate.
	//       a must be set to 0.0. Why is that?
	//
	
	
	vColour.r = pos.x/pos.w;
	vColour.r = (vColour.r + 1) / 2;
	
	vColour.g = pos.y/pos.w;
	vColour.g = (vColour.g + 1) / 2;
	
// 	vColour.b = pos.z / pos.w;
// 	vColour.b = (vColour.b + 1) / 2;
	vColour.b = pos.w / 10000;
	
	vColour.a = 0.0;
}
