#ifndef ME_CALIBRATION
#define ME_CALIBRATION

#include "math/mathTypes.h"

#include <string>
#include <iostream>
#include <fstream>
using std::endl;

class Calibration
{
public:

	Calibration()
	{
		distParams.assign(5,0);
		K = transMatrix2D::Identity();
		L = transMatrix3D::Identity();
	}

	transMatrix2D K;
	transMatrix3D L;
	std::vector<float> distParams;
	int width, height;

	// wrappers around using L
	hVec3D TransformToCamera(const hVec3D &in) const;
	hVec3D TransformToWorld(const hVec3D &in) const;

	// projection
	hVec2D Project(const hVec3D &in) const;
	bool   Project(const hVec3D &in, hVec2D &p) const;  // project, but return false if point is behind camera
	hVec2D ProjectFromCamera(const hVec3D &in) const;	// project a point already in camera coords.

	hVec3D Unproject(const hVec2D &in) const;
	hVec3D UnprojectToCamera(const hVec2D &in) const;	// unproject, but only to camera coords.
	
	// rescale the translation. Useful if we have calibrated in mm but need metres, for example.
	void Rescale( float scale )
	{
		L.block(0,3, 3, 1) *= scale;
	}
	
	// if we want this calibration to work on a resized version of the image,
	// we can also do some simple rescaling, this time of both the principle point and focal length.
	void RescaleImage( float scale )
	{
		// rescale focal length
		K(0,0) *= scale;
		K(1,1) *= scale;
		
		// rescale principle point.
		K(0,2) *= scale;
		K(1,2) *= scale;
		
		// rescale width/height
		width  *= scale;
		height *= scale;
		
	}

	// read and write the calibration.
	bool Read( std::string filename );
	bool Write( std::string filename );

	// apply distortion params.
	// private because these should not be called externally,
	// they are only valid inside Project and Unproject
private:
	hVec2D Distort(const hVec2D &in) const;
	hVec2D Undistort(const hVec2D &in) const;

public:
	hVec2D DistortPoint( const hVec2D &in ) const;
	hVec2D UndistortPoint( const hVec2D &in ) const;
	
	
	// undistort an image.
	cv::Mat Undistort( const cv::Mat img ) const;
	void Undistort( const cv::Mat img, cv::Mat &res );

	hVec3D GetCameraCentre()
	{
		hVec3D o; o << 0,0,0,1;
		return TransformToWorld(o);
	}

	Calibration ConvertToLeftHanded()
	{
		Calibration left;
		left.K = K;
		left.L = L;
		left.width = width;
		left.height = height;
		left.distParams = distParams;

		left.L(1,3) *= -1;	// flip y-axis direction.
		// left the rotation
		left.L(0,1) *= -1;
		left.L(1,0) *= -1;
		left.L(1,2) *= -1;
		left.L(2,1) *= -1;

		// tweak the principle point.
		left.K(1,2) = height - K(1,2);

		// and the tangential y parameter?
		left.distParams[3] *= -1.0;

		return left;
	}

	Calibration ConvertToRightHanded()
	{
		// I think this is the same process...(?)
		return ConvertToLeftHanded();
	}

	cv::Mat KtoMat() const
	{
		cv::Mat cvK(3,3, CV_32FC1);
		for( unsigned rc = 0; rc < 3; ++rc )
		{
			for( unsigned cc = 0; cc < 3; ++cc )
			{
				cvK.at<float>(rc,cc) = K(rc,cc);
			}
		}
		return cvK;
	}

	cv::Mat PtoMat() const
	{
		cv::Mat cvK(3,4, CV_32FC1);
		for( unsigned rc = 0; rc < 3; ++rc )
		{
			for( unsigned cc = 0; cc < 3; ++cc )
			{
				cvK.at<float>(rc,cc) = K(rc,cc);
			}
		}
		return cvK;
	}

	projMatrix P()
	{
		projMatrix P = projMatrix::Identity();
		P.block(0,0,3,3) = K;
		return P;
	}

