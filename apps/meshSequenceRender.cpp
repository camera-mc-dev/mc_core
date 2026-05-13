#include "calib/calibration.h"
#include "renderer2/i3dRenderer.h"
#include "renderer2/geomTools.h"
#include "commonConfig/commonConfig.h"
#include "math/matrixGenerators.h"

#include "misc/obj.h"

#include <thread>
#include <mutex>
#include <chrono>

//#define USE_TEXTURE

//
// globals shared between main and thread.
//
std::vector< std::string > objList; 
int listPos;
int lastPos;

std::shared_ptr< Rendering::MeshNode > meshNode;
std::shared_ptr< Rendering::Mesh > objMesh;
cv::Mat objImg;


std::mutex meshmux;

void LoadNextMeshLoop()
{
	
	while( 1 )
	{
		std::unique_lock<std::mutex> lock( meshmux );
		
		int np = lastPos + 1;
		if( np >= objList.size() )
			np = 0;
		
		// load
#ifdef USE_TEXTURE
			LoadOBJ( objList[ np ], objMesh, objImg );
#else
			LoadOBJ( objList[ np ], objMesh );
#endif
		
		
		// increment counter when ready.
		listPos = np;
		
		lock.unlock();
		
		
		// sleep a bit
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}




int main( int argc, char *argv[] )
{
	
	if( argc != 2 )
	{
		cout << "Tool to do simple rendering of a sequence of .obj meshes (e.g. from agisoft)." << endl;
		cout << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <directory of .obj> " << endl;
		return 0;
	}
	
	
	//
	// Figure out the size of window that we can render with.
	//
	CommonConfig ccfg;
	float calRat = 1080.0f / 1920.0f;
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
	Rendering::RendererFactory::Create( ren, winW,winH, "mesh sequence viewer" );
	
	Calibration viewCalib;
	viewCalib.width = winW;
	viewCalib.height = winH;
	viewCalib.K << winW/3.0,        0, winW/2.0,
	                      0, winW/3.0, winH/2.0,
	                      0,        0,        1;
	hVec3D o; o << 0,0,0,1;
	
	
	
	hVec3D eye, up, targ;
	eye  <<  0, -2.0, 1.0, 1.0;
	up   <<  0,    0,   1,   0;
	targ <<  0,    0, 1.0, 1.0;
	
	viewCalib.L = LookAt( eye, up, targ );
	ren->Set3DCamera( viewCalib, 0.1, 1000.0 );
	ren->SetViewCentre( targ );
	
	
	
	
	//
	// Create a scene node we can attach meshes to for rendering
	//
	Rendering::NodeFactory::Create( meshNode, "meshNode" );
	meshNode->SetTexture( ren->GetBlankTexture() );
#ifdef USE_TEXTURE
	meshNode->SetShader( ren->GetShaderProg("basicShader") );
#else
	meshNode->SetShader( ren->GetShaderProg("basicLitColourShader") );
#endif
	
	
	
	
	
	
	
	//
	// Find .obj files in specified directory
	//
	boost::filesystem::path p( argv[1] );
	
	if( boost::filesystem::exists(p) && boost::filesystem::is_directory(p))
	{
		boost::filesystem::directory_iterator di(p), endi;
		for( ; di != endi; ++di )
		{
			std::string s = di->path().string();
			
			if( s.find(".obj") == s.size()-4 )
			{
				objList.push_back(s);
			}
		}
	}
	else
	{
		throw std::runtime_error("Could not find .obj source directory.");
	}
	
	std::sort( objList.begin(), objList.end() );
	
	cout << "found " << objList.size() << " .obj files" << endl;
	
	listPos = 0;
	
	
	// start a thread loading the meshes.
	lastPos = -1;
	std::thread meshThread = std::thread( &LoadNextMeshLoop );
	
	
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	Eigen::Vector4f grey;
	grey << 0.7, 0.7, 0.7, 1.0;
	
	bool done = false;
	while( !done )
	{
		if( listPos != lastPos )
		{
			std::unique_lock<std::mutex> lock( meshmux );
			objMesh->UploadToRenderer( ren );
			
			meshNode->RemoveFromParent();
			meshNode->SetMesh( objMesh );
			
#ifdef USE_TEXTURE
			meshNode->GetTexture()->UploadImage( objImg );
#else
			meshNode->SetBaseColour( grey );
#endif
			
			ren->Get3dRoot()->AddChild( meshNode );
			
			lastPos = listPos;
			lock.unlock();
		}
		
		
		done = ren->StepEventLoop();
		
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		
	}
}
