#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "imgio/loadsave.h"
#include "math/mathTypes.h"

#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "renderer2/sdfText.h"

#include <map>
#include <iostream>
#include <fstream>
using std::cout;
using std::endl;

#include "libconfig.h++"

#include "commonConfig/commonConfig.h"

class PointMatcher;

class PointMatchRenderer : public Rendering::BasicRenderer
{

public:
    friend class Rendering::RendererFactory;		
protected:
	PointMatchRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title)
	{
	}
public:
	virtual bool StepEventLoop();	
	bool StepSBPEventLoop( hVec3D &p );
	
private:
	using BasicRenderer::StepEventLoop;
	
	PointMatcher *pm;
	
	friend class PointMatcher;
};


class PointMatcher
{
public:
	
	struct PointMatch
	{
		unsigned id;
		std::map<std::string, hVec3D> imgPoints;
	};
	
	PointMatcher( std::string networkConfigFilename );
	void Load();
	void Save();
	
	void Run();
	void CreateInterface();
	void SetImages();
	void SetPID();
	void SetPoints();
	
	void ClickTopBar( hVec2D p );
	void ClickBotBar( hVec2D p );
	void ClickImgRegion( hVec2D p );
	
	hVec3D GetSubPixel( hVec3D p, cv::Mat img );
	
	
	std::map< std::string, ImageSource* > sources;
	std::map< unsigned, PointMatch > matches;
	std::string matchesFile;
	
	friend class PointMatchRenderer;
	
	
	// rendering stuff.
	std::shared_ptr<PointMatchRenderer> ren;
	Rendering::SDFText *textMaker;
	
	// interface and controls.
	unsigned winX, winY;
	std::shared_ptr<Rendering::MeshNode>  topBar, pidDown, pidUp, lImg, rImg;
	std::shared_ptr<Rendering::MeshNode>  botBar, lfDown, lfUp, rfDown, rfUp;
	std::shared_ptr<Rendering::SceneNode> pidText, pidDltText, saveBtnText, lfCam, rfCam, imgsRoot;
	std::shared_ptr<Rendering::SceneNode> lPointsRoot, rPointsRoot;

	
	// state
	std::string leftCam, rightCam;
	unsigned    pid;
	
	CommonConfig ccfg;
	
};








PointMatcher::PointMatcher(std::string networkConfigFilename)
{
	// read network config file to get image sources etc.
	libconfig::Config cfg;
	std::string dataRoot = ccfg.dataRoot;
	std::string testRoot;

	std::map< std::string, std::string > imgDirs;

	std::map< std::string, unsigned > frameInds;
	unsigned ignoreBeforeFrame = 0;
	winX = 1200;
	winY = 800;
	try
	{
		cfg.readFile(networkConfigFilename.c_str() );

		if( cfg.exists("dataRoot") )
			dataRoot = (const char*)cfg.lookup("dataRoot");
		testRoot = (const char*)cfg.lookup("testRoot");

		if( cfg.exists("winX") )
			winX = cfg.lookup("winX");
		else
			winX = 1200;

		if( cfg.exists("winY") )
			winY = cfg.lookup("winY");
		else
			winY = 1200;
		
		float ar = winY / (float)winX;
		if( winX > ccfg.maxSingleWindowWidth )
		{
			winX = ccfg.maxSingleWindowWidth;
			winY = ar * winX;
		}
		if( winY > ccfg.maxSingleWindowHeight )
		{
			winY = ccfg.maxSingleWindowHeight;
			winX = winY / ar;
		}

		matchesFile = dataRoot + testRoot + (const char*)  cfg.lookup("matchesFile");


		libconfig::Setting &idirs  = cfg.lookup("imgDirs");
		libconfig::Setting &frames = cfg.lookup("frameInds");
		for( unsigned ic = 0; ic < idirs.getLength(); ++ic )
		{
			std::string s;
			s = dataRoot + testRoot + (const char*) idirs[ic];
			imgDirs[(const char*) idirs[ic] ] = s;
			frameInds[(const char*) idirs[ic] ] = (unsigned) frames[ic];
		}
	}
	catch( libconfig::SettingException &e)
	{
		cout << "Setting error: " << endl;
		cout << e.what() << endl;
		cout << e.getPath() << endl;
		exit(2);
	}
	catch( libconfig::ParseException &e )
	{
		cout << "Parse error:" << endl;
		cout << e.what() << endl;
		cout << e.getError() << endl;
		cout << e.getFile() << endl;
		cout << e.getLine() << endl;
		exit(3);
	}
	
	
	// now create the image sources
	for( auto i = imgDirs.begin(); i != imgDirs.end(); ++i )
	{
		cout << "creating source: " << i->first << " : " << i->second << endl;
		ImageSource *isrc;
		if( boost::filesystem::is_directory( i->second ))
		{
			isrc = (ImageSource*) new ImageDirectory(i->second);
// 			isDirectorySource.push_back(true);
		}
		else
		{
			isrc = (ImageSource*) new VideoSource(i->second, "none");
// 			isDirectorySource.push_back(false);
		}
		sources[i->first] = isrc;
		sources[i->first]->JumpToFrame( frameInds[i->first] );
	}
	
	
	leftCam = sources.begin()->first;
	rightCam = sources.begin()->first;
	pid = 0;
	
	// load existing matches if they exist.
	cout << "Loading previous matches" << endl;
	Load();
	
}


