#include "math/mathTypes.h"
#include "math/matrixGenerators.h"

#include <iostream>
using std::cout;
using std::endl;

transMatrix3D LookAt( hVec3D eye, hVec3D up, hVec3D target )
{
	// rotation vector is created from three rows,
	// each row is an orthogonal vector.
	// these vectors mean the matrix is typically
	// | right |
	// | up    |
	// | fwd   |
	
// 	cout << "e: " << eye.transpose() << endl;
// 	cout << "u: " << up.transpose() << endl;
// 	cout << "t: " << target.transpose() << endl;
	
	
	// we want to face from eye to target...
	Eigen::Vector3f fwd = target.head(3) - eye.head(3);
	fwd = fwd / fwd.norm();
	
// 	cout << "f: " << fwd.transpose() << endl;

	// up we already know, because we've been given it
	Eigen::Vector3f up3 = up.head(3);

	// then right must come from the cross product of
	// up and fwd... this should be the correct way round
	// for our right handed coord system.
	Eigen::Vector3f rgt = fwd.cross(up3);
	rgt = rgt/rgt.norm();
	
// 	cout << "r: " << rgt.transpose() << endl;

	// my example suggests re-computing
	// up after we have rgt... I guess
	// because up may not be orthogonal to fwd.
	up3 = fwd.cross(rgt);
	up3 = up3/up3.norm();
	
// 	cout << "u: " << up3.transpose() << endl;

	transMatrix3D L;
	L = transMatrix3D::Identity();

	L.block(0,0,1,3) = rgt.transpose();
	L.block(1,0,1,3) = up3.transpose();
	L.block(2,0,1,3) = fwd.transpose();

	L(0,3) = -rgt.dot(eye.head(3));
	L(1,3) = -up3.dot(eye.head(3));
	L(2,3) = -fwd.dot(eye.head(3));

	return L;
}


transMatrix3D ProjMatGLFromK( transMatrix2D K, float w, float h, float near, float far )
{
	transMatrix3D M;
	M << 2*K(0,0)/w,   2*K(0,1)/w, -( w - 2*K(0,2))/w      ,                        0,
	              0,  -2*K(1,1)/h,  ( h - 2*K(1,2))/h      ,                        0,
	              0,            0, -(near+far)/(near-far)  ,  2*far*near/(near - far),
	              0,            0,                        1,                        0;
	return M;
}

// useful stuff...
 transMatrix3D ScaleMatrix(float sx, float sy, float sz)
{
	transMatrix3D S = transMatrix3D::Identity();
	S(0,0) = sx;
	S(1,1) = sy;
	S(2,2) = sz;
	return S;
}

transMatrix3D TranslationMatrix(float tx, float ty, float tz)
{
	transMatrix3D T = transMatrix3D::Identity();
	T(0,3) = tx;
	T(1,3) = ty;
	T(2,3) = tz;
	return T;
}

 transMatrix3D TranslationMatrix(hVec3D t)
{
	transMatrix3D T = transMatrix3D::Identity();
	T.col(3) = t;
	T(3,3) = 1.0f; // just in case t was a direction vector.
	return T;
}



 transMatrix3D RotMatrix( Eigen::Vector3f axis, float angle )
{
	Eigen::AngleAxisf aa(angle, axis);
	transMatrix3D R = transMatrix3D::Identity();
	R.block(0,0,3,3) = aa.matrix();
	return R;
}

 transMatrix3D RotMatrix( hVec3D axis, float angle )
{
	Eigen::Vector3f ax3 = axis.head(3);
	ax3.normalize();
	return RotMatrix(ax3, angle);
}

 transMatrix3D RotMatrix( float ax, float ay, float az, float angle )
{
	Eigen::Vector3f axis; axis << ax, ay, az;
	axis.normalize();
	return RotMatrix(axis, angle);
}


transMatrix3D RotMatrix( float ax, float ay, float az )
{
	Eigen::Vector3f axis; axis << ax, ay, az;
	float angle = axis.norm();
	axis /= angle;
	if( fabs(angle) < 0.00001 )
	{
		axis << 0,0,1;
		angle = 0.0f;
	}
	return RotMatrix(axis, angle);
	
	
// 	std::vector<float> axv(3);
// 	axv[0] = ax;
// 	axv[1] = ay;
// 	axv[2] = az;
// 	std::vector<float> R33(9);
// // 	
// 	Eigen::Matrix<float, 4, 4> R = Eigen::Matrix<float,4,4>::Identity();
// 	ceres::AngleAxisToRotationMatrix( &axv[0], &R33[0] );
// 	R << R33[0], R33[3], R33[6], 0,
// 	     R33[1], R33[4], R33[7], 0,
// 	     R33[2], R33[5], R33[8], 0,
// 	          0,      0,      0, 1;
// 
// 	
// 	return R;
	
}

transMatrix3D RotMatrixEuler( float rx, float ry, float rz, EulerOrder order )
{
	Eigen::Vector3f ax,ay,az;
	ax = Eigen::Vector3f::UnitX();
	ay = Eigen::Vector3f::UnitY();
	az = Eigen::Vector3f::UnitZ();
	transMatrix3D X = RotMatrix( ax, rx);
	transMatrix3D Y = RotMatrix( ay, ry);
	transMatrix3D Z = RotMatrix( az, rz);
	transMatrix3D R = transMatrix3D::Identity();
	switch(order)
	{
		case EO_XYZ: R = X*Y*Z; break;
		case EO_XZY: R = X*Z*Y; break;
		case EO_YXZ: R = Y*X*Z; break;
		case EO_YZX: R = Y*Z*X; break;
		case EO_ZXY: R = Z*X*Y; break;
		case EO_ZYX: R = Z*Y*X; break;
	}
	return R;
}

