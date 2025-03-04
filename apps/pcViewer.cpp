#include "misc/ply.h"
#include "renderer2/i3dRenderer.h"
#include "renderer2/geomtools.h"
#include "commonConfig/commonConfig.h"
#include "math/mathTypes.h"
#include "imgio/sourceFactory.h"
#include "calib/calibration.h"


#include <iostream>
#include <vector>
using std::cout;
using std::endl;



int main( int argc, char *argv[] )
{
	
	if( argc < 2 )
	{
		cout << "Tool to do simple rendering of a point cloud." << endl;
		cout << "Can also overlay views from calibrated image sources, or just the camera positions." << endl;
		cout << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <point cloud .ply> [ sources | calibs ] [ source/calib 0 ] [source/calib 1] ... " << endl;
		return 0;
	}
	
	
	//
	// Load point cloud. Try binary format, if that fails, try text.
	//
	genRowMajMatrix cloud = LoadPlyPointRGB( argv[1] );
	if( cloud.rows() == 0 )
	{
		cloud = LoadPlyTxtPointRGB( argv[1] );
		if( cloud.rows() == 0 )
			throw std::runtime_error( "no points loaded" );
	}
	
	
	//
	// Find out if we've got calibs or sources as extra to render with.
	//
	std::vector< cv::Mat > imgs;
	std::vector< Calibration > calibs;
	if( argc > 3 )
	{
		std::string t( argv[2] );
		if( t.compare( "sources" ) == 0 )
		{
			for( unsigned c = 3; c < argc; ++c )
			{
				auto sp = CreateSource( argv[c] );
				calibs.push_back( sp.source->GetCalibration() );
				cv::Mat i = sp.source->GetCurrent();
				float sc = 320 / i.cols();
				cv::Mat i2;
				cv::resize( i, i2, cv::Size(), sc, sc )
				imgs.push_back( i2 );
			}
		}
		else if( t.compare( "calibs" ) == 0 )
		{
			for( unsigned c = 3; c < argc; ++c )
			{
				Calibration cal;
				cal.Read( argv[c] );
				calibs.push_back( cal );
			}
		}
	}
	
	
	
	
	
	//
	// Figure out the size of window that we can render with.
	//
	CommonConfig ccfg;
	float calRat;
	if( calibs.size() > 0 )
		calRat = calibs[0].height / (float)calibs[0].width;
	else
		calRat = 1080.0f / 1920.0f;
	
	float winW = ccfg.maxSingleWindowWidth;
	float winH = winW * calRat;
	if( winH > ccfg.maxSingleWindowHeight )
	{
		winH = ccfg.maxSingleWindowHeight;
		winW = winH / calRat;
	}
	
	
	
	
	
	
	//
	// Create a "standard" interactive 3D renderer
	//
	std::shared_ptr<I3DRenderer> ren;
	Rendering::RendererFactory::Create( ren, winW,winH, "point cloud viewer" );
	
	
	
	
	
	//
	// The easiest way of rendering all our points is to just make a small cube for each one.
	// This is unlikely to be the most efficient way of doing things!
	// We'll assume that the point cloud & calib files are scaled in metres, and just set each
	// marker to be about 5mm.
	//
	for( unsigned c = 0; c < cloud.rows(); ++c )
	{
		hVec3D x; x << cloud.row(c).head(3), 1.0f;
		std::stringstream ss; ss << "cn" << std::setw(12) << std::setfill('0') << c;
		std::shared_ptr<Rendering::MeshNode> n = GenerateCubeNode( x, 5/1000.0f, ss.str(), ren );
		
		Eigen::Vector4f b; b << cloud(c,3), cloud(c,4), cloud(c,5), 1.0;
		n->SetBaseColour( b );
		
		// texture?
		
		ren->Get3dRoot()->AddChild( n );
	}
	
	Calibration viewCalib;
	if( calibs.size() > 0 )
		viewCalib = calibs[0];
	else
	{
		viewCalib.K << imgW,    0, imgW/2.0,
		            0, imgW, imgW/2.0,
		            0,    0,        1;
		viewCalib.L = transMatrix3D::Identity();
	}
	ren->Set3DCamera( viewCalib );
	
	
	
	//
	// And then there's not much more to it than to loop the render loop.
	// TODO: variation of the renderer with text overlays, jump-to-view, adjust point sizes, etc.
	bool done = false;
	while( !done )
	{
		ren->StepEventLoop();
	}
	
}
