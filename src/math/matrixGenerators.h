#ifndef MC_MATRIX_GENERATORS_H
#define MC_MATRIX_GENERATORS_H

#include <ceres/rotation.h>

#include "math/mathTypes.h"

// This header provides useful matrix generators,
// such as rotation matrices or transformation matrices
// to look from one location to another, or other such things.

transMatrix3D LookAt( hVec3D eye, hVec3D up, hVec3D target );

// OpenGL projection matrix from camera K matrix
transMatrix3D ProjMatGLFromK( transMatrix2D K, float w, float h, float near, float far );



// useful stuff...
transMatrix3D ScaleMatrix(float sx, float sy, float sz);

transMatrix3D TranslationMatrix(float tx, float ty, float tz);

transMatrix3D TranslationMatrix(hVec3D t);



transMatrix3D RotMatrix( Eigen::Vector3f axis, float angle );

transMatrix3D RotMatrix( hVec3D axis, float angle );

transMatrix3D RotMatrix( float ax, float ay, float az, float angle );

// for the case where the angle is encoded as the length of the axis
transMatrix3D RotMatrix( float ax, float ay, float az );

template <typename T>
Eigen::Matrix<T, 4, 4> RotMatrixT(T ax, T ay, T az )
{
	// We need to use the ceres angle axis function if we want to 
	// have ceres differentiate properly - the Eigen one somehow fails.
	// There are probably cleaner ways of doing all this,
	// but the ceres use of pointers for io to functions is horrid.
	// What is wrong with vectors?
	
	std::vector<T> axv(3);
	axv[0] = ax;
	axv[1] = ay;
	axv[2] = az;
	std::vector<T> R33(9);

	Eigen::Matrix<T, 4, 4> R = Eigen::Matrix<T,4,4>::Identity();
	ceres::AngleAxisToRotationMatrix( &axv[0], &R33[0] );
	R << R33[0], R33[3], R33[6], T(0),
	     R33[1], R33[4], R33[7], T(0),
	     R33[2], R33[5], R33[8], T(0),
	       T(0),   T(0),   T(0), T(1);

	
	return R;
}

template <typename T>
Eigen::Matrix<T, 4, 4> TranslationMatrixT(T tx, T ty, T tz )
{
	Eigen::Matrix<T, 4, 4> Tr = Eigen::Matrix<T,4,4>::Identity();
	Tr(0,3) = tx;
	Tr(1,3) = ty;
	Tr(2,3) = tz;
	return Tr;
}

enum EulerOrder
{
	EO_XYZ,
	EO_XZY,
	EO_YXZ,
	EO_YZX,
	EO_ZXY,
	EO_ZYX
};
transMatrix3D RotMatrixEuler( float rx, float ry, float rz, EulerOrder order );

void TransformToEuler( transMatrix3D T, hVec3D &t, float &rx, float &ry, float &rz, EulerOrder order );


// it has become necessary to have the option of using these as well.
template <typename T>
Eigen::Matrix<T, 4, 1> RotToQuatT( Eigen::Matrix<T, 4, 4> &R )
{
	// ceres doesn't directly give us matrix to quaternion, but it does give us
	// matrix->axis and axis->quaternion
	
	// Fill this as column-major.
	T Rc[9] = { R(0,0), R(1,0), R(2,0),  R(0,1), R(1,1), R(1,2),  R(0,2), R(1,2), R(2,2) };
	
	// compute angle-axis
	T aax[3];
	ceres::RotationMatrixToAngleAxis(Rc,aax);
	
	// and then quaternion
	T q[4];
	ceres::AngleAxisToQuaternion(aax, q);
	
	Eigen::Matrix<T, 4, 1> res; res << q[0], q[1], q[2], q[3];
	
	return res;
}

template <typename T>
Eigen::Matrix<T, 4, 1> RotToAngleAxis( Eigen::Matrix<T, 4, 4> &R )
{
	
	// Fill this as column-major.
	T Rc[9] = { R(0,0), R(1,0), R(2,0),  R(0,1), R(1,1), R(2,1),  R(0,2), R(1,2), R(2,2) };
	//T Rc[9] = { R(0,0), R(0,1), R(0,2),  R(1,0), R(1,1), R(1,2),  R(2,0), R(2,1), R(2,2) };
	
	// compute angle-axis
	T aax[3];
	ceres::RotationMatrixToAngleAxis(Rc,aax);
	
	Eigen::Matrix<T, 4, 1> res; res << aax[0], aax[1], aax[2], 0.0;
	
	return res;
}


#endif
