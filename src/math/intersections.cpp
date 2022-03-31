#include "intersections.h"
#include <iostream>
using std::cout;
using std::endl;

hVec3D IntersectRays( vector<hVec3D> ss, vector<hVec3D> ds )
{
	assert( ss.size() == ds.size() );

	// algorithm uses Slabaugh,Schafer & Livingston 2001:
	// http://www.staff.city.ac.uk/~sbbh653/publications/opray.pdf - dead link.
	// https://www.researchgate.net/publication/2837100_Optimal_Ray_Intersection_For_Computing_3D_Points

	// make sure ds are normalised.
	for( unsigned dc = 0; dc < ds.size(); ++dc )
	{
		float n = ds[dc].norm();
		ds[dc] /= n;

		// cout << n << " " << ds[dc].transpose() << endl;
	}

	Eigen::Matrix3f A;
	Eigen::Vector3f  b;

	A = Eigen::Matrix3f::Zero();
	b = Eigen::Vector3f::Zero();

	for( unsigned c = 0; c < ss.size(); ++c )
	{
		hVec3D &s = ss[c];
		hVec3D &d = ds[c];
		A(0,0) += (1.0f - d(0)*d(0));
		A(0,1) -= d(0)*d(1);
		A(0,2) -= d(0)*d(2);

		A(1,0) -= d(0)*d(1);
		A(1,1) += 1.0 - d(1)*d(1);
		A(1,2) -= d(1)*d(2);

		A(2,0) -= d(0)*d(2);
		A(2,1) -= d(1)*d(2);
		A(2,2) += 1.0 - d(2)*d(2);

		b(0) +=  (1-d(0)*d(0))*s(0) - d(0)*d(1)*s(1) - d(0)*d(2)*s(2);
		b(1) += -d(0)*d(1)*s(0) + (1-d(1)*d(1))*s(1) - d(1)*d(2)*s(2);
		b(2) += -d(0)*d(2)*s(0) - d(1)*d(2)*s(1) + (1-d(2)*d(2))*s(2);
	}

	Eigen::Vector3f ans;
	ans = A.jacobiSvd(Eigen::ComputeFullU | Eigen::ComputeFullV).solve(b);

	hVec3D ret;
	ret << ans(0), ans(1), ans(2), 1.0;
	return ret;
}


float IntersectRays( const hVec2D &s0,  hVec2D &d0, const hVec2D &s1,  hVec2D &d1 )
{
// 	cout << d0.transpose() << " | " << s0.transpose() << "     ---    " << d1.transpose() << " | " << s1.transpose() << endl;
	
	d0 = d0 / d0.norm();
	d1 = d1 / d1.norm();
	
	float top = d1(0) * (s0(1)-s1(1)) + d1(1) * (s1(0)-s0(0));
	float bot = (d1(1) * d0(0)) - (d0(1) * d1(0));
	float t = top/bot;
	
	// check...
// 	top = d0(0) * (s1(1) - s0(1))  +  d0(1) * (s0(0) - s1(0) );
// 	bot = (d1(0) * d0(1))  -  (d1(1) * d0(0));
// 	float s = top/bot;
// 	
// 	cout << "check: " << ((s0 + t * d0) - (s1 + s * d1)).transpose() << endl;
	
	return t;
	
}



hVec3D IntersectRayPlane( const hVec3D &p0, const hVec3D &dir, const Eigen::Vector4f &plane)
{
	float d = plane(3);
	hVec3D n; n.head(3) = plane.head(3); n(3) = 0;
	float t = -(p0.dot(n) + d) / dir.dot(n);

	return p0 + t*dir;
}



bool InsideObb( const cv::RotatedRect &r, const unsigned x, const unsigned y)
{
	// the easiest way of doing this is to...
	// get the point relative to the centre of the box...
	float tx = x - r.center.x;
	float ty = y - r.center.y;
	// rotate the point against the rotation of the bbox...
	float a = (3.14159*-r.angle) / 180.0f;
	float ntx = tx * cos(a) - ty * sin(a);
	float nty = tx * sin(a) + ty * cos(a);
	// then just check as if we were axis-aligned.
	if( ntx > -r.size.width/2.0f && ntx < r.size.width/2.0f && nty > -r.size.height/2.0f && nty < r.size.height/2.0f)
	{
		return true;
	}
	return false;
}
