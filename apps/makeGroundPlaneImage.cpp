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

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <map>
#include <iomanip>
using std::cout;
using std::endl;
using std::vector;

#include "commonConfig/commonConfig.h"

class GPRenderer : public Rendering::BasicRenderer
{
	friend class Rendering::RendererFactory;	
	// The constructor should be private or protected so that we are forced 
	// to use the factory...
protected:
	// the constructor creates the renderer with a window of the specified
 	// size, and with the specified title.
	GPRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title) {}
public:
	bool Step()
	{
		Render();

		win.setActive();
		sf::Event ev;
		while( win.pollEvent(ev) )
		{
			if (ev.type == sf::Event::Closed)
			{
				return false;
			}
		}
		return true;
	}
};


int main(int argc, char *argv[] )
{
	if( argc != 6 )
	{
		cout << "simply merges camera views together on the ground plane" << endl;
		cout << "Usage: " << endl;
		cout << argv[0] << " <calib config> <minx> <miny> <world size> <image size> " << endl;
		exit(0);
	}
	
	float minx, miny, worldSize, imgSize;
	minx = atof( argv[2] );
	miny = atof( argv[3] );
	worldSize = atof( argv[4] );
	imgSize = atof( argv[5] );
	
	CommonConfig ccfg;
	
	// parse config file
	libconfig::Config cfg;

	// the settings that we want...
	std::string dataRoot = ccfg.dataRoot;
	std::string testRoot;
	vector< std::string > imgDirs;
	vector< std::string > vidFiles;
	std::map< unsigned, std::string > calibFiles;
	std::map< std::string, unsigned > srcId2Indx;
	std::map< unsigned, std::string > srcIndx2Id;
	float targetDepth = 0;
	unsigned originFrame = 0;
	std::string matchesFile("none");
	bool animate = false;
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
		
		if( cfg.exists("matchesFile") )
		{
			matchesFile = dataRoot + testRoot + (const char*)cfg.lookup("matchesFile");
		}

		originFrame  = cfg.lookup("originFrame");
		animate = false;
		if( cfg.exists("animateGroundPlane") )
			animate      = cfg.lookup("animateGroundPlane");
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
	
	if( !animate )
	{
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			sources[isc]->JumpToFrame( originFrame );
		}
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
	
	
	cout << "creating window" << endl;
	float winW = ccfg.maxSingleWindowWidth;
	float winH = winW;
	if( winH > ccfg.maxSingleWindowHeight )
	{
		winH = ccfg.maxSingleWindowHeight;
		winW = winH;
	}
	std::shared_ptr<GPRenderer> ren;
	Rendering::RendererFactory::Create( ren, winW, winH, "Ground Plane");
	ren->Get2dBgCamera()->SetOrthoProjection(0, imgSize, 0, imgSize, -10, 10);
	
	bool done = false;
	while( !done )
	{
		cv::Mat gpImage( imgSize, imgSize, CV_32FC3, cv::Scalar(0,0,0) );
		for( unsigned rc = 0; rc < imgSize; ++rc )
		{
			for( unsigned cc = 0; cc < imgSize; ++cc )
			{
				hVec3D wp;
				wp(0) = minx + worldSize * (cc/(float)imgSize);
				wp(1) = miny + worldSize * (rc/(float)imgSize);
				wp(2) = 0.0f;
				wp(3) = 1.0f;
				
				cv::Vec3f &gp = gpImage.at<cv::Vec3f>(rc,cc);
				float count = 0;
				for( unsigned isc = 0; isc < sources.size(); ++isc )
				{
					hVec2D ip;
					if( sources[isc]->GetCalibration().Project(wp,ip) )
					{
						cv::Mat img = sources[isc]->GetCurrent();
						if( ip(0) > 0 && ip(1) > 0 && ip(1) < img.rows && ip(0) < img.cols )
						{
							gp[0] += img.at< cv::Vec3b >( ip(1), ip(0) )[0] / 255.0f;
							gp[1] += img.at< cv::Vec3b >( ip(1), ip(0) )[1] / 255.0f;
							gp[2] += img.at< cv::Vec3b >( ip(1), ip(0) )[2] / 255.0f;
							count += 1;
						}
					}
				}
				
				gp /= count;
				
			}
			ren->SetBGImage( gpImage );
			ren->Step();
		}
		
		
		if( !animate )
		{
			done = true;
			gpImage *= 255;
			SaveImage( gpImage, "groundPlane.png" );
		}
		if( animate )
		{
			std::stringstream ss;
			ss << "groundPlane-" << std::setw(8) << std::setfill('0') << sources[0]->GetCurrentFrameID() << ".charImg";
			gpImage *= 255;
			SaveImage( gpImage, ss.str() );
			for( unsigned isc = 0; isc < sources.size(); ++isc )
			{
				done = done | !sources[isc]->Advance();
			}
		}
	}
	
	while(ren->Step());
}