void PointMatcher::Run()
{
	pid = 0;
	CreateInterface();
	
	SetImages();
	SetPID();
	
	while(1)
	{
		ren->StepEventLoop();
	}
}


void PointMatcher::ClickTopBar( hVec2D p )
{
	if( p(0) >= 10 &&  p(0) <= 20 )
	{
		++pid;
		SetPID();
	}
	
	if( p(0) >= 30 &&  p(0) <= 40 )
	{
		if( pid > 0 )
			--pid;
		SetPID();
	}
	
	// TODO: Save and delete buttons.
}

void PointMatcher::ClickBotBar( hVec2D p )
{
	if( p(0) >= 10 && p(0) <= 20 )
	{
		auto i = sources.find( leftCam );
		if( i != sources.begin() )
			--i;
		leftCam = i->first;
		SetImages();
	}
	
	if( p(0) >= winX/2-20 && p(0) <= winX/2-10 )
	{
		auto i = sources.find( leftCam );
		++i;
		if( i != sources.end() )
			leftCam = i->first;
		SetImages();
	}
	
	if( p(0) >= winX/2 + 10 && p(0) <= winX/2 + 20 )
	{
		auto i = sources.find( rightCam );
		if( i != sources.begin() )
			--i;
		rightCam = i->first;
		SetImages();
	}
	
	if( p(0) >= winX-20 && p(0) <= winX-10 )
	{
		auto i = sources.find( rightCam );
		++i;
		if( i != sources.end() )
			rightCam = i->first;
		SetImages();
	}
}

void PointMatcher::ClickImgRegion( hVec2D p )
{
	hVec3D p3; p3 << p(0), p(1), 0.0, 1.0;
	
// 	cout << "wx/2, wy/2: " << winX / 2 << " " << winY/2 << endl;
	
	std::string img;
	if( p(0) < winX/2 )
	{
// 		cout << pid << " " << leftCam << endl;
// 		cout << p3.transpose() << "  ----> "; 
		p3 = lPointsRoot->GetTransformation().inverse() * p3;
// 		cout << p3.transpose() << endl;
		
		matches[pid].id = pid;
		matches[pid].imgPoints[leftCam] = GetSubPixel(p3, sources[leftCam]->GetCurrent() ) ;
		
		
	}
	else
	{
// 		cout << pid << " " << rightCam << endl;
// 		cout << p3.transpose() << "  ----> "; 
		p3 = rPointsRoot->GetTransformation().inverse() * p3;
// 		cout << p3.transpose() << endl;
		
		matches[pid].id = pid;
		matches[pid].imgPoints[rightCam] = GetSubPixel(p3, sources[rightCam]->GetCurrent() ) ;
		

	}
	
	SetPoints();
	
	
}