	cv::Mat LtoMat()
	{
		cv::Mat cvL(4,4, CV_32FC1);
		for( unsigned rc = 3; rc < 4; ++rc )
		{
			for( unsigned cc = 0; cc < 4; ++cc )
			{
				cvL.at<float>(rc,cc) = L(rc,cc);
			}
		}
		return cvL;
	}

	// although the SBA FAQ very explicitly says the demo program uses
	// a left handed coordinate system with the z-axis pointing into the image,
	// the images it links to very explcitly show a right-handed coordinate system
	// with the z axis pointing into the image, y-up and x to the LEFT.
	// that's a 180-degree rotation around z from my coordinate system.
	// but that doesn't make sense, because none of the demo program input data
	// has negative x principle points. This CAN'T be the problem?
	// Calibration ConvertToSBA()
	// {
	// 	transMatrix3D R = transMatrix3D::Identity();
	// 	Eigen::AngleAxisf zrot(3.14159265359, Eigen::Vector3f::UnitZ());

	// 	R.block(0,0,3,3) = zrot.matrix();

	// 	Calibration sba;
	// 	sba.K = K;
	// 	sba.L = L;
	// 	sba.width = width;
	// 	sba.height = height;
	// 	sba.distParams = distParams;

	// 	sba.L = R * sba.L;

	// 	// tweak the principle point for the new camera orientation.
	// 	// why would anyone use this orientation? -ve values of the x axis? Seriously?
	// 	sba.K(0,2) *= -1.0;
	// 	sba.K(1,2) = height - sba.K(1,2);

	// }

	// Calibration ConvertFromSBA()
	// {
	// 	// exactly the same transformation will do.
	// }
	
	
	
	
	// these methods have been created to be used primarily with
	// the Ceres solver, which needs to use its own data types 
	// instead of floats/doubles etc.
	template<typename T>
	void ProjectT( Eigen::Matrix< T, Eigen::Dynamic, Eigen::Dynamic> &v3D, Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> &v2D )
	{
		v2D = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>::Zero( v3D.cols(), 2 );
		for( unsigned vc = 0; vc < v3D.cols(); ++vc )
		{
			Eigen::Matrix<T, 4, 1> p = v3D.col(vc);
			
			// project to normalised coords.
			Eigen::Matrix<T, 3, 4> P; P << T(1),T(0),T(0),T(0),  T(0),T(1),T(0),T(0),  T(0),T(0),T(1),T(0);

			Eigen::Matrix<T, 4, 4> L2;
			L2 = L.cast<T>();
			
			Eigen::Matrix<T, 3, 1> pn = P * L2 * p;
			pn = pn / pn(2);

			// distortion.
			Eigen::Matrix<T, 3, 1> dc; dc << T(0),T(0),T(1);

			// image point relative to centre of distortion.
			Eigen::Matrix<T, 3, 1> rx = pn - dc;

			// length of rx squared (distortion only using even powers)
			T r2 = rx(0)*rx(0) + rx(1)*rx(1);

			// radial distortion weighting
			T v  = T(1) + T(distParams[0])*r2 + T(distParams[1])*(r2*r2) + T(distParams[4])*(r2*r2*r2);

			// tangential distortion
			Eigen::Matrix<T, 3, 1> dx; dx << T(0),T(0),T(0);
			dx(0) = T(2)*T(distParams[2])*rx(0)*rx(1) + T(distParams[3])*(r2 + T(2)*rx(0)*rx(0));
			dx(1) = T(distParams[2])*(r2 + T(2)*rx(1)*rx(1)) + T(2)*T(distParams[3])*rx(0)*rx(1);

			Eigen::Matrix<T, 3, 1> pd = dc + v*rx + dx;
			
			Eigen::Matrix<T, 3, 1> res;
			res = (K.cast<T>()*pd);
			
			v2D(vc, 0) = res(0);
			v2D(vc, 1) = res(1);
		}
			
	}
	
	
	
	
	

};

#endif
