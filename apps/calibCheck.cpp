#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>
#include <set>
#include <string>
using std::cout;
using std::endl;
using std::vector;

#include <Eigen/Geometry>

#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "calib/circleGridTools.h"
#include "math/intersections.h"
#include "math/products.h"

#include "libconfig.h++"


#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "renderer2/sdfText.h"

#include "commonConfig/commonConfig.h"

#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <map>
using std::cout;
using std::endl;
using std::vector;


class CalCheckRenderer : public Rendering::BasicRenderer
{
	friend class Rendering::RendererFactory;	
	// The constructor should be private or protected so that we are forced 
	// to use the factory...
protected:
	// the constructor creates the renderer with a window of the specified
 	// size, and with the specified title.
	CalCheckRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title) {}
public:
	bool Step(int &camChange, bool &clicked, hVec2D &clickPos )
	{
		Render();

		win.setActive();
		sf::Event ev;
		while( win.pollEvent(ev) )
		{
			if( ev.type == sf::Event::Closed )
				return false;
			if (ev.type == sf::Event::MouseButtonReleased)
			{
				clickPos << ev.mouseButton.x, ev.mouseButton.y, 1.0f;
				clicked = true;
			}
			if (ev.type == sf::Event::KeyReleased)
			{
				if (ev.key.code == sf::Keyboard::Left )
				{
					camChange = -1;
				}
				if (ev.key.code == sf::Keyboard::Right )
				{
					camChange = 1;
				}
			}
		}
		return true;
	}
};


void DebugOutput(vector< transMatrix3D > Ls);


struct PointMatch
{
	unsigned id;
	std::map<std::string, hVec2D> obs;
	std::map<std::string, hVec2D> proj;
	hVec3D p3d;
};

void LoadPointMatches( std::string matchesFile, std::map< unsigned, PointMatch > &matches )
{
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
			hVec2D p;
			infi >> p(0);
			infi >> p(1);
			infi >> p(2);	// should be 1.0

			matches[id].obs[sid] = p;
		}
	}
	
	cout << "loaded " << numMatches << endl;
}

std::vector< ImageSource* > sources;
void ResCheck( std::vector< ImageSource* > &sources, std::vector< std::vector<float> > &cube );