hVec3D PointMatcher::GetSubPixel( hVec3D p, cv::Mat img )
{
	cv::Rect bb;
	bb.x = std::max(0, (int)p(0) - 100 );
	bb.y = std::max(0, (int)p(1) - 100 );
	bb.width = 200;
	bb.height = 200;
	
	if( bb.x + bb.width > img.cols )
		bb.x = img.cols - bb.width - 1;
	if( bb.y + bb.width > img.rows )
		bb.y = img.rows - bb.width - 1;
	
	cv::Mat win = img(bb).clone();
	
	// we should be able to render it 4 times larger...
	auto n = Rendering::GenerateImageNode( winX/2-400, winY/2-400, 800, win, "subPixWin", ren );
	
	ren->Get2dFgRoot()->AddChild(n);
	
	hVec3D sbp;
	while( !ren->StepSBPEventLoop( sbp ) );
	
	// now we should be able to figure out where the click is in the actual image.
	// sbp comes back to us in screen coords.
	cout << "sbp: " << sbp.transpose() << endl;
	
	// we can transform it to be relative to the centre of the image,
	// which is where our bounding box is.
	float x,y;
	x = sbp(0) - winX/2;
	y = sbp(1) - winY/2;
	
	cout << "x,y: " << x << " " << y << endl;
	
	// we've quadrupled the size of the image for display in the window,
	// so x and y must be downsized...
	x /= 4.0f;
	y /= 4.0f;
	
	// they are now offsets from the centre of the bounding box bb, which is (bb.x+100, bb.y+100)
	// so the final pixel position in the image is...
	sbp(0) = bb.x+100+x;
	sbp(1) = bb.y+100+y;
	
	
	n->RemoveFromParent();
	
	return sbp;
}

bool PointMatchRenderer::StepSBPEventLoop( hVec3D &p )
{
	Render();

	win.setActive();
	sf::Event ev;
	while( win.pollEvent(ev) )
	{
		// quit...
		if( ev.type == sf::Event::Closed )
		{
			pm->Save();
			exit(0);
		}
		
		// mouse clicked.
		if( ev.type == sf::Event::MouseButtonReleased )
		{
			p << ev.mouseButton.x, ev.mouseButton.y, 0.0f, 1.0f;
			
			return true;
		}
	}
	
	return false;
}

bool PointMatchRenderer::StepEventLoop()
{
	Render();

	win.setActive();
	sf::Event ev;
	while( win.pollEvent(ev) )
	{
		// quit...
		if( ev.type == sf::Event::Closed )
		{
			pm->Save();
			exit(0);
		}
		
		// mouse clicked.
		if( ev.type == sf::Event::MouseButtonReleased )
		{
			hVec2D p; p << ev.mouseButton.x, ev.mouseButton.y, 1.0f;
			
			if( p(1) < 30 )
			{
				// we're in the top bar.
				pm->ClickTopBar(p);
			}
			else if( p(1) > pm->winY-30 )
			{
				// we're in the bottom bar.
				pm->ClickBotBar(p);
			}
			else
			{
				// we're in the image region
				pm->ClickImgRegion(p);
				
			}
		}
		
		else if( ev.type == sf::Event::KeyPressed )
		{
			if (ev.key.code == sf::Keyboard::Up )
			{
				++pm->pid;
				pm->SetPID();
			}
			if (ev.key.code == sf::Keyboard::Down )
			{
				if( pm->pid > 0 )
					--pm->pid;
				pm->SetPID();
			}
			
			if(ev.key.code == sf::Keyboard::A )
			{
				auto i = pm->sources.find( pm->leftCam );
				if( i != pm->sources.begin() )
					--i;
				pm->leftCam = i->first;
				pm->SetImages();
			}
			
			if(ev.key.code == sf::Keyboard::S )
			{
				auto i = pm->sources.find( pm->leftCam );
				++i;
				if( i != pm->sources.end() )
					pm->leftCam = i->first;
				pm->SetImages();
			}
			
			if(ev.key.code == sf::Keyboard::Z )
			{
				auto i = pm->sources.find( pm->rightCam );
				if( i != pm->sources.begin() )
					--i;
				pm->rightCam = i->first;
				pm->SetImages();
			}
			
			if(ev.key.code == sf::Keyboard::X )
			{
				auto i = pm->sources.find( pm->rightCam );
				++i;
				if( i != pm->sources.end() )
					pm->rightCam = i->first;
				pm->SetImages();
			}
		}
	}
	return false;
}

void PointMatcher::SetPID()
{
	pidText->Clean();
	
	std::stringstream ss;
	ss << "pointID: " << pid;
	textMaker->RenderString( ss.str(), 25, 0.0, 1.0, 1.0, pidText );
	
	SetPoints(); // change colour of active point.
}

