#include "misc/ply.h"
#include "renderer2/geomTools.h"
#include "renderer2/basicRenderer.h"
#include "math/mathTypes.h"
#include "math/matrixGenerators.h"
#include "imgio/sourceFactory.h"
#include "calib/calibration.h"

class PCBGRenderer : public Rendering::BasicRenderer
{
	friend class Rendering::RendererFactory;
	template<class T0, class T1 > friend class RenWrapper;
	
	PCBGRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title) {}
	
	void FinishConstructor()
	{
		BasicRenderer::FinishConstructor();
		std::stringstream ss;
		ss << ccfg.shadersRoot << "/";
		LoadVertexShader(ss.str() + "pcloudVertex.glsl", "pcloudVertex");
		CreateShaderProgram("pcloudVertex", "colourFrag", "pcloudShader");
		
	}
};


int main( int argc, char* argv[] )
{
	if( argc != 4 )
	{
		cout << "draw point cloud over background image" << endl;
		cout << argv[0] << " <image> <calib> <cloud.ply> " << endl;
		exit(0);
	}
	
	std::string imgfn( argv[1] );
	std::string calfn( argv[2] );
	std::string cldfn( argv[3] );
	
	cv::Mat img = LoadImage( imgfn );
	
	Calibration cal;
	cal.Read( calfn );
	
	std::map<std::string, genRowMajMatrix> cloud = LoadPlyPointCloud( cldfn );
	
	
	
	
	std::shared_ptr<PCBGRenderer> ren;
	float winWidth = 1280;
	float rat = (float)img.rows / (float)img.cols;
	float winHeight = winWidth * rat;
	Rendering::RendererFactory::Create(ren,   winWidth, winHeight, "cloud on background" );
	
	ren->Get2dBgCamera()->SetOrthoProjection(0, img.cols, 0, img.rows, -10, 10);
	ren->Get2dFgCamera()->SetOrthoProjection(0, img.cols, 0, img.rows, -10, 10);
	ren->Get3dCamera()->SetFromCalibration(cal, 0.1, 15000);
	
	ren->SetBGImage( img );
	
	cout << img.cols << " " << img.rows << endl;
	cout << cal.width << " " << cal.height << endl;
	
	
	assert( cloud.find("xyz") != cloud.end() );
	int numVert = cloud["xyz"].rows();
	std::shared_ptr<Rendering::Mesh> cloudMesh( new Rendering::Mesh( numVert,0 ) ); // lots of verts, no faces.
	cloudMesh->vertices.block(   0,0, 3, numVert) = cloud["xyz"].block(0,0, numVert, 3 ).transpose();
	
	if( cloud.find("rgb") != cloud.end() )
	{
		cloudMesh->vertColours.block(0,0, 3, numVert) = cloud["rgb"].block(0,0, numVert, 3 ).transpose();
		for( unsigned c = 0; c < numVert; ++c )
			cloudMesh->vertColours(3,c) = 1.0f;
	}
	
	if( cloud.find("norm") != cloud.end() )
	{
		// ignoring normals.
	}
	
	cout << "first ten points:" << endl;
	cout << cloudMesh->vertices.block( 0,0, 4, 10 ) << endl;
	cout << "first ten colours: " << endl;
	cout << cloudMesh->vertColours.block(0,0, 4, 10) << endl;
	
	
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
	
	
	while( !ren->StepEventLoop() )
	{
	}
	
}
