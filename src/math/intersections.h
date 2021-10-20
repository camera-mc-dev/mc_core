#ifndef INTERSECTIONS_H_MC
#define INTERSECTIONS_H_MC

#include <vector>
using std::vector;

#include "math/mathTypes.h"

// Intersect 3D rays given their start points (s) and
// their normalised directions (d)
hVec3D IntersectRays( vector<hVec3D> s, vector<hVec3D> d );

// Intersect 3D ray (originating at p0 with direction dir) with the plane (nx,ny,nz,d)
hVec3D IntersectRayPlane( const hVec3D &p0, const hVec3D &dir, const Eigen::Vector4f &plane);

// Intersect 2D ray with another ray. Note that this will normalise the directions.
float IntersectRays( const hVec2D &s0, hVec2D &d0, const hVec2D &s1,  hVec2D &d1 );

// check if a point is inside of an oriented bounding box.
bool InsideObb( const cv::RotatedRect &r, const unsigned x, const unsigned y);


#endif
