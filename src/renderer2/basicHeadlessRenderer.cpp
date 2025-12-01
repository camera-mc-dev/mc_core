#include "renderer2/basicHeadlessRenderer.h"
#include "renderer2/sdfText.h"
#include "renderer2/geomTools.h"

Rendering::BasicHeadlessRenderer::BasicHeadlessRenderer(unsigned width, unsigned height, std::string title) : BaseHeadlessRenderer(width,height,title)
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

	bgImgTex  = nullptr;
	bgImgCard = nullptr;
	bgImgNode = nullptr;
	
	
}


Rendering::BasicHeadlessRenderer::~BasicHeadlessRenderer()
{
	DeleteBGImage();
}


void Rendering::BasicHeadlessRenderer::InitialiseGraphs()
{
	// Load basic vertex and pixel shaders for
	// the 2D and 3D scenes.


	// we'll have three render graphs.
	// backgound 2D, 3D, and foreground 2D (overlay)
	scenes.resize(3, SceneGroup(this) );

	// set up render graphs.
	//
	// 0 -
	//   - root
	//      - ortho camera
	//      - stuff.
	std::shared_ptr<SceneNode> rt;
	Rendering::NodeFactory::Create(rt, "root_2dBg" );
	scenes[0].SetRoot( rt );
	std::shared_ptr<CameraNode> bg2dcam;  Rendering::NodeFactory::Create( bg2dcam, "camera_2dBg" );
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
	std::shared_ptr<CameraNode> cam3d; Rendering::NodeFactory::Create(cam3d, "camera_3d" );
	scenes[1].rootNode->AddChild( cam3d );
	scenes[1].SetCamera( cam3d );
	scenes[1].enableDepth = true;

	transMatrix2D K;
	K << GetWidth(),             0,  GetWidth() /2,
				  0,    GetWidth(),  GetHeight()/2,
				  0,             0,              1;
	cam3d->SetPerspectiveProjection(K, GetWidth(), GetHeight(), 1.0, 10000 );
	cam3d->SetActive();
	cam3d->SetTransformation(I);


	// 2 -
	//   - root
	//      - ortho camera
	//      - other stuff.
	Rendering::NodeFactory::Create(rt, "root_2dFg" );
	scenes[2].SetRoot( rt );
	std::shared_ptr<CameraNode> fg2dcam; Rendering::NodeFactory::Create(fg2dcam, "camera_2dFg");
	scenes[2].rootNode->AddChild( fg2dcam );
	scenes[2].SetCamera( fg2dcam );
	scenes[2].enableDepth = false;

	fg2dcam->SetOrthoProjection(0, GetWidth(), 0, GetHeight(), -100, 100 );
	fg2dcam->SetActive();
	fg2dcam->SetTransformation(I);
}



std::shared_ptr<Rendering::MeshNode> Rendering::BasicHeadlessRenderer::SetBGImage(cv::Mat img)
{
	if( !bgImgNode )
	{
		bgImgTex.reset(  new Texture(smartThis) );
		bgImgTex->SetFilters( GL_NEAREST, GL_NEAREST );
		bgImgCard =  Rendering::GenerateCard(img.cols, img.rows, false);
		Rendering::NodeFactory::Create( bgImgNode, "convenienceBGImageNode");
	}

	bgImgTex->UploadImage(img);
	bgImgCard->UploadToRenderer( smartThis );
	bgImgNode->SetTexture(bgImgTex);
	bgImgNode->SetShader( GetShaderProg("basicShader") );
	bgImgNode->SetMesh( bgImgCard );
	

	Get2dBgRoot()->AddChild( bgImgNode );

	return bgImgNode;
}

void Rendering::BasicHeadlessRenderer::DeleteBGImage()
{
	if( bgImgNode != NULL )
	{
		bgImgNode->RemoveFromParent();
		bgImgNode = nullptr;
	}
	bgImgTex = nullptr;
	bgImgCard = nullptr;
}


bool Rendering::BasicHeadlessRenderer::StepEventLoop()
{
	SetActive();
	Render();
	return false;
}



void Rendering::BasicHeadlessRenderer::RunEventLoop()
{

}


void Rendering::BasicHeadlessRenderer::RenderToTexture(cv::Mat &res)
{
	// we use the format of res to configure the texture buffer, so res must be configured before hand.
	this->SetActive();
	
	// 1) create a frame buffer...
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	
	// 2) Create the texture we're going to render into.
	GLuint renderedTexture;
	glGenTextures(1, &renderedTexture);
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	
	
	switch( res.type() )
	{
		case CV_32FC4:
			glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA32F, res.cols, res.rows, 0,GL_BGRA, GL_FLOAT, 0);
			break;
		case CV_32FC3:
			glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB32F, res.cols, res.rows, 0,GL_BGR, GL_FLOAT, 0);
			break;
		case CV_8UC4:
			glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, res.cols, res.rows, 0,GL_BGRA, GL_UNSIGNED_BYTE, 0);
			break;
		case CV_8UC3:
			glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, res.cols, res.rows, 0,GL_BGR, GL_UNSIGNED_BYTE, 0);
			break;
		default:
			cout << "res type: " << res.type() << endl;
			throw( std::runtime_error("RenderToTexture: res must be configured on input to specify texture format" ) );
			break;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture, 0);
// 	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, DrawBuffers);
	
	// 3) And a depth buffer.
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, res.cols, res.rows);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);
	
	// 4) Error check.
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw( std::runtime_error("Didn't make framebuffer...") );
	}
	
	// 5) Render as normal.
	glViewport(0,0, res.cols, res.rows);
	glClearColor(0.0,0.0,0.0,1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	ParseSceneGraphs();
	for( unsigned sc = 0; sc < scenes.size(); ++sc )
	{
		scenes[sc].Render();
	}
	
	glFinish();
	
	// 6) Read the texture into res
	glBindTexture(GL_TEXTURE_2D, renderedTexture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	switch( res.type() )
	{
		case CV_32FC4:
			glGetTexImage( GL_TEXTURE_2D, 0, GL_BGRA, GL_FLOAT, res.data );
// 			glReadPixels(0, 0, res.cols, res.rows, GL_BGRA, GL_FLOAT, res.data );
			break;
		case CV_32FC3:
			glGetTexImage( GL_TEXTURE_2D, 0, GL_BGR, GL_FLOAT, res.data );
// 			glReadPixels(0, 0, res.cols, res.rows, GL_BGR, GL_FLOAT, res.data );
			break;
		case CV_8UC4:
			glGetTexImage( GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, res.data );
// 			glReadPixels(0, 0, res.cols, res.rows, GL_BGRA, GL_UNSIGNED_BYTE, res.data );
			break;
		case CV_8UC3:
			glGetTexImage( GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, res.data );
// 			glReadPixels(0, 0, res.cols, res.rows, GL_BGR, GL_UNSIGNED_BYTE, res.data );
			break;
		default:
			throw( std::runtime_error("RenderToTexture: res must be configured on input to specify texture format" ) );
			break;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// 7) Unbind the framebuffer and clean up
	glBindFramebuffer(GL_FRAMEBUFFER, this->framebufferName);
	
	glDeleteTextures(1, &renderedTexture );
	glDeleteRenderbuffers( 1, &depthrenderbuffer );
	glDeleteFramebuffers(1, &fbo);
	
	glViewport(0,0, GetWidth(), GetHeight() );
}


