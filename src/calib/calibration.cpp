#include "calib/calibration.h"

#include <iostream>
using std::cout;
using std::endl;

// wrappers around using L
hVec3D Calibration::TransformToCamera(const hVec3D &in) const
{
	return L * in;
}
hVec3D Calibration::TransformToWorld(const hVec3D &in) const
{
	return L.inverse() * in;
}

// projection
hVec2D Calibration::Project(const hVec3D &in) const
{
	// project to normalised coords.
	projMatrix P; P << 1,0,0,0,  0,1,0,0,  0,0,1,0;

	// although for efficiency Eigen would
	// prefer if we did the full projection in one line,
	hVec2D pn = P * L * in;
	pn = pn / pn(2);

	hVec2D pd = Distort(pn);
	return K * pd;
}

// projection
bool Calibration::Project(const hVec3D &in, hVec2D &p) const
{
	projMatrix P; P << 1,0,0,0,  0,1,0,0,  0,0,1,0;

	// transform to camera coords first.
	hVec3D pc = L * in;
	if( pc(2) <= 0 )
	{
		return false;
	}
	
	// then project.
	hVec2D pn = P * pc;
	pn = pn / pn(2);

	hVec2D pd = Distort(pn);
	p = K * pd;
	return true;
}


hVec2D Calibration::ProjectFromCamera(const hVec3D &in) const
{
	// project to normalised coords.
	projMatrix P; P << 1,0,0,0,  0,1,0,0,  0,0,1,0;
	hVec2D pn = P * in;
	pn = pn / pn(2);

	hVec2D pd = Distort(pn);

	return K * pd;
}	

hVec3D Calibration::Unproject(const hVec2D &in) const
{
	hVec2D pd = K.inverse() * in;
	hVec2D pn = Undistort(pd);

	// for this specific projection matrix, inverse is the transpose.
	projMatrix P; P << 1,0,0,0,  0,1,0,0,  0,0,1,0;
	hVec3D rc = P.transpose() * pn;
	hVec3D rw = L.inverse() * rc;

	// nice to make the ray unit length...
	float l = sqrt(rw(0)*rw(0) + rw(1)*rw(1) + rw(2)*rw(2));
	rw = rw / l;
	return rw;
}

cv::Mat Calibration::Undistort( const cv::Mat img ) const
{
	// K as opencv matrix
	cv::Mat cvK = KtoMat();
	
	cv::Mat res = cv::Mat( img.rows, img.cols, img.type(), cv::Scalar(0) );
	if( res.data == img.data )	// how could this possibly happen?
	{
		res.data = new uchar[ img.rows * img.cols * img.channels() ];
	}
	if( res.data == img.data )
	{
		cout << "wtf? Not possible!" << endl;
	}
	cv::undistort(img, res, cvK, distParams );
	return res;
}

void Calibration::Undistort( const cv::Mat img, cv::Mat &res )
{
	// K as opencv matrix
	cv::Mat cvK = KtoMat();
	
	if( res.rows != img.rows || res.cols != img.cols || res.type() != img.type() )
		res = cv::Mat( img.rows, img.cols, img.type(), cv::Scalar(0) );
	assert( res.data != img.data );
	cv::undistort(img, res, cvK, distParams );
}


hVec3D Calibration::UnprojectToCamera(const hVec2D &in) const
{
	hVec2D pd = K.inverse() * in;
	hVec2D pn = Undistort(pd);

	// for this specific projection matrix, inverse is the transpose.
	projMatrix P; P << 1,0,0,0,  0,1,0,0,  0,0,1,0;
	hVec3D rc = P.transpose() * pn;

	// nice to make the ray unit length...
	float l = sqrt(rc(0)*rc(0) + rc(1)*rc(1) + rc(2)*rc(2));
	rc = rc / l;
	return rc;
}