int main(int argc, char *argv[] )
{
	
	CommonConfig ccfg;
	
	// parse config file
	libconfig::Config cfg;

	// the settings that we want...
	std::string dataRoot = ccfg.dataRoot;
	std::string testRoot;
	vector< std::string > imgDirs;
	vector< std::string > vidFiles;
	std::map< unsigned, std::string> calibFiles;
	std::map< std::string, unsigned > srcId2Indx;
	std::map< unsigned, std::string > srcIndx2Id;
	float targetDepth = 0;
	unsigned originFrame = 0;
	std::string matchesFile("none");
	bool renderCube = false;
	std::vector< std::vector<float> > cube;
	
	bool resCheck = false;
	
	
	try
	{
		cfg.readFile(argv[1]);

		if( cfg.exists("dataRoot") )
			dataRoot = (const char*)cfg.lookup("dataRoot");
		testRoot = (const char*)cfg.lookup("testRoot");
		
		if( cfg.exists("imgDirs") )
		{
			libconfig::Setting &idirs = cfg.lookup("imgDirs");
			for( unsigned ic = 0; ic < idirs.getLength(); ++ic )
			{
				std::string s;
				s = dataRoot + testRoot + (const char*) idirs[ic];
				srcId2Indx[(const char*)idirs[ic]] = imgDirs.size();
				srcIndx2Id[ imgDirs.size() ] = (const char*)idirs[ic];
				imgDirs.push_back(s);
			}
		}
		else if( cfg.exists("vidFiles") && cfg.exists("calibFiles") )
		{
			libconfig::Setting &vfs = cfg.lookup("vidFiles");
			for( unsigned ic = 0; ic < vfs.getLength(); ++ic )
			{
				std::string s;
				s = dataRoot + testRoot + (const char*) vfs[ic];
				vidFiles.push_back(s);
			}
			
			libconfig::Setting &cfs = cfg.lookup("calibFiles");
			for( unsigned ic = 0; ic < cfs.getLength(); ++ic )
			{
				std::string s;
				s = dataRoot + testRoot + (const char*) cfs[ic];
				calibFiles[ic] = s;
			}
		}
		
		if( cfg.exists("cube") )
		{
			libconfig::Setting &cs = cfg.lookup("cube");
			assert( cs.getLength() == 3 );
			cube.resize(3);
			for( unsigned ac = 0; ac < cs.getLength(); ++ac )
			{
				cube[ac] = { cs[ac][0], cs[ac][1] };
			}
			renderCube = true;
			
			if( cfg.exists("resCheck") )
			{
				resCheck = cfg.lookup("resCheck");
			}
		}
		
		if( cfg.exists("matchesFile") )
		{
			matchesFile = dataRoot + testRoot + (const char*)cfg.lookup("matchesFile");
		}

		originFrame  = cfg.lookup("originFrame");
		if( cfg.exists("targetDepth" ) )
			targetDepth  = cfg.lookup("targetDepth");
	}
	catch( libconfig::SettingException &e)
	{
		cout << "Setting error: " << endl;
		cout << e.what() << endl;
		cout << e.getPath() << endl;
	}
	catch( libconfig::ParseException &e )
	{
		cout << "Parse error:" << endl;
		cout << e.what() << endl;
		cout << e.getError() << endl;
		cout << e.getFile() << endl;
		cout << e.getLine() << endl;
	}

	// create image sources
	std::vector< ImageSource* > sources;
	
	bool gotSources = false;
	if( imgDirs.size() > 0 )
	{
		for( unsigned ic = 0; ic < imgDirs.size(); ++ic )
		{
// 			cout << "creating source: " << imgDirs[ic] << endl;
// 			sources.push_back( new ImageDirectory(imgDirs[ic]));
			
			
			cout << "creating source: " << imgDirs[ic] << endl;
			ImageSource *isrc;
			if( boost::filesystem::is_directory( imgDirs[ic] ))
			{
				isrc = (ImageSource*) new ImageDirectory(imgDirs[ic]);
			}
			else
			{
				calibFiles[ic] = imgDirs[ic] + ".calib";
				isrc = (ImageSource*) new VideoSource(imgDirs[ic], calibFiles[ic]);
			}
			sources.push_back( isrc );
			
		}
		
		
		
		gotSources = true;
	}
	else if( vidFiles.size() > 0 && calibFiles.size() > 0 )
	{
		for( unsigned ic = 0; ic < vidFiles.size(); ++ic )
		{
			cout << "creating source: " << vidFiles[ic] << " with " << calibFiles[ic] << endl;
			sources.push_back( new VideoSource( vidFiles[ic], calibFiles[ic] ) );;
		}
		gotSources = true;
	}

	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		sources[isc]->JumpToFrame( originFrame );
	}
	
	if( resCheck )
	{
		ResCheck( sources, cube );
	}
	
	// work out where all the cameras are.
	cout << "original camera centres: " << endl;
	hVec3D o; o << 0, 0, 0, 1;
	vector< hVec3D > camCentres;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		const Calibration &cal = sources[isc]->GetCalibration();
		hVec3D c;
		c = cal.TransformToWorld(o);
		camCentres.push_back(c);
		cout << c.transpose() << endl;
	}
	cout << endl;
	
	cout << "original camera Ls: " << endl;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		const Calibration &cal = sources[isc]->GetCalibration();
		cout << cal.L << endl << endl;
	}
	cout << endl;
	
	
	// if we have matches, work out their 3d positions, and reprojections
	std::map< unsigned, PointMatch > matches;
	if( matchesFile.compare("none") != 0 )
	{
		LoadPointMatches( matchesFile, matches );
	}
	for( auto mi = matches.begin(); mi != matches.end(); ++mi )
	{
		PointMatch &m = mi->second;
		
		std::vector< hVec3D > centres, rays;
		for( auto ci = m.obs.begin(); ci != m.obs.end(); ++ci )
		{
			unsigned isc = srcId2Indx[ ci->first ];
			centres.push_back( camCentres[ isc ] );
			hVec3D r;
			r = sources[ isc ]->GetCalibration().Unproject( ci->second );
			rays.push_back( r );
		}
		
		m.p3d = IntersectRays( centres, rays );
		for( auto ci = srcId2Indx.begin(); ci != srcId2Indx.end(); ++ci )
		{
			unsigned isc = srcId2Indx[ ci->first ];
			m.proj[ ci->first ] = sources[ isc ]->GetCalibration().Project( m.p3d );
		}
	}
	


	// are all the images the same ratio, or should we force a square window instead?
	bool allSameRatio = true;
	cv::Mat img = sources[0]->GetCurrent();
	float ar = img.rows / (float) img.cols;
	for( unsigned c = 1; c < sources.size(); ++c )
	{
		cv::Mat img = sources[c]->GetCurrent();
		float ar2 = img.rows / (float) img.cols;
		
		if( ar2 != ar ) // probably not good enough, what with float rounding. But, feeling lazy.
			allSameRatio = false; 
	}
	
	if( !allSameRatio )
		ar = 1.0f;
	
	
	// create the renderer for display purposes.
	cout << "creating window" << endl;
	std::shared_ptr<CalCheckRenderer> ren;
	float winW = ccfg.maxSingleWindowWidth;
	float winH = winW * ar;
	if( winH > ccfg.maxSingleWindowHeight )
	{
		winH = ccfg.maxSingleWindowHeight;
		winW = winH / ar;
	}
	Rendering::RendererFactory::Create( ren, winW, winH, "Camera network alignment check");
	
	// create text maker to display text.
	std::stringstream fntss;
	fntss << ccfg.coreDataRoot << "/NotoMono-Regular.ttf";
	Rendering::SDFText textMaker(fntss.str(), ren);
	std::shared_ptr<Rendering::SceneNode> textRoot;
	Rendering::NodeFactory::Create( textRoot, "textRoot" );
	
	// create the scene.
	// we could add camera nodes for each camera, but that requires more effort on
	// my part. So instead we keep one camera node, and have a tweaked wireframe cube at each
	// camera location.
	cv::Mat nullImg(10,10, CV_8UC3, cv::Scalar(0,0,0) );
	auto nullTex = std::make_shared<Rendering::Texture>( Rendering::Texture(ren) );
	nullTex->UploadImage(nullImg);
	std::vector< std::shared_ptr<Rendering::Mesh> > camCubes(sources.size());
	std::vector< std::shared_ptr<Rendering::MeshNode> > camNodes(sources.size());
	Eigen::Vector4f magenta;
	magenta << 1.0, 0, 1.0, 1.0;
	
	bool scaleIsMM = false;
	std::vector<float> dists;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		hVec3D c = sources[isc]->GetCalibration().GetCameraCentre();
		
		dists.push_back( c.head(3).norm() );
	}
	std::sort( dists.begin(), dists.end() );
	if( dists[ dists.size()/2 ] > 200 )
		scaleIsMM = true;
	
	if( scaleIsMM )
		cout << "looks like calib is using mm" << endl;
	else
		cout << "looks like calib is using m" << endl;
	
	std::shared_ptr<Rendering::MeshNode> cubeNode;
	if( renderCube )
	{
		hVec3D zero; zero << 0,0,0,1.0f;
		auto cubeMesh = Rendering::GenerateWireCube(zero, 20);
		
		cubeMesh->vertices << cube[0][0], cube[0][0], cube[0][1], cube[0][1],    cube[0][0], cube[0][0], cube[0][1], cube[0][1],
		                      cube[1][0], cube[1][1], cube[1][1], cube[1][0],    cube[1][0], cube[1][1], cube[1][1], cube[1][0],
		                      cube[2][0], cube[2][0], cube[2][0], cube[2][0],    cube[2][1], cube[2][1], cube[2][1], cube[2][1],
		                               1,          1,          1,          1,             1,          1,          1,          1;
		
		cubeMesh->UploadToRenderer(ren);
		
		Eigen::Vector4f cubeCol;
		magenta << 0.6, 0.8, 0.6, 0.5;
		
		
		Rendering::NodeFactory::Create(cubeNode, "cubeNode" );
		cubeNode->SetMesh( cubeMesh );
		cubeNode->SetBaseColour(magenta);
		cubeNode->SetTexture(nullTex);
		cubeNode->SetShader( ren->GetShaderProg("basicColourShader"));
		
		ren->Get3dRoot()->AddChild( cubeNode );
	}
	
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		hVec3D zero; zero << 0,0,0,1.0f;
		camCubes[isc] = Rendering::GenerateWireCube(zero, 20);
		
		// we'll actually tweak the vertices slightly to more obviously show
		// which way the camera is facing.
		if( scaleIsMM )
		{
			camCubes[isc]->vertices <<  -20, -20,  20,  20,    -50,-50, 50,  50,
			                            -20,  20,  20, -20,    -50, 50, 50, -50,
			                              0,   0,   0,   0,     50, 50, 50,  50,
			                              1,   1,   1,   1,      1,  1,  1,   1;
		}
		else
		{
			camCubes[isc]->vertices <<  -0.02, -0.02,  0.02,  0.02,    -0.05,-0.05, 0.05,  0.05,
			                            -0.02,  0.02,  0.02, -0.02,    -0.05, 0.05, 0.05, -0.05,
			                              0,       0,     0,     0,     0.05, 0.05, 0.05,  0.05,
			                              1,       1,     1,     1,        1,    1,    1,     1;
		}
		camCubes[isc]->UploadToRenderer(ren);
		
		std::stringstream ss;
		ss << "nodeForCamera_" << isc;
		Rendering::NodeFactory::Create(camNodes[isc], ss.str() );
		camNodes[isc]->SetMesh( camCubes[isc] );
		camNodes[isc]->SetBaseColour(magenta);
		camNodes[isc]->SetTexture(nullTex);
		camNodes[isc]->SetTransformation( sources[isc]->GetCalibration().L.inverse() );
		camNodes[isc]->SetShader( ren->GetShaderProg("basicColourShader"));
		
		ren->Get3dRoot()->AddChild( camNodes[isc] );
	}
	
	std::shared_ptr<Rendering::SceneNode> axesNode;
	if( scaleIsMM )
		axesNode = Rendering::GenerateAxisNode3D( 500, "axesNode", ren );
	else
		axesNode = Rendering::GenerateAxisNode3D( 0.5, "axesNode", ren );
	
	ren->Get3dRoot()->AddChild(axesNode);
	
	ren->Get2dFgRoot()->AddChild( textRoot );
	
	
	Calibration calib;
	bool clicked = false;
	int camChange = 1;
	hVec2D clickPos, p2d;
	hVec3D p;
	int cam = -1;
	cv::Mat udimg;
	while(ren->Step(camChange, clicked, clickPos))
	{
		if( camChange != 0 )
		{
			cam += camChange;
			cam = std::max(0, cam);
			cam = std::min( (int)sources.size()-1, cam );
			cv::Mat img = sources[cam]->GetCurrent();
			
			ren->DeleteBGImage();
			
			//
			// When we set the calibration, we need to take into account if the window has a different aspect ratio 
			// to the images.
			//
			if( !allSameRatio )
			{
				//
				// This should allow us to handle mix of different cameras being used.
				// ar (window aspect ratio) should be 1 in this case.
				//
				calib = sources[cam]->GetCalibration();
				if( img.rows > img.cols )
				{
					ren->Get2dBgCamera()->SetOrthoProjection(0, img.rows, 0, img.rows, -10, 10);
					ren->Get2dFgCamera()->SetOrthoProjection(0, img.rows, 0, img.rows, -10, 10);
					calib.width  = img.rows;
					calib.height = img.rows;
				}
				else
				{
					ren->Get2dBgCamera()->SetOrthoProjection(0, img.cols, 0, img.cols, -10, 10);
					ren->Get2dFgCamera()->SetOrthoProjection(0, img.cols, 0, img.cols, -10, 10);
					calib.width  = img.cols;
					calib.height = img.cols;
				}
				
				ren->Get3dCamera()->SetFromCalibration(calib, 0.1, 15000);
				
			}
			else
			{
				//
				// this is easy mode and just the code that we have always had when we never had to deal
				// with rotated images.
				//
				ren->Get2dBgCamera()->SetOrthoProjection(0, img.cols, 0, img.rows, -10, 10);
				ren->Get2dFgCamera()->SetOrthoProjection(0, img.cols, 0, img.rows, -10, 10);
				calib = sources[cam]->GetCalibration();
			
				ren->Get3dCamera()->SetFromCalibration(calib, 0.1, 15000);
			}
			
			
			p2d = calib.Project(p);
			p2d = calib.UndistortPoint(p2d);
			
			cout << "cc : " << calib.GetCameraCentre().transpose() << endl;
			cout << "p : " << p.transpose() << endl;
			cout << "pd: " << p2d.transpose() << endl;
			
			udimg = calib.Undistort(img);
			cv::Point cvp( p2d(0), p2d(1) );
			cv::circle( udimg, cvp, 8, cv::Scalar(255,255,255), 3 );
			
			cout << endl << endl << endl << " === matches === " << endl;
			textRoot->Clean();
			
			for( auto mi = matches.begin(); mi != matches.end(); ++mi )
			{
				PointMatch &m = mi->second;
				cout << mi->first << ": " << endl;
				cout << "\tp3d : " << m.p3d.transpose() << endl;
				std::string sid = srcIndx2Id[ cam ];
				auto oi = m.obs.find( sid );
				auto pi = m.proj.find( sid );
				
				
				cv::Point o, p;
				hVec2D uo, up;	// need undistorted versions for display (because we undistorted the image)
				if( oi != m.obs.end() )
				{
					uo = calib.UndistortPoint( oi->second );
					o.x = uo(0);
					o.y = uo(1);
					cv::circle( udimg, o, 8, cv::Scalar(0,255,0), 3 );
					cout << "\t\tobs:" << oi->second.transpose() << endl;
					cout << "\t\tuo :" << uo.transpose() << endl;
				}
				else
				{
					cout << "\t\t" << "no obs" << endl;
				}
				
				up = calib.UndistortPoint( pi->second );
				p.x = up(0);
				p.y = up(1);
				cv::circle( udimg, p, 8, cv::Scalar(0,255,255), 3 );
				
				cout << "\t\tpi :" << pi->second.transpose() << endl;
				cout << "\t\tup :" << up.transpose() << endl;
				
				
				std::stringstream ss; ss << "point-text-" << mi->first;
				std::shared_ptr< Rendering::SceneNode > pointText;
				Rendering::NodeFactory::Create( pointText, ss.str() );
				ss.str("");
				ss << mi->first;
				up(0) += 10;
				Eigen::Vector4f mag; mag << 1.0, 0.0, 1.0, 1.0;
				textMaker.RenderString( ss.str(), 50, up, mag, pointText );
				textRoot->AddChild( pointText );
				
			}
			
			
			ren->SetBGImage( udimg );
		}
		if( clicked )
		{
			float x = clickPos(0) / (float)ren->GetWidth();
			float y = clickPos(1) / (float)ren->GetHeight();
			x = x * udimg.cols;
			y = y * udimg.rows;
			
			hVec2D p2; p2 << x, y, 1;
			
			Calibration &calib = sources[cam]->GetCalibration();
			
			// remember that we clicked on a point on the undistorted image...
			p2 = calib.DistortPoint(p2);
			
			
			hVec3D r = calib.Unproject( p2 );
			hVec3D c = calib.GetCameraCentre();
			Eigen::Vector4f plane;
			plane << 0,0,1,0;
			p = IntersectRayPlane( c, r, plane);
			p2d = calib.Project(p);
			
			// and undistort it again
			p2d = calib.UndistortPoint(p2d);
			
			cout << "p2: " << p2.transpose() << endl;
			cout << "p : " << p.transpose() << endl;
			cout << "pd: " << p2d.transpose() << endl;
			
			
			cv::Mat img = sources[cam]->GetCurrent();
			udimg = calib.Undistort(img);
			cv::Point cvp( p2d(0), p2d(1) );
			cv::circle( udimg, cvp, 8, cv::Scalar(255,255,255), 3 );
			
			
			for( auto mi = matches.begin(); mi != matches.end(); ++mi )
			{
				PointMatch &m = mi->second;
				cout << mi->first << ": " << endl;
				cout << "\tp3d : " << m.p3d.transpose() << endl;
				std::string sid = srcIndx2Id[ cam ];
				auto oi = m.obs.find( sid );
				auto pi = m.proj.find( sid );
				
				
				cv::Point o, p;
				hVec2D uo, up;	// need undistorted versions for display (because we undistorted the image)
				if( oi != m.obs.end() )
				{
					uo = calib.UndistortPoint( oi->second );
					o.x = uo(0);
					o.y = uo(1);
					cv::circle( udimg, o, 8, cv::Scalar(0,255,0), 3 );
				}
				
				up = calib.UndistortPoint( pi->second );
				p.x = up(0);
				p.y = up(1);
				cv::circle( udimg, p, 8, cv::Scalar(0,255,255), 3 );
				
			}
			
			ren->SetBGImage( udimg );
			
		}
		camChange = 0;
		clicked = false;
	}
}


