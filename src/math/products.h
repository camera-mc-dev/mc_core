#ifndef PRODUCTS_H_MC
#define PRODUCTS_H_MC

#include "math/mathTypes.h"

hVec3D Cross(const hVec3D &a, const hVec3D &b);

// given a point and two rays from that point, return the plane formed as (nx,ny,nz,d)
Eigen::Vector4f GetPlane( const hVec3D &p, const hVec3D &r0, const hVec3D &r1 );


#endif
