#include "renderer2/i3dRenderer.h"
#include "renderer2/geomTools.h"
#include "math/matrixGenerators.h"



Rendering::I3DRenderer::I3DRenderer(unsigned width, unsigned height, std::string title) : BaseRenderer(width,height,title)
{
}

void Rendering::I3DRenderer::FinishConstructor()
{
	AbstractRenderer::FinishConstructor();
	
	// load core shaders.
	std::stringstream ss;
	ss << ccfg.shadersRoot << "/";
	
	LoadVertexShader(ss.str() + "basicVertex.glsl", "basicVertex");
	LoadVertexShader(ss.str() + "pcloudVertex.glsl", "pcloudVertex");
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
	CreateShaderProgram("pcloudVertex", "colourFrag", "pcloudShader");
// 	CreateShaderProgram("basicVertex", "thickLineGeom", "colourFrag", "thickLine");
	
	CreateShaderProgram("texLearnVertex", "colourFrag", "texLearnShader");
	
	
	
	InitialiseGraphs();
	
	leftMousePressed  = false;
	rightMousePressed = false;

	renMode           = I3DR_ORBIT;
	prevRenderTime = std::chrono::steady_clock::now();
}

Rendering::I3DRenderer::~I3DRenderer()
{

}


void Rendering::I3DRenderer::InitialiseGraphs()
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
	
	hVec3D o; o << 0,0,0,1;
	viewCentNode = GenerateCubeNode( o, 0.01, "viewCentreNode", smartThis );
	scenes[2].rootNode->AddChild( viewCentNode );
	
}

void Rendering::I3DRenderer::ResetViewCentre()
{
}

bool Rendering::I3DRenderer::HandleEvents()
{
	sf::Event ev;
	bool done = false;
	while( win.pollEvent(ev) )
	{
		if( ev.type == sf::Event::Closed )
			done = true;
				
		if( ev.type == sf::Event::KeyReleased )
		{
			if( ev.key.code == sf::Keyboard::Space )
			{
				switch( renMode )
				{
					case I3DR_ORBIT:
						renMode = I3DR_PIVOT;
						break;
					case I3DR_PIVOT:
						{
							renMode = I3DR_ORBIT;
							ResetViewCentre();
						}
						break;
				}
				leftMousePressed = false;
			}
		}
		
		if( ev.type == sf::Event::MouseButtonReleased )
		{
			// not using 
		}
	}
	return done;
}


void Rendering::I3DRenderer::HandlePivotCamera( float ft )
{
	
	transMatrix3D T = transMatrix3D::Identity();
	if( win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::W) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(2,3) = -5.0*ft;
		T = T0 * T;
	}
	else if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::S) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(2,3) =  5.0*ft;
		T = T0 * T;
	}
	
	if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::A) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(0,3) =  5.0*ft;
		T = T0 * T;
	}
	else if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::D) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(0,3) = -5.0*ft;
		T = T0 * T;
	}
	
	if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::R) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(1,3) =  5.0*ft;
		T = T0 * T;
	}
	else if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::F) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(1,3) = -5.0*ft;
		T = T0 * T;
	}
	
	
	if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::Q) )
	{
		transMatrix3D R = RotMatrix( 0.0, 0.0, ft );
		T = R * T;
	}
	else if(win.hasFocus() && sf::Keyboard::isKeyPressed(sf::Keyboard::E) )
	{
		transMatrix3D R = RotMatrix( 0.0, 0.0,-ft );
		T = R * T;
	}
	TransformView( T );
	
	
	if( win.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) )
	{
		auto mPos = sf::Mouse::getPosition(win);
		hVec2D curMousePos; curMousePos << mPos.x, mPos.y, 1.0;
		
		if( leftMousePressed )
		{
			hVec2D mouseMove = curMousePos - prevMousePos;
			RotateView( mouseMove, 0.2*ft );
		}
		
		leftMousePressed = true;
		prevMousePos = curMousePos;
	}
	else
		leftMousePressed = false;
}