void ResCheck( std::vector< ImageSource* > &sources, std::vector< std::vector<float> > &cube )
{
	genMatrix pts = genMatrix::Zero( 4, 9 );
	pts << 0.0, cube[0][0], cube[0][0], cube[0][1], cube[0][1],    cube[0][0], cube[0][0], cube[0][1], cube[0][1],
		   0.0, cube[1][0], cube[1][1], cube[1][1], cube[1][0],    cube[1][0], cube[1][1], cube[1][1], cube[1][0],
		   0.0, cube[2][0], cube[2][0], cube[2][0], cube[2][0],    cube[2][1], cube[2][1], cube[2][1], cube[2][1],
		   1.0,          1,          1,          1,          1,             1,          1,          1,          1;
	
	float omean, ocnt;
	omean = 0.0f;
	ocnt  = 0;
	for( unsigned sc = 0; sc < sources.size(); ++sc )
	{
		Calibration &calib = sources[sc]->GetCalibration();
		std::vector<float> dists;
		hVec3D o, x, y;
		o << 0,0,0,1;
		x << 1,0,0,1;
		y << 0,1,0,1;
		
		o = calib.TransformToWorld( o );
		x = calib.TransformToWorld( x );
		y = calib.TransformToWorld( y );
		
		hVec3D xd = x - o;
		hVec3D yd = y - o;
		
		hVec2D o2 = calib.Project( o );
		
		for( unsigned pc = 0; pc < pts.cols(); ++pc )
		{
			hVec3D a = pts.col(pc);
			hVec3D b = a;
			hVec2D a2, b2;
			
			float d = 0.0f;
			a2 = calib.Project(a);
			do
			{
				d += 0.1;
				b = a + xd*d;
				b2 = calib.Project(b);
			}
			while( (b2-a2).norm() < 1.0f );
			
			dists.push_back(d);
			cout << std::setw(5) << d << " ";
			d = 0.0f;
			
			do
			{
				d += 0.1;
				b = a + yd*d;
				b2 = calib.Project(b);
			}
			while( (b2-a2).norm() < 1.0f );
			
			dists.push_back(d);
			cout << std::setw(5) << d << " | ";
		}
		
		float md = 0.0f;
		for( unsigned c = 0; c < dists.size(); ++c )
		{
			md += dists[c];
			omean += dists[c];
			ocnt += 1;
		}
		md /= dists.size();
		cout << "md: " << md << endl;
	}
	cout << "omd: " << omean / ocnt << endl;
}