void PointMatcher::SetImages()
{
	imgsRoot->Clean();
	transMatrix3D T = transMatrix3D::Identity();
	T(0,3) = 0;
	T(1,3) = 30;
	imgsRoot->SetTransformation(T);
	
	cv::Mat l, r;
	l = sources[ leftCam ]->GetCurrent();
	r = sources[ rightCam ]->GetCurrent();
	
	lImg = Rendering::GenerateImageNode(0, 0, winX/2, l, "leftImage", ren );
	rImg = Rendering::GenerateImageNode(winX/2, 0, winX/2, r, "rightImage", ren );
	
	
	
	imgsRoot->AddChild( lImg );
	imgsRoot->AddChild( rImg );
	
	lfCam->Clean();
	rfCam->Clean();
	textMaker->RenderString( leftCam,  25, 0.0, 1.0, 1.0, lfCam );
	textMaker->RenderString( rightCam, 25, 0.0, 1.0, 1.0, rfCam );
	
	T = transMatrix3D::Identity();
	transMatrix3D S; S = T;	// i.e. identity.
	
	// first, a scaling so that s* pi => ps
	float s = (winX/2) / (float) l.cols;
	S(0,0) = S(1,1) = s;
	
	// translation caused by the top bar.
	T(1,3) = 30;
	lPointsRoot->SetTransformation( T * S );
	cout << "teft T: " << endl << lPointsRoot->GetTransformation() << endl << endl;
	
	// right view also has a translation in X.
	T(0,3) = winX/2;
	rPointsRoot->SetTransformation( T * S );
	cout << "right T: " << endl << rPointsRoot->GetTransformation() << endl << endl;
	
	SetPoints();
}

void PointMatcher::SetPoints()
{
	lPointsRoot->Clean();
	rPointsRoot->Clean();
	
	for( auto pidi = matches.begin(); pidi != matches.end(); ++pidi )
	{
		auto l = pidi->second.imgPoints.find( leftCam );
		auto r = pidi->second.imgPoints.find( rightCam );
		std::stringstream ss;
		ss << "point:" << pidi->first;
		if( l != pidi->second.imgPoints.end() )
		{
			hVec2D p2;
			p2 << l->second(0), l->second(1), 1.0;
			
			auto n = GenerateCircleNode2D( p2, 10, 3, ss.str(), ren );
			
			if( pidi->first == pid )
			{
				Eigen::Vector4f red; red << 1.0, 0, 0, 1.0;
				n->SetBaseColour(red);
			}
			
			lPointsRoot->AddChild(n);
		}
		
		if( r != pidi->second.imgPoints.end() )
		{
			hVec2D p2;
			p2 << r->second(0), r->second(1), 1.0;
			
			auto n = GenerateCircleNode2D( p2, 15, 3, ss.str(), ren );
			
			if( pidi->first == pid )
			{
				Eigen::Vector4f red; red << 1.0, 0, 0, 1.0;
				n->SetBaseColour(red);
			}
			
			rPointsRoot->AddChild(n);
		}
	}
}

