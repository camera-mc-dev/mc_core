#include "calib/calibration.h"
#include "math/intersections.h"
#include "math/products.h"

#include <iostream>
using std::cout;
using std::endl;

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>


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
	boost::filesystem::path p(filename);
	boost::filesystem::ifstream infi( p );
	if(!infi.is_open())
	{
		// cout << "?: " << p.string() << endl;
		return false;
	}

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








//
// Given a line between p0 -> p1, clip the line so that it fits inside the 
// camera's view frustum, and return new points p0b, p1b
//
void Calibration::ClipToFrustum( hVec3D p0, hVec3D p1, hVec3D &p0b, hVec3D &p1b )
{
	hVec3D dir = p1 - p0;
	
	// this suits my needs, probably. If either point is behind the camera,
	// don't bother.
	hVec3D tstp0, tstp1;
	tstp0 = TransformToCamera( p0 );
	tstp1 = TransformToCamera( p1 );
	cout << "==========" << endl;
	cout << tstp0.transpose() << endl;
	cout << tstp1.transpose() << endl;
	if( tstp0(2) < 0 || tstp1(2) < 0 )
	{
		p0b << 0,0,0,0;
		p1b << 0,0,0,0;
		return;
	}
	
	
	// get the 4 planes of the frustum... ummm. what about front/back?
	hVec3D rtl, rtr, rbr, rbl;
	hVec2D ptl, ptr, pbr, pbl;
	ptl << 0,0,1.0;
	ptr << width,0,1.0;
	pbr << width,height,1.0;
	pbl << 0,height,1.0;
	rtl = Unproject( ptl );
	rtr = Unproject( ptr );
	rbr = Unproject( pbr );
	rbl = Unproject( pbl );
	
	Eigen::Vector4f pt, pr, pb, pl;
	hVec3D c = GetCameraCentre();
	pt = GetPlane( c, rtl, rtr );
	pr = GetPlane( c, rtr, rbr );
	pb = GetPlane( c, rbr, rbl );
	pl = GetPlane( c, rbl, rtl );
	
	// intersect ray with each plane.
	// there's probably a real risk of the plane and ray
	// being parallel...
	std::vector< float > t(4);
	t[0] = IntersectRayPlane0( p0, dir, pt);
	t[1] = IntersectRayPlane0( p0, dir, pr);
	t[2] = IntersectRayPlane0( p0, dir, pb);
	t[3] = IntersectRayPlane0( p0, dir, pl);
	
	std::sort( t.begin(), t.end() );
	
	// only care about the middle two...
	float f0 = std::max( 0.0f, t[1] );
	float f1 = std::min( 1.0f, t[2] );
	
	// ... but be careful. Maybe we don't intersect the image.
	if( f1 <= f0 )
	{
		p0b << 0,0,0,0;
		p1b << 0,0,0,0;
		return;
	}
	
	cout << p0.transpose() << endl;
	cout << p1.transpose() << endl;	
	cout << t[0] << " : " << ( p0 + t[0] * dir ).transpose() << endl;
	cout << t[1] << " : " << ( p0 + t[1] * dir ).transpose() << endl;
	cout << t[2] << " : " << ( p0 + t[2] * dir ).transpose() << endl;
	cout << t[3] << " : " << ( p0 + t[3] * dir ).transpose() << endl;
	cout << "------" << endl;
		
	p0b = p0 + f0 * dir;
	p1b = p0 + f1 * dir;

}
