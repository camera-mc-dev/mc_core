#include "misc/ply.h"
#include "renderer2/i3dRenderer.h"
#include "renderer2/geomTools.h"
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
	genRowMajMatrix cloud = LoadPlyPointNormalRGB( argv[1] );
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
			throw( std::runtime_error( "Not bothering with version that shows images at camera positions right now" ) );
			for( unsigned c = 3; c < argc; ++c )
			{
				cout << "\tsource: " << argv[c] << " " << std::flush;
				auto sp = CreateSource( argv[c] );
				calibs.push_back( sp.source->GetCalibration() );
				cv::Mat i = sp.source->GetCurrent();
				float sc = 320.0f / i.cols;
				cout << i.cols << " " << i.rows << endl;
				cv::Mat i2;
				cv::resize( i, i2, cv::Size(), sc, sc );
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
	
	cout << "imgs: " << imgs.size() << endl;
	cout << "calibs: " << calibs.size() << endl;
	
	
	
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
	cout << winW << " " << winH << endl;
	
	
	
	
	
	//
	// Create a "standard" interactive 3D renderer
	//
	std::shared_ptr<Rendering::I3DRenderer> ren;
	Rendering::RendererFactory::Create( ren, winW,winH, "point cloud viewer" );
	
	
	
	
	
	//
	// The easiest way of rendering all our points is to just make a small cube for each one.
	// This is unlikely to be the most efficient way of doing things!
	// We'll assume that the point cloud & calib files are scaled in metres, and just set each
	// marker to be about 5mm.
	//
	cout << "making renderables..." << endl;
	
	//
	// Create mesh with vertices and vertexColours
	//
	std::shared_ptr<Rendering::Mesh> cloudMesh( new Rendering::Mesh( cloud.rows(),0 ) ); // lots of verts, no faces.
	if( cloud.cols() == 6 ) // x,y,z,  r,g,b
	{
		cout << "got xyz, rgb data" << endl;
		cloudMesh->vertices.block(   0,0, 3, cloud.rows()) = cloud.block(0,0, cloud.rows(), 3 ).transpose();
		cloudMesh->vertColours.block(0,0, 3, cloud.rows()) = cloud.block(0,3, cloud.rows(), 3 ).transpose();
		//cloudMesh->vertColours.block(3,0, 1, cloud.rows()) = genMatrix::Ones( 1, cloud.rows() );
		for( unsigned c = 0; c < cloud.rows(); ++c )
			cloudMesh->vertColours(3,c) = 1.0f;
	}
	else if( cloud.cols() == 10 ) // x,y,z,   nx,ny,nz,   r,g,b,a
	{
		cout << "got xyz, nxnynx, rgba data " << endl;
		cloudMesh->vertices.block(   0,0, 3, cloud.rows()) = cloud.block(0,0, cloud.rows(), 3 ).transpose();
		cloudMesh->vertColours.block(0,0, 4, cloud.rows()) = cloud.block(0,6, cloud.rows(), 4 ).transpose();
	}
	else
	{
		cout << "unexpected column count: " << cloud.rows() << " " << cloud.cols() << endl;
	}
	cout << "first ten points:" << endl;
	cout << cloudMesh->vertices.block( 0,0, 4, 10 ) << endl;
	cout << "first ten colours: " << endl;
	cout << cloudMesh->vertColours.block(0,0, 4, 10) << endl;
// 	exit(0);
	
	//
	// Create mesh node.
	//
	std::shared_ptr< Rendering::MeshNode > mn;
	Rendering::NodeFactory::Create( mn, "cloud mesh" );
	
	mn->SetShader( ren->GetShaderProg("pcloudShader") );
	mn->SetTexture( ren->GetBlankTexture() );
	
	Eigen::Vector4f black; black << 0.0f, 0.0f, 0.0f, 0.0f;
	mn->SetBaseColour(black);
	
	cloudMesh->UploadToRenderer(ren);
	mn->SetMesh( cloudMesh );
	
	ren->Get3dRoot()->AddChild( mn );
	
	
	//
	// Put an axis on the 3D overlay.
	//
	auto axesNode = Rendering::GenerateAxisNode3D( 0.5, "axesNode", ren );
	ren->Get3dOverlayRoot()->AddChild( axesNode );
	
	
	cout << "setting view calib" << endl;
	Calibration viewCalib;
	if( calibs.size() > 0 )
		viewCalib = calibs[0];
	else
	{
		viewCalib.width = winW;
		viewCalib.height = winH;
		viewCalib.K << winW/4.0,    0, winW/2.0,
		                  0, winW/4.0, winH/2.0,
		                  0,        0,        1;
		viewCalib.L = transMatrix3D::Identity();
	}
	ren->Set3DCamera( viewCalib, 0.1, 1000.0 );
	
	
	//
	// Put cameras on the 3D overlay - a squished cube / frustum.
	//
	Eigen::Vector4f magenta;
	magenta << 1.0, 0, 1.0, 1.0;
	
	hVec3D zero; zero << 0,0,0,1.0f;
	auto camCube = Rendering::GenerateWireCube(zero, 20);
	
	
	// we'll actually tweak the vertices slightly to more obviously show
	// which way the camera is facing.
	camCube->vertices <<  -0.02, -0.02,  0.02,  0.02,    -0.05,-0.05, 0.05,  0.05,
	                      -0.02,  0.02,  0.02, -0.02,    -0.05, 0.05, 0.05, -0.05,
	                          0,       0,     0,     0,     0.05, 0.05, 0.05,  0.05,
	                          1,       1,     1,     1,        1,    1,    1,     1;
	
	camCube->UploadToRenderer(ren);
	for( unsigned cc = 0; cc < calibs.size(); ++cc )
	{
		std::shared_ptr< Rendering::MeshNode > cn;
		std::stringstream ss;
		ss << "nodeForCamera_" << cc;
		Rendering::NodeFactory::Create(cn, ss.str() );
		cn->SetMesh( camCube );
		cn->SetBaseColour(magenta);
		cn->SetTexture(ren->GetBlankTexture());
		cn->SetTransformation( calibs[cc].L.inverse() );
		cn->SetShader( ren->GetShaderProg("basicColourShader"));
		
		ren->Get3dOverlayRoot()->AddChild( cn );
	}
	
	
	cout << "starting render loop" << endl;
	
	cout << viewCalib.K << endl;
	cout << viewCalib.L << endl;
	
	//
	// And then there's not much more to it than to loop the render loop.
	// TODO: variation of the renderer with text overlays, jump-to-view, adjust point sizes, etc.
	bool done = false;
	while( !done )
	{
		done = ren->StepEventLoop();
	}
	
}
