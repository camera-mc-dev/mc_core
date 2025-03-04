#include "renderer2/i3dRenderer.h"

Rendering::I3DRenderer::I3DRenderer(unsigned width, unsigned height, std::string title) : BaseRenderer(width,height,title)
{
	InitialiseGraphs();
	
	// load core shaders.
	std::stringstream ss;
	ss << ccfg.shadersRoot << "/";
	
	LoadVertexShader(ss.str() + "basicVertex.glsl", "basicVertex");
	LoadVertexShader(ss.str() + "texLearnVertex.glsl", "texLearnVertex");
	LoadFragmentShader(ss.str() + "basicTextureOnlyFragment.glsl", "unlitFrag" );
	LoadFragmentShader(ss.str() + "basicDirLightAndTextureFragment.glsl", "dirlitFrag" );
	LoadFragmentShader(ss.str() + "basicColourOnlyFragment.glsl", "colourFrag" );
	LoadFragmentShader(ss.str() + "basicLightAndColourFragment.glsl", "dirlitColourFrag" );
// 	LoadGeometryShader(ss.str() + "thickLineGeom.glsl", "thickLineGeom" );
	
	CreateShaderProgram("basicVertex", "unlitFrag", "basicShader");
	CreateShaderProgram("basicVertex", "dirlitFrag", "basicLitShader");
	CreateShaderProgram("basicVertex", "colourFrag", "basicColourShader");
	CreateShaderProgram("basicVertex", "dirlitColourFrag", "basicLitColourShader");
// 	CreateShaderProgram("basicVertex", "thickLineGeom", "colourFrag", "thickLine");
	
	CreateShaderProgram("texLearnVertex", "colourFrag", "texLearnShader");
	
}

Rendering::I3DRenderer::~I3DRenderer()
{

}


void Rendering::BasicRenderer::InitialiseGraphs()
{

	// we'll have four render graphs.
	// backgound 2D, 3D main, 3D overlay, 2D overlay
	scenes.resize(4, SceneGroup(this) );
	
	// set up render graphs.
	//
	// 0 -
	//   - root
	//      - ortho camera
	//      - stuff.
	std::shared_ptr<SceneNode> rt;
	Rendering::NodeFactory::Create(rt, "root_2dBg" );
	scenes[0].SetRoot( rt );
	std::shared_ptr<CameraNode> bg2dcam;  
	Rendering::NodeFactory::Create( bg2dcam, "camera_2dBg" );
	scenes[0].rootNode->AddChild( bg2dcam );
	scenes[0].SetCamera( bg2dcam );
	scenes[0].enableDepth = false;
	
	bg2dcam->SetOrthoProjection(0, GetWidth(), 0, GetHeight(), -100, 100 );
	bg2dcam->SetActive();
	
	transMatrix3D I = transMatrix3D::Identity();
	bg2dcam->SetTransformation(I);
	
	// 1 -
	//   - root
	//      - Perspective camera
	//      - other stuff
	Rendering::NodeFactory::Create(rt, "root_3d" );
	scenes[1].SetRoot( rt );
	std::shared_ptr<CameraNode> cam3dMain; Rendering::NodeFactory::Create(cam3dMain, "camera_3d" );
	scenes[1].rootNode->AddChild( cam3dMain );
	scenes[1].SetCamera( cam3dMain );
	scenes[1].enableDepth = true;
	
	transMatrix2D K;
	K << GetWidth(),             0,  GetWidth() /2,
	              0,    GetWidth(),  GetHeight()/2,
	              0,             0,              1;
	cam3dMain->SetPerspectiveProjection(K, GetWidth(), GetHeight(), 1.0, 10000 );
	cam3dMain->SetActive();
	cam3dMain->SetTransformation(I);
	
	
	
	// 2 -
	//   - root
	//      - Perspective camera
	//      - other stuff
	Rendering::NodeFactory::Create(rt, "over_3d" );
	scenes[2].SetRoot( rt );
	std::shared_ptr<CameraNode> cam3dOver; Rendering::NodeFactory::Create(cam3dOver, "camera_3d" );
	scenes[2].rootNode->AddChild( cam3dOver );
	scenes[2].SetCamera( cam3dOver );
	scenes[2].enableDepth = true;
	
	transMatrix2D K;
	K << GetWidth(),             0,  GetWidth() /2,
	              0,    GetWidth(),  GetHeight()/2,
	              0,             0,              1;
	cam3dOver->SetPerspectiveProjection(K, GetWidth(), GetHeight(), 1.0, 10000 );
	cam3dOver->SetActive();
	cam3dOver->SetTransformation(I);
	
	
	
	// 3 -
	//   - root
	//      - ortho camera
	//      - other stuff.
	Rendering::NodeFactory::Create(rt, "root_2dFg" );
	scenes[3].SetRoot( rt );
	std::shared_ptr<CameraNode> fg2dcam; Rendering::NodeFactory::Create(fg2dcam, "camera_2dFg");
	scenes[3].rootNode->AddChild( fg2dcam );
	scenes[3].SetCamera( fg2dcam );
	scenes[3].enableDepth = false;

	fg2dcam->SetOrthoProjection(0, GetWidth(), 0, GetHeight(), -100, 100 );
	fg2dcam->SetActive();
	fg2dcam->SetTransformation(I);
}

