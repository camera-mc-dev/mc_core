#include "renderer2/basicHeadlessRenderer.h"
#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "renderer2/sdfText.h"
#include "renderer2/showImage.h"
#include "imgio/loadsave.h"


#include <iostream>
using std::cout;
using std::endl;


#include <Eigen/Geometry>

using namespace Rendering;


int main(void)
{
	std::shared_ptr<BasicHeadlessRenderer> ren;
	Rendering::RendererFactory::Create(ren, 1024, 512, "basic headless renderer test" );
	
	cout << "err after create: " <<  glGetError() << endl;
	
	// Image on the Background
	cv::Mat img = LoadImage("data/bagpipe.jpg");
	ren->SetBGImage(img);
	ren->Get2dBgCamera()->SetOrthoProjection(0,1024,0,512,-100,100);
	
	
	// cube on the 3D
	Calibration calib;
	calib.K << 512,  0, 512,
	             0,512, 256,
	             0,  0,   1;
	calib.L = transMatrix3D::Identity();
	calib.width = 1024;
	calib.height = 512;
	ren->Get3dCamera()->SetFromCalibration(calib, 150, 10000);
	hVec3D c; c << 0,0,3000,1.0;
	std::shared_ptr<Rendering::MeshNode> cn = GenerateCubeNode( c, 800, "cube", ren );
	Eigen::Vector4f white; white << 0.97, 0.97, 0.97, 1.0f;
	cn->SetBaseColour(white);
	ren->Get3dRoot()->AddChild(cn);
	
	
	
	
	// circle on the FG
	ren->Get2dFgCamera()->SetOrthoProjection(0,1024,0,512,-100,100);
	
	hVec2D ic; ic << 600, 200, 1.0f;
	auto in = GenerateCircleNode2D( ic, 200, 10, "circle",  ren );
	
	cn->SetBaseColour(white);
	ren->Get2dFgRoot()->AddChild( in );
	
	
	
	genMatrix verts = cn->GetMesh()->vertices;
	for( int cc = 0; cc < verts.cols(); ++cc )
	{
		cout << verts.col(cc).transpose() << "  ->  " << calib.Project( verts.col(cc) ).transpose() << endl;
	}
	
	ren->StepEventLoop();
	cout << "a: " << glGetError() << endl;
		
	cv::Mat grab = ren->Capture();
	
	cout << "b: " << endl;
	SaveImage(grab, "tst.jpg");
	
	cv::Mat i( 1024, 1024, CV_32FC3, cv::Scalar(0,0,0,0) );
	ren->RenderToTexture(i);
	i = i * 255.0f;
	SaveImage(i, "tst2.jpg");
	
	
}
