#ifndef ME_LINES_H
#define ME_LINES_H
#include "math/mathTypes.h"

// distance between a point p, and a line with direction d which goes through lp
float PointLineDistance2D( const hVec2D &p, const hVec2D &d, const hVec2D &lp );

// given a point p, and a line with direction d starting at point lp,
// where the line is lp + a.d, return the a of the closest point on the line to p
float ClosestPositionOnLine2D( const hVec2D &p, const hVec2D &d, const hVec2D &lp );


float DistanceBetweenRays( const hVec3D &c0, const hVec3D &d0, const hVec3D &c1, const hVec3D &d1 );


float PointRayDistance3D( const hVec3D &p, const hVec3D &rayStart, const hVec3D &rayDir );


#endif
