#include "math/distances.h"
#include "math/products.h"
float PointLineDistance2D( const hVec2D &p, const hVec2D &d, const hVec2D &lp )
{
	// consider a line a->b, and a point c.
	//
	// find the closest point on ab to c, and then
	// the distance between than point and c.
	float u = ClosestPositionOnLine2D(p, d, lp);
	hVec2D x = lp + u * d;

	return (p-x).norm();
}

float ClosestPositionOnLine2D( const hVec2D &p, const hVec2D &d, const hVec2D &lp )
{
	hVec2D ac = p - lp;
	float top = (ac(0)*d(0)) + (ac(1)*d(1));
	float u = top / d.norm();
	return u;
}


float DistanceBetweenRays( const hVec3D &c0, const hVec3D &d0, const hVec3D &c1, const hVec3D &d1 )
{
	// find a vector perpendicular to both lines
	hVec3D pv = Cross( d0, d1 );
	
	if( pv.norm() > 0 )
	{
		pv = pv / pv.norm();
	
		// and a vector between any two points on the line
		hVec3D r = c1 - c0;
		
		// then project r onto pv....
		float d = abs(r.transpose() * pv);
		return d;
	}
	else
	{
		// the lines are parallel (I think...)
		
		hVec3D r = c1 - c0;
		
		return r.norm();
	}
}

float PointRayDistance3D( const hVec3D &p, const hVec3D &rayStart, const hVec3D &rayDir )
{
	hVec3D rdn = rayDir / rayDir.norm();
	hVec3D a   = p - rayStart;
	
	return Cross( rayDir, a ).norm();
}
