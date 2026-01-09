#include <iostream>
using std::cout;
using std::endl;

#include "imgio/loadsave.h"
#include "calib/calibration.h"


// #define D_IS_DEPTH

int main( int argc, char *argv[] )
{
	if( argc != 6 && argc != 9)
	{
		cout << "Usage: " << argv[0] << " <calib> <rgb image> <dpth image> <[z][d]> <out file>" << endl;
		cout << "Usage: " << argv[0] << " <calib> <rgb image> <dpth image> <[z][d]> <out file> [r] [g] [b]" << endl;
		cout << endl;
		cout << "Use the second variant if you want to set all points to a specific colour" << endl;
		cout << "use [z] if you have a z-map, use [d] if you have a depth map." << endl;
		cout << "  > z-map: pixel value is z-coordinate of ray " << endl;
		cout << "  > d-map: pixel value is distance along ray " << endl;
		exit(0);
	}
	bool fixedColour = false;
	
	cout << "reading calib: " << argv[1] << endl;
	Calibration calib; calib.Read( argv[1] );
	
	cout << "reading rgb: " << argv[2] << endl;
	cv::Mat     I = MCDLoadImage( argv[2] );
	
	cout << "reading dpth: " << argv[3] << endl;
	cv::Mat     D = MCDLoadImage( argv[3] );
	
	bool isZmap = false;
	if( *argv[4] == 'z' )
		isZmap = true;
	else if( *argv[4] == 'd' )
		isZmap = false;
	else
		throw std::runtime_error( "must specify [d]epth-map or [z]-map" );
	
	std::string outfn( argv[5] );
	
	Eigen::Vector4f colour;
	if( argc > 6 )
	{
		fixedColour = true;
		colour << atof( argv[6] ), atof( argv[7] ), atof( argv[8] ), 1.0f;
		
		cout << "Setting fixed colour: " << colour.transpose() << endl;
	}
	
	assert( calib.width == I.cols );
	assert( calib.height == I.rows );
	
	
	// more efficient to downscale to size of depth, but _easier_ to upscale depth.
	cv::Mat D2;
	if( D.cols != I.cols || D.rows != I.rows )
	{
		cout << "resizing depth from " << D.cols << " x " << D.rows << "  to  " << I.cols << " x " << I.rows << endl;
		cv::resize( D, D2,  cv::Size( I.cols, I.rows ) );
	}
	else
	{
		cout << "depth is the same size as rgb" << endl;
		D2 = D;
	}
	
	cv::Mat D2tmp = D2/5.0f;
	MCDSaveImage( D2tmp, "tmp/D2.floatImg" );
	
	assert( D2.cols == I.cols );
	assert( D2.rows == I.rows );
	
	
	
	// and now it's just back-projection.
	hVec3D cc = calib.GetCameraCentre();
	std::vector< hVec3D > pts;
	std::vector< Eigen::Vector4f > colours;
	for( unsigned y = 0; y < I.rows; ++y )
	{
		for( unsigned x = 0; x < I.cols; ++x )
		{
			float &d = D2.at<float>( y, x );
			
			if( d > 0.1 && d < 2.5 )
			{
				hVec2D pi; pi << x,y,1.0f;
				
				hVec3D o; o << 0,0,0,1;
				
				// TODO: remove redundant checks
				
				hVec3D  r = calib.Unproject( pi );
				hVec3D p3 = cc + r * d;
				hVec3D p3c = calib.TransformToCamera( p3 );
				hVec3D  rc = p3c - o;
				hVec3D rcn = rc / rc.norm();
				
				
				hVec3D rca  = calib.UnprojectToCamera( pi );
				hVec3D p3ca =  o + rca * d;
				hVec3D p3a = calib.TransformToWorld( p3ca );
				
				
				float t = d/rca(2);
				hVec3D p3cb = o + rca * t;
				hVec3D p3b = calib.TransformToWorld( p3cb );
				
				
				if( isZmap )
					p3 = p3b;
				else
					p3 = p3a;

				pts.push_back( p3 );
				
				if( fixedColour )
				{
					colours.push_back( colour );
				}
				else
				{
					cv::Vec3b &bgr = I.at< cv::Vec3b >( y, x );
					colour << bgr[2], bgr[1], bgr[0], 1.0f;
					colours.push_back( colour );
				}
			}
		}
	}
	
	// testing
	cout << pts.size() << " " << colours.size() << endl;
	{
		colour << 255, 255, 255, 1.0;
		hVec3D p;
		p << -0.0238529,  0.0141937, 0.6188,     1; pts.push_back(p); colours.push_back( colour );
		p << -0.0276796,  0.0881701, -0.0607625, 1; pts.push_back(p); colours.push_back( colour );
		p << -0.0104838,    0.16318,   -0.83729, 1; pts.push_back(p); colours.push_back( colour );
		p <<  0.262982,    -1.34326, -0.615118,  1; pts.push_back(p); colours.push_back( colour );
		
		p << -0.0238529,  0.0141937, 0.6188,     1; pts.push_back(p); colours.push_back( colour );
		p << -0.0276796,  0.0881701, -0.0607625, 1; pts.push_back(p); colours.push_back( colour );
		p << -0.0104838,    0.16318,   -0.83729, 1; pts.push_back(p); colours.push_back( colour );
		p <<  0.262982,    -1.34326, -0.615118,  1; pts.push_back(p); colours.push_back( colour );
		
		p << -0.0238529,  0.0141937, 0.6188,     1; pts.push_back(p); colours.push_back( colour );
		p << -0.0276796,  0.0881701, -0.0607625, 1; pts.push_back(p); colours.push_back( colour );
		p << -0.0104838,    0.16318,   -0.83729, 1; pts.push_back(p); colours.push_back( colour );
		p <<  0.262982,    -1.34326, -0.615118,  1; pts.push_back(p); colours.push_back( colour );
		
		
		
		
		
		
		
	}
	cout << pts.size() << " " << colours.size() << endl;
	
	// put everything in a single big matrix
	genRowMajMatrix M = genRowMajMatrix::Zero( pts.size(), 3 + 3 );
	for( unsigned pc = 0; pc < pts.size(); ++pc )
	{
		M.row(pc) << pts[pc].head(3).transpose(), colours[pc].head(3).transpose();
	}
	
	// write a ply file.
	
	
	std::ofstream outfi( outfn, std::ios::binary );
	outfi << "ply"                             << std::endl;
	outfi << "format binary_little_endian 1.0" << std::endl;
	outfi << "element vertex "                 << M.rows() << std::endl;
	outfi << "property float x"                << std::endl;
	outfi << "property float y"                << std::endl;
	outfi << "property float z"                << std::endl;
	outfi << "property uchar red"              << std::endl;
	outfi << "property uchar green"            << std::endl;
	outfi << "property uchar blue"             << std::endl;
	
	outfi << "end_header" << std::endl;
	
	//outfi.write( (char*) M.data(), sizeof( float ) * M.rows() * M.cols() );
	for( unsigned pc = 0; pc < pts.size(); ++pc )
	{
		outfi.write( (char*) &pts[pc](0), sizeof( float ) );
		outfi.write( (char*) &pts[pc](1), sizeof( float ) );
		outfi.write( (char*) &pts[pc](2), sizeof( float ) );
		
		unsigned char r,g,b;
		r = colours[pc](0);
		g = colours[pc](1);
		b = colours[pc](2);
		outfi.write( (char*) &r, sizeof( unsigned char ) );
		outfi.write( (char*) &g, sizeof( unsigned char ) );
		outfi.write( (char*) &b, sizeof( unsigned char ) );
		
	}
	
	outfi.close();
}
