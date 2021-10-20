#include "renderer2/bbox.h"


Rendering::BBox2D::BBox2D(hVec2D tl, hVec2D br, float thickness) : Mesh( 8, 8 )
{
	Set(tl,br, thickness);
}

Rendering::BBox2D::BBox2D(cv::Rect r, float thickness) : Mesh( 8, 8 )
{
	Set(r, thickness);
}

Rendering::BBox2D::~BBox2D()
{
	
}

void Rendering::BBox2D::Set(cv::Rect r, float thickness)
{
	hVec2D tl, br;
	tl << r.x, r.y, 1.0;
	br << r.x + r.width, r.y + r.height, 1.0;
	Set(tl,br,thickness);
}

void Rendering::BBox2D::Set(hVec2D tl, hVec2D br, float t)
{
	// although a square should only need 4 vertices, we have 8 so
	// that we can control the thickness of the lines.
	vertices << tl(0), br(0), br(0), tl(0),    tl(0)-t, br(0)+t, br(0)+t, tl(0)-t,
	            tl(1), tl(1), br(1), br(1),    tl(1)-t, tl(1)-t, br(1)+t, br(1)+t,
				0.0f,  0.0f,  0.0f,  0.0f,     0.0f,    0.0f,    0.0f,    0.0f,
				1.0f,  1.0f,  1.0f,  1.0f,     1.0f,    1.0f,    1.0f,    1.0f;

	faces <<    4, 5,   5, 6,   6, 7,   7, 4,
	            5, 1,   6, 2,   7, 3,   4, 0,
				0, 0,   1, 1,   2, 2,   3, 3;
}