hVec2D Calibration::Distort(const hVec2D &in) const
{
	// centre of distortion assumed to be centre of projection...
	// however, it doesn't need to be in the long run, so that's
	// why it is being exposed as an explicit parameter,
	// even though, it's just... 0...
	hVec2D dc; dc << 0,0,1;

	// image point relative to centre of distortion.
	hVec2D rx = in - dc;

	// length of rx squared (distortion only using even powers)
	float r2 = rx(0)*rx(0) + rx(1)*rx(1);

	// radial distortion weighting
	float v  = 1 + distParams[0]*r2 + distParams[1]*(r2*r2) + distParams[4]*(r2*r2*r2);

	// tangential distortion
	hVec2D dx; dx << 0,0,0;
	dx(0) = 2*distParams[2]*rx(0)*rx(1) + distParams[3]*(r2 + 2*rx(0)*rx(0));
	dx(1) = distParams[2]*(r2 + 2*rx(1)*rx(1)) + 2*distParams[3]*rx(0)*rx(1);

	hVec2D res = dc + v*rx + dx;
	return res;
}

hVec2D Calibration::Undistort(const hVec2D &in) const
{
	// centre of distortion assumed to be centre of projection,
	// but in future it doesn't have to be, hence explicit 
	// variable for it here.
	hVec2D dc; dc << 0,0,1;
	hVec2D rx = in-dc;
	hVec2D guess = rx;
	for(unsigned c = 0; c < 20; ++c )
	{
		// radial distortion weighting
		float r2 = guess(0)*guess(0) + guess(1)*guess(1);
		float v  = 1 + distParams[0]*r2 + distParams[1]*(r2*r2) + distParams[4]*(r2*r2*r2);

		// tangential distortion
		hVec2D dx; dx << 0,0,0;
		dx(0) = 2*distParams[2]*guess(0)*guess(1) + distParams[3]*(r2 + 2*guess(0)*guess(0));
		dx(1) = distParams[2]*(r2 + 2*guess(1)*guess(1)) + 2*distParams[3]*guess(0)*guess(1);
	
		guess = (rx-dx)/v;
	}
	return dc + guess;
}



hVec2D Calibration::DistortPoint(const hVec2D &in) const
{
	hVec2D pd = K.inverse() * in;
	hVec2D pn = Distort(pd);
	return K * pn;
}


hVec2D Calibration::UndistortPoint(const hVec2D &in) const
{
	hVec2D pd = K.inverse() * in;
	hVec2D pn = Undistort(pd);
	return K * pn;	
}



bool Calibration::Read( std::string filename )
{
	std::ifstream infi(filename);
	if(!infi.is_open())
		return false;

	infi >> width;
	infi >> height;

	for(unsigned rc = 0; rc < 3; ++rc)
	{
		for(unsigned cc = 0; cc < 3; ++cc)
		{
			infi >> K(rc,cc);
		}
	}

	for(unsigned rc = 0; rc < 4; ++rc)
	{
		for(unsigned cc = 0; cc < 4; ++cc)
		{
			infi >> L(rc,cc);
		}
	}

	distParams.assign(5, 0.0f);
	for(unsigned kc =0; kc < 5; ++kc )
	{
		infi >> distParams[kc];
	}

	infi.close();

	return true;
}

bool Calibration::Write( std::string filename )
{
	std::ofstream outfi(filename);
	if(!outfi.is_open())
		return false;

	outfi << width << endl;
	outfi << height << endl;
	for(unsigned rc = 0; rc < 3; ++rc)
	{
		for(unsigned cc = 0; cc < 3; ++cc)
		{
			outfi << K(rc,cc) << " ";
		}
		outfi << endl;
	}
	outfi << endl;

	for(unsigned rc = 0; rc < 4; ++rc)
	{
		for(unsigned cc = 0; cc < 4; ++cc)
		{
			outfi << L(rc,cc) << " ";
		}
		outfi << endl;
	}
	outfi << endl;

	for(unsigned kc =0; kc < 5; ++kc )
	{
		outfi << distParams[kc] << " ";
	}
	outfi << endl;

	outfi.close();

	return true;
}