void Rendering::I3DRenderer::HandleOrbitCamera( float ft )
{
	hVec3D camCentre = viewCalib.GetCameraCentre();
	hVec3D viewDir   = viewCentre - camCentre;
	
	
	//
	// Move the camera / view centre along the axes of the camera.
	//
	
	transMatrix3D T = transMatrix3D::Identity();
	if( sf::Keyboard::isKeyPressed(sf::Keyboard::W) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(2,3) = -5.0*ft;
		T = T0 * T;
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::S) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(2,3) =  5.0*ft;
		T = T0 * T;
	}
	
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::A) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(0,3) =  5.0*ft;
		T = T0 * T;
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::D) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(0,3) = -5.0*ft;
		T = T0 * T;
	}
	
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::R) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(1,3) =  5.0*ft;
		T = T0 * T;
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::F) )
	{
		transMatrix3D T0 = transMatrix3D::Identity();
		T0(1,3) = -5.0*ft;
		T = T0 * T;
	}
	
	viewCalib.L = T * viewCalib.L;
	camCentre = viewCalib.GetCameraCentre();
	viewCentre = camCentre + viewDir;
	
	
	
	
	
	
	//
	// Roll around camera view axis.
	//
	T = transMatrix3D::Identity();
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Q) )
	{
		transMatrix3D R = RotMatrix( 0.0, 0.0, ft );
		T = R * T;
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::E) )
	{
		transMatrix3D R = RotMatrix( 0.0, 0.0,-ft );
		T = R * T;
	}
	viewCalib.L = T * viewCalib.L;
	
	
	
	
	//
	// Move closer to/away from view centre.
	//
	T = transMatrix3D::Identity();
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::T) )
	{
		hVec3D ovd       = viewDir;
		viewDir.head(3)  = (viewDir.head(3) - ft*1.0f*viewDir.head(3) );
		hVec3D t         = ovd - viewDir;
		T.block(0,3,3,1) = t.head(3);
		
		cout << "---" << endl;
		cout << viewDir.transpose() << endl;
		cout << ovd.transpose() << endl;
		cout << T << endl;
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::G) )
	{
		hVec3D ovd       = viewDir;
		viewDir.head(3)  = viewDir.head(3) + ft*1.0*viewDir.head(3);
		hVec3D t         = ovd - viewDir;
		T.block(0,3,3,1) = t.head(3);
		
		cout << "---" << endl;
		cout << viewDir.transpose() << endl;
		cout << ovd.transpose() << endl;
		cout << T << endl;
	}
	viewCalib.L = viewCalib.L * T;
	
	
	
	//
	// Rotate around view centre based on mouse motion.
	//
	hVec2D mouseMove; mouseMove << 0,0,0;
	if( sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) )
	{
		auto mPos = sf::Mouse::getPosition(win);
		hVec2D curMousePos; curMousePos << mPos.x, mPos.y, 1.0;
		
		if( leftMousePressed )
		{
			mouseMove = curMousePos - prevMousePos;
		}
		
		leftMousePressed = true;
		prevMousePos = curMousePos;
	}
	else
	{
		leftMousePressed = false;
	}
	
	OrbitView( mouseMove, 1.0*ft );
	
	
	// keep view centre node in the right place.
	transMatrix3D VCT = transMatrix3D::Identity();
	VCT.col(3) = viewCentre;
	viewCentNode->SetTransformation( VCT );
}

bool Rendering::I3DRenderer::StepEventLoop()
{
	Render();
	
	auto t0 = std::chrono::steady_clock::now();
	auto ft = std::chrono::duration <double, std::milli>(t0-prevRenderTime).count() / 1000.0;
	
	win.setActive();
	
	bool done = HandleEvents();
	switch( renMode )
	{
		case I3DR_ORBIT:
			HandleOrbitCamera( ft ); break;
		case I3DR_PIVOT:
			HandlePivotCamera( ft ); break;
	}
	
	
	
	prevRenderTime = t0;
	
	return done;
}

void Rendering::I3DRenderer::TransformView( transMatrix3D T )
{
	viewCalib.L = T * viewCalib.L;
	
	Set3DCamera( viewCalib, near, far );
}

void Rendering::I3DRenderer::RotateView( hVec2D mm, float time )
{
	// mm is some vector in screen space. 
	// If we rotate around a perpendicular vector that should be great... right?
	
	hVec3D fwd;
	fwd << 0,0,1,0;
	
	hVec3D mm3;
	mm3 << mm(0), mm(1), 0.0, 0.0f;
	float a = mm3.norm();
	
	if( a > 0 )
	{
		mm3 /= a;
		a *= time;
		hVec3D rax = Cross( fwd, mm3 );
		
		transMatrix3D R = RotMatrix( rax, -a );
		
		viewCalib.L = R * viewCalib.L; // ?
		
		Set3DCamera( viewCalib, near, far );
	}
}

void Rendering::I3DRenderer::OrbitView( hVec2D mm, float time )
{
	//
	// move the camera in camera space along the mm vector.
	// I know this is ridiculous but, it works.
	//
	hVec3D tc; tc << -0.1*mm(0), -0.1*mm(1), 0.0, 0.0f;
	hVec3D tw = viewCalib.TransformToWorld( tc );
	
	hVec3D up; up << 0,-1,0,0.0f;
	up = viewCalib.TransformToWorld( up );
	
	hVec3D cc = viewCalib.GetCameraCentre();
	hVec3D vd = cc - viewCentre;
	
	hVec3D tmp;
	tmp = cc + tw;
	
	hVec3D vd2 = tmp - viewCentre;
	hVec3D eye = viewCentre + vd.norm() * vd2/vd2.norm();
	
	viewCalib.L = LookAt( eye, up, viewCentre );
	Set3DCamera( viewCalib, near, far );
}



void Rendering::I3DRenderer::RunEventLoop()
{

}