void PointMatcher::CreateInterface()
{
	// first, we need a window...
	Rendering::RendererFactory::Create( ren, winX, winY, "point matcher" );
	ren->Get2dBgCamera()->SetOrthoProjection(0, winX, 0, winY, -100, 100 );
	ren->Get2dFgCamera()->SetOrthoProjection(0, winX, 0, winY, -100, 100 );
	ren->pm = this;
	
	// and something to render text.
	std::stringstream fntss;
	fntss << ccfg.coreDataRoot << "/NotoMono-Regular.ttf";
	textMaker = new Rendering::SDFText (fntss.str(), ren);
	
	// and some colours.
	Eigen::Vector4f ifaceBaseColour; ifaceBaseColour << 0.6,0.6,0.7,1.0;
	Eigen::Vector4f ifaceButColour;  ifaceButColour  << 0.7,0.9,0.9,1.0;
	
	// Top bar
	auto tbm = Rendering::GenerateCard(winX, 30, false);
	tbm->UploadToRenderer(ren);
	Rendering::NodeFactory::Create( topBar, "top bar" );
	topBar->SetShader( ren->GetShaderProg("basicColourShader") );
	topBar->SetMesh( tbm );
	topBar->SetTexture( ren->GetBlankTexture() );
	topBar->SetBaseColour( ifaceBaseColour );
	
	// point id control
	std::shared_ptr<Rendering::Mesh> pidUpArrowMesh( new Rendering::Mesh(3, 1) );
	pidUpArrowMesh->vertices <<  0.0,   10,  -10,
	                             0.0,   10,   10,
	                               0,    0,    0,
	                             1.0,  1.0,  1.0,
	pidUpArrowMesh->faces    << 0,1,2;
	pidUpArrowMesh->UploadToRenderer(ren);
	
	Rendering::NodeFactory::Create( pidUp, "pidUp" );
	pidUp->SetShader( ren->GetShaderProg("basicColourShader") );
	pidUp->SetMesh( pidUpArrowMesh );
	pidUp->SetTexture( ren->GetBlankTexture() );
	pidUp->SetBaseColour( ifaceButColour );
	
	transMatrix3D T; T = transMatrix3D::Identity();
	T(0,3) = 10;
	T(1,3) = 10;
	pidUp->SetTransformation(T);
	
	
	std::shared_ptr<Rendering::Mesh> pidDownArrowMesh( new Rendering::Mesh(3, 1) );
	pidDownArrowMesh->vertices <<  0.0,   10,  -10,
	                                10,    0,    0,
	                                 0,    0,    0,
	                               1.0,  1.0,  1.0,
	pidDownArrowMesh->faces    << 0,1,2;
	pidDownArrowMesh->UploadToRenderer(ren);
	
	Rendering::NodeFactory::Create( pidDown, "pidDown" );
	pidDown->SetShader( ren->GetShaderProg("basicColourShader") );
	pidDown->SetMesh( pidDownArrowMesh );
	pidDown->SetTexture( ren->GetBlankTexture() );
	pidDown->SetBaseColour( ifaceButColour );
	
	T = transMatrix3D::Identity();
	T(0,3) = 30;
	T(1,3) = 10;
	pidDown->SetTransformation(T);
	
	Rendering::NodeFactory::Create( imgsRoot, "imgsRoot" );
	
	Rendering::NodeFactory::Create( pidText, "pidText" );
	T(0,3) = 55;
	T(1,3) = 25;
	pidText->SetTransformation(T);
	
	Rendering::NodeFactory::Create( pidDltText, "pidDltText" );
	textMaker->RenderString( "Delete point", 25, 0.0, 1.0, 1.0, pidDltText );
	T(0,3) = 300;
	T(1,3) = 25;
	
	pidDltText->SetTransformation(T);
	
	Rendering::NodeFactory::Create( saveBtnText, "saveBtnText" );
	textMaker->RenderString( "Save", 25, 0.0, 1.0, 1.0, saveBtnText );
	T(0,3) = winX - 100;
	T(1,3) = 25;
	saveBtnText->SetTransformation(T);
	
	
	// two main image areas are created and adjusted as the images are loaded and displayed, so skip them here...
	
	
	// the bottom bars are then...
	auto bbm = Rendering::GenerateCard(winX, 30, false);
	bbm->UploadToRenderer(ren);
	Rendering::NodeFactory::Create( botBar, "botBar" );
	botBar->SetShader( ren->GetShaderProg("basicColourShader") );
	botBar->SetMesh( bbm );
	botBar->SetTexture( ren->GetBlankTexture() );
	botBar->SetBaseColour( ifaceBaseColour );
	T(0,3) = 0; T(1,3) = winY - 30;
	botBar->SetTransformation( T );
	
	
	Rendering::NodeFactory::Create( lfDown, "lfDown" );
	lfDown->SetShader( ren->GetShaderProg("basicColourShader") );
	lfDown->SetMesh( pidDownArrowMesh );
	lfDown->SetTexture( ren->GetBlankTexture() );
	lfDown->SetBaseColour( ifaceButColour );
	
	T(0,3) = 20;
	T(1,3) = winY - 20;
	lfDown->SetTransformation(T);
	
	Rendering::NodeFactory::Create( lfCam, "lfCam" );
	T(0,3) = winX/4 - 30;
	T(1,3) = winY - 10;
	lfCam->SetTransformation(T);
	
	Rendering::NodeFactory::Create( lfUp, "lfUp" );
	lfUp->SetShader( ren->GetShaderProg("basicColourShader") );
	lfUp->SetMesh( pidUpArrowMesh );
	lfUp->SetTexture( ren->GetBlankTexture() );
	lfUp->SetBaseColour( ifaceButColour );
	
	T(0,3) = winX/2 - 20;
	T(1,3) = winY - 20;
	lfUp->SetTransformation(T);
	
	
	Rendering::NodeFactory::Create( rfDown, "rfDown" );
	rfDown->SetShader( ren->GetShaderProg("basicColourShader") );
	rfDown->SetMesh( pidDownArrowMesh );
	rfDown->SetTexture( ren->GetBlankTexture() );
	rfDown->SetBaseColour( ifaceButColour );
	
	T(0,3) = winX/2 + 20;
	T(1,3) = winY - 20;
	rfDown->SetTransformation(T);
	
	Rendering::NodeFactory::Create( rfCam, "rfCam" );
	T(0,3) = 3*winX/4 - 30;
	T(1,3) = winY - 10;
	rfCam->SetTransformation(T);
	
	Rendering::NodeFactory::Create( rfUp, "rfUp" );
	rfUp->SetShader( ren->GetShaderProg("basicColourShader") );
	rfUp->SetMesh( pidUpArrowMesh );
	rfUp->SetTexture( ren->GetBlankTexture() );
	rfUp->SetBaseColour( ifaceButColour );
	
	T(0,3) = winX - 20;
	T(1,3) = winY - 20;
	rfUp->SetTransformation(T);
	
	
	
	// To make life easy, we want to create a transformation T such that points in images space 
	// are mapped to screen space if they are children of this node...
	Rendering::NodeFactory::Create( lPointsRoot, "lPointsRoot" );
	Rendering::NodeFactory::Create( rPointsRoot, "rPointsRoot" );
	
	
	
	
	ren->Get2dBgRoot()->AddChild( topBar );
	ren->Get2dBgRoot()->AddChild( botBar );
	
	ren->Get2dFgRoot()->AddChild( pidDown );
	ren->Get2dFgRoot()->AddChild( pidUp );
	ren->Get2dFgRoot()->AddChild( pidDltText );
	ren->Get2dFgRoot()->AddChild( pidText );
	ren->Get2dFgRoot()->AddChild( saveBtnText );
	
	ren->Get2dFgRoot()->AddChild( lfDown );
	ren->Get2dFgRoot()->AddChild( lfUp );
	ren->Get2dFgRoot()->AddChild( lfCam );
	
	ren->Get2dFgRoot()->AddChild( rfDown );
	ren->Get2dFgRoot()->AddChild( rfUp );
	ren->Get2dFgRoot()->AddChild( rfCam );
	
	ren->Get2dBgRoot()->AddChild( imgsRoot );
	ren->Get2dFgRoot()->AddChild( lPointsRoot );
	ren->Get2dFgRoot()->AddChild( rPointsRoot );
}


