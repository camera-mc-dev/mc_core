#ifndef ME_MATHTYPES
#define ME_MATHTYPES

#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

// matrix types for various purposes:

// general purpose dynamic-sized matrix:
typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic > genMatrix;
typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor > genRowMajMatrix;
typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor > genColMajMatrix;

// projection matrix. Always 3x4.
typedef Eigen::Matrix<float,3,4> projMatrix;

// 3D and 2D homogeneous transformation matrices
typedef Eigen::Matrix<float,4,4> transMatrix3D;
typedef Eigen::Matrix<float,3,3> transMatrix2D;

// OpenGL compatible 4x4 matrix, for projection etc.
// I'm sure I remember OpenGL wanting row-major? Hey-ho,
// no worries then.
typedef Eigen::Matrix<float,4,4, Eigen::ColMajor> gl44Matrix;

// matrix type for filters etc.
typedef Eigen::Matrix< std::complex<float>, Eigen::Dynamic, Eigen::Dynamic> cfMatrix;

// vector types, for various purposes

// 3D and 2D homogeneous vectors.
typedef Eigen::Matrix<float,4,1> hVec3D;
typedef Eigen::Matrix<float,3,1> hVec2D;



struct Rect
{
	hVec2D topLeft;
	hVec2D botRight;
};









#endif
