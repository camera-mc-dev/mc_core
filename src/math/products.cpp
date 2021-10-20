#include "math/products.h"

hVec3D Cross(const hVec3D &a, const hVec3D &b)
{
	// cross product only really interesting
	// for direction vectors, so assume input
	// is direction vector... for now...

	hVec3D res;

	Eigen::Vector3f a3, b3;
	a3 = a.block(0,0,3,1);
	b3 = b.block(0,0,3,1);
	auto p = a3.cross(b3);


	res << p(0), p(1), p(2), 0.0;
	return res;
}