int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		cout << "Tool to perform manual point matches to aide calibration where grid-matches alone are insufficient" << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <config file> " << endl;
		return(1);
	}
	
	PointMatcher pm( argv[1] );
	
	cout << "pid pre run: " << pm.pid << endl;
	pm.Run();
}



void PointMatcher::Save()
{
	std::ofstream outfi( matchesFile );

	outfi << sources.size() << endl;
	for( auto si = sources.begin(); si != sources.end(); ++si )
	{
		outfi << si->first << endl;
	}
	outfi << matches.size() << endl;
	for(auto mi = matches.begin(); mi != matches.end(); ++mi )
	{
		PointMatch &pm = mi->second;

		outfi << pm.id << " " << pm.imgPoints.size() << endl;

		for(auto pi = pm.imgPoints.begin(); pi != pm.imgPoints.end(); ++pi )
		{
			outfi << pi->first << " ";
			outfi << pi->second(0) << " " << pi->second(1) << " " << 1 << endl;
		}
	}
}

void PointMatcher::Load()
{
	pid = 0;
	
	matches.clear();
	std::ifstream infi( matchesFile );
	if( !infi )
	{
		cout << "Problem loading matches file: " << matchesFile << endl;
		return;
	}
	
	unsigned numSources;
	infi >> numSources;
	std::vector<std::string> srcIds;
	for(unsigned ic = 0; ic < numSources; ++ic )
	{
		std::string s;
		infi >> s;
		srcIds.push_back(s);
	}

	unsigned numMatches;
	infi >> numMatches;
	for( unsigned mc = 0; mc < numMatches; ++mc )
	{
		unsigned id;
		unsigned numViews;
		infi >> id;
		infi >> numViews;

		matches[id].id = id;
		for(unsigned vc = 0; vc < numViews; ++vc )
		{
			std::string sid;
			infi >> sid;
			hVec3D p;
			infi >> p(0);
			infi >> p(1);
			p(2) = 0;
			infi >> p(3);

			matches[id].imgPoints[sid] = p;
		}
	}
	
	pid = (matches.begin())->first;
	cout << "loaded " << numMatches << endl;
}
