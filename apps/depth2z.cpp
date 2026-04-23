#include <iostream>
using std::cout;
using std::endl;

#include "imgio/loadsave.h"
#include "calib/calibration.h"


// #define D_IS_DEPTH

int main( int argc, char *argv[] )
{
	if( argc != 4 )
	{
		cout << "Usage: " << argv[0] << " <calib> <dpth image>  <out image>" << endl;
		exit(0);
	}
	
	cout << "reading calib: " << argv[1] << endl;
	Calibration calib; calib.Read( argv[1] );
	
	cout << "reading dpth: " << argv[2] << endl;
	cv::Mat     D = LoadImage( argv[2] );
	
	
	std::string outfn( argv[3] );
	
	
	// more efficient to downscale calib to size of depth, but _easier_ to upscale depth.
	cv::Mat D2;
	if( D.cols != calib.width || D.rows != calib.height )
	{
		cout << "resizing depth from " << D.cols << " x " << D.rows << "  to  " << calib.width << " x " << calib.height << endl;
		cv::resize( D, D2,  cv::Size( calib.width, calib.height ) );
	}
	else
	{
		cout << "depth is the same size as rgb" << endl;
		D2 = D;
	}
	
	cv::Mat D2tmp = D2/5.0f;
	SaveImage( D2tmp, "tmp/D2.floatImg" );
	
	assert( D2.cols == calib.width );
	assert( D2.rows == calib.height );
	
	
	
	// and now it's just back-projection.
	hVec3D cc = calib.GetCameraCentre();
	
	cv::Mat D3( D2.rows, D2.cols, D2.type() );
	for( unsigned y = 0; y < calib.height; ++y )
	{
		for( unsigned x = 0; x < calib.width; ++x )
		{
			float &d2 = D2.at<float>( y, x );
			float &d3 = D3.at<float>( y, x );
			
			hVec2D pi; pi << x,y,1.0f;
			
			hVec3D o; o << 0,0,0,1;
			hVec3D rc  = calib.UnprojectToCamera( pi );
			
			// point if d is _depth_:
			hVec3D p = o + d2 * rc;
			
			// thus, new value:
			d3 = p(2);
			
		}
	}
	
	cv::Mat D3tmp = D3/5.0f;
	SaveImage( D3tmp, "tmp/D3.floatImg" );
	
	SaveImage( D3, outfn );
	
}
