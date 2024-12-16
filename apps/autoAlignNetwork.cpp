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
#include "math/matrixGenerators.h"
#include "math/distances.h"


#include "libconfig.h++"

#include "commonConfig/commonConfig.h"


#include "SDS/opt.h"

enum method_t {VIEW_CENT_RING};

#define MODE_CALIB_ONLY 0
#define MODE_SOURCES    1


struct Settings
{
	std::string dataRoot;
	std::string testRoot;
	vector< std::string > imgDirs;
	vector< std::string > vidFiles;
	std::vector< std::string > calibFiles;
	
	method_t alignMethod;
	int highCamera;
};




void CreateSources( Settings &s, std::vector< ImageSource* > &sources );
Settings ParseConfig( std::string cfn );
void AlignRingViewCent( std::vector< Calibration > &calibs, int highCamera );


int main(int argc, char *argv[] )
{
	if( argc != 2 && argc < 4)
	{
		cout << "Tool to automatically realign/recentre a set of calibrated cameras"                  << endl;
		cout << "Assumes cameras are in a ring or half dome and looking at a \"centre\"."             << endl;
		cout << "Usage: "                                                                             << endl;
		cout << argv[0] << " < calibration config file > "                                            << endl;
		cout << endl;
		cout << "If we don't have a config file, just a set of image directory sources, then we have" << endl;
		cout << "this option, giving the calibrations on the command line."                           << endl;
		cout << "This version wont actually load the images either."                                  << endl;
		cout << endl;
		cout << " <method> : one of viewCentRing or... no other options right now ;p"                 << endl;
		cout << " <high calib>  : the calib file for the camera that should be highest"               << endl;
		cout << "                 after alignment."                                                   << endl;
		cout << " <all calibs>  : all calib files _including_ the high calib."                        << endl;
		cout << endl;
		cout << argv[0] << " < method > < high calib > [ all calibs ]"                                << endl;
		cout << endl;
		exit(1);
	}
	
	cout << "Loading..." << endl;
	Settings settings;
	std::vector< Calibration > calibs;
	std::vector< ImageSource* > sources;
	int mode;
	if( argc == 2 )
	{
		settings = ParseConfig( argv[1] );
		CreateSources( settings, sources );
		for( unsigned c = 0; c < sources.size(); ++c )
		{
			calibs[c] = sources[c]->GetCalibration();
		}
		mode = MODE_SOURCES;
	}
	else if( argc >= 4 )
	{
		std::string methodStr( argv[1] );
		if( methodStr.compare("viewCentRing") == 0 )
		{
			settings.alignMethod = VIEW_CENT_RING;
		}
		else
		{
			throw std::runtime_error( "Don't know the alignment method." );
		}
		
		std::string highCamStr( argv[2] );
		for( unsigned c = 3; c < argc; ++c )
		{
			settings.calibFiles.push_back( argv[c] );
			cout << c << " : " << argv[c];
			
			
			if( highCamStr.compare( argv[c] ) == 0 )
			{
				settings.highCamera = settings.calibFiles.size()-1;
				cout << " ###### ";
			}
			
			cout << endl;
			
		}
		
		calibs.resize( settings.calibFiles.size() );
		for( unsigned c = 0; c < settings.calibFiles.size(); ++c )
		{
			cout << c << "/" << settings.calibFiles.size() << " : " << settings.calibFiles[c] << endl;
			calibs[c].Read( settings.calibFiles[c] );
		}
		
		mode = MODE_CALIB_ONLY;
	}
	
	
	if( settings.alignMethod == VIEW_CENT_RING )
	{
		cout << "view cent ring align." << endl;
		cout << "  - phase 1..." << endl;
		AlignRingViewCent( calibs, settings.highCamera );
		cout << "  - phase 2..." << endl;
		AlignRingViewCent( calibs, settings.highCamera );
		cout << "done" << endl;
	}
	else
	{
		cout << "Couldn't do anything, you should have had an error before now." << endl;
		exit(2);
	}
	
	cout << "saving" << endl;
	
	if( mode == MODE_CALIB_ONLY )
	{
		for( unsigned c = 0; c < calibs.size(); ++c )
		{
			calibs[c].Write( settings.calibFiles[c] );
		}
	}
	else if( mode == MODE_SOURCES )
	{
		for( unsigned sc = 0; sc < sources.size(); ++sc )
		{
			sources[sc]->GetCalibration() = calibs[sc];
			sources[sc]->SaveCalibration();
		}
	}
	
	
}





Settings ParseConfig( std::string cfn )
{
	// parse config file
	libconfig::Config cfg;

	CommonConfig ccfg;
	
	Settings s;


	// the settings that we want...
	s.dataRoot = ccfg.dataRoot;
	try
	{
		cfg.readFile(cfn.c_str());
		
		if( cfg.exists("dataRoot") )
			s.dataRoot = (const char*)cfg.lookup("dataRoot");
		s.testRoot = (const char*)cfg.lookup("testRoot");
		
		if( cfg.exists("imgDirs") )
		{
			libconfig::Setting &idirs = cfg.lookup("imgDirs");
			for( unsigned ic = 0; ic < idirs.getLength(); ++ic )
			{
				std::string str;
				str = s.dataRoot + s.testRoot + (const char*) idirs[ic];
				s.imgDirs.push_back(str);
			}
		}
		else if( cfg.exists("vidFiles") && cfg.exists("calibFiles") )
		{
			libconfig::Setting &vfs = cfg.lookup("vidFiles");
			for( unsigned ic = 0; ic < vfs.getLength(); ++ic )
			{
				std::string str;
				str = s.dataRoot + s.testRoot + (const char*) vfs[ic];
				s.vidFiles.push_back(str);
			}
			
			libconfig::Setting &cfs = cfg.lookup("calibFiles");
			for( unsigned ic = 0; ic < cfs.getLength(); ++ic )
			{
				std::string str;
				str = s.dataRoot + s.testRoot + (const char*) cfs[ic];
				s.calibFiles[ic] = str;
			}
		}
		
		std::string methodStr = (const char*)cfg.lookup("alignMethod");
		if( methodStr.compare("viewCentRing") == 0 )
		{
			s.alignMethod = VIEW_CENT_RING;
		}
		else
		{
			throw std::runtime_error( "Don't know the alignment method." );
		}
		
		std::string highCamStr = s.dataRoot + s.testRoot + (const char*)cfg.lookup("highCamHint");
		int hcc = 0;
		while( hcc < s.imgDirs.size() && s.imgDirs[hcc].compare( highCamStr ) != 0 )
			++hcc;
		
		if( hcc >= s.imgDirs.size() )
		{
			throw std::runtime_error("high camera not found in specified sources");
		}
		s.highCamera = hcc;
	}
	catch( libconfig::SettingException &e)
	{
		cout << "Setting error: " << endl;
		cout << e.what() << endl;
		cout << e.getPath() << endl;
		exit(0);
	}
	catch( libconfig::ParseException &e )
	{
		cout << "Parse error:" << endl;
		cout << e.what() << endl;
		cout << e.getError() << endl;
		cout << e.getFile() << endl;
		cout << e.getLine() << endl;
		exit(1);
	}
	
	return s;
}




void CreateSources( Settings &s, std::vector< ImageSource* > &sources )
{
	bool gotSources = false;
	if( s.imgDirs.size() > 0 )
	{
		for( unsigned ic = 0; ic < s.imgDirs.size(); ++ic )
		{
			cout << "creating source: " << s.imgDirs[ic] << endl;
			ImageSource *isrc;
			if( boost::filesystem::is_directory( s.imgDirs[ic] ))
			{
				isrc = (ImageSource*) new ImageDirectory(s.imgDirs[ic]);
			}
			else
			{
				s.calibFiles[ic] = s.imgDirs[ic] + ".calib";
				isrc = (ImageSource*) new VideoSource(s.imgDirs[ic], s.calibFiles[ic]);
			}
			sources.push_back( isrc );
		}
	}
	else if( s.vidFiles.size() > 0 && s.calibFiles.size() > 0 )
	{
		for( unsigned ic = 0; ic < s.vidFiles.size(); ++ic )
		{
			cout << "creating source: " << s.vidFiles[ic] << " with " << s.calibFiles[ic] << endl;
			sources.push_back( new VideoSource( s.vidFiles[ic], s.calibFiles[ic] ) );;
		}
		gotSources = true;
	}
	

	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		sources[isc]->JumpToFrame( 0 );
	}
}










class RingAgent: public SDS::Agent
{
public:
	RingAgent( std::vector< hVec3D > inCentres, hVec3D inOrigin ) : centres( inCentres), origin( inOrigin )
	{
		dists.resize( centres.size() );
	}

	std::vector< hVec3D > centres;
	std::vector< float > dists;
	hVec3D origin;
	
	double var, n;

	virtual double EvaluatePosition()
	{
		//
		// TODO: If the cameras are in a dome more than a ring, then
		//       we probably need to pre-group the cameras based on "height" 
		//       along the ray first.
		//
		error = 0.0f;

		hVec3D rayDir0; rayDir0 << position[0], position[1], position[2], 0.0;
		n = rayDir0.norm();
		hVec3D rayDir;
		if( rayDir0.norm() > 0.1 )
			rayDir  = rayDir0 / rayDir0.norm();
		else
		{
			error = 9999999;
			return error;
		}

		float mean = 0.0f;
		for( unsigned c = 0; c < dists.size(); ++c )
		{
			dists[c] = PointRayDistance3D( centres[c], origin, rayDir );
			mean += dists[c];
		}
		mean /= dists.size();
		
		var = 0.0f;
		for( unsigned c = 0; c < dists.size(); ++c )
		{
			dists[c] = pow(dists[c] - mean, 2);
			var += dists[c];
		}
		var /= dists.size();
		
		error = var + std::abs( 1.0 - n );
		
		return error;
	}
};



void AlignRingViewCent( std::vector< Calibration > &calibs, int highCamera )
{
	
	//
	// Get camera centres and their view direction rays.
	//

	cout << "Camera centres and view dirs: " << endl;
	std::vector< hVec3D > centres( calibs.size() ), viewDirs( calibs.size() );
	hVec3D zdir(4); zdir << 0,0,1,0;
	for( unsigned sc = 0; sc < calibs.size(); ++sc )
	{
		centres[sc]  = calibs[sc].GetCameraCentre();
		viewDirs[sc] = calibs[sc].TransformToWorld( zdir );
		cout << "\t - " << sc << ": " << centres[sc].transpose() << " : " << viewDirs[sc].transpose() << endl;
	}
	cout << endl;

	
	
	//
	// Intersect those view rays, that will give us the new origin,
	// so our first transformation is just a translation to make it the new origin.
	//
	hVec3D origin = IntersectRays( centres, viewDirs );
	cout << "New centre: " << origin.transpose() << endl;
	
	transMatrix3D T;
	T = transMatrix3D::Identity();
	T.block(0,3,3,1) = origin.head(3);

	
	//
	// The cameras are in a ring, looking inwards towards our new origin,
	// give or take. We want to make the z-axis go straight through the 
	// middle of that ring as our up/down axis.
	//
	// At any given height along the ray, we should find that the cameras
	// all have the same distance to the ray - which is to say, we minimise 
	// the variance of the distance of the cameras from the ray.
	//
	// I'm just going to use my not-quite SDS for this.
	//
	SDS::Optimiser sdsopt;
	std::vector< RingAgent* > ragents;
	ragents.assign( 100, NULL );
	std::vector< SDS::Agent* > agents( ragents.size() );
	for( unsigned ac = 0; ac < agents.size(); ++ac )
	{
		ragents[ac] = new RingAgent(centres, origin);
		agents[ac] = ragents[ac];
	}

	// initialise the search.
	int bestAgent;
	std::vector<double> initPos(3), initRanges(3);
	initPos = {0,0,1};
	initRanges = {1.0, 1.0, 1.0};
	sdsopt.InitialiseOpt(initPos, initRanges, agents, 0.001, 5000);

	// loop until we have a solution.
	int ic = 0;
	do
	{
		bestAgent = sdsopt.StepOpt();
		hVec3D r; r << ragents[bestAgent]->position[0], ragents[bestAgent]->position[1], ragents[bestAgent]->position[2], 0.0f;
		
		cout << ragents[ bestAgent ]->error << " : " << r.transpose() << " : " << ragents[bestAgent]->var << " : " << ragents[bestAgent]->n << endl;
	}
	while( !sdsopt.CheckTerm() );
	
	hVec3D rayDir; rayDir << ragents[bestAgent]->position[0], ragents[bestAgent]->position[1], ragents[bestAgent]->position[2], 0.0f;
	rayDir /= rayDir.norm();
	

	//
	// The next thing we need to do is find a rotation that takes that ray direction to be (0,0,1,0)
	//
	hVec3D target; target << 0,0,1,0;
	hVec3D axis = Cross( rayDir, target );
	float angle = axis.norm();
	axis = axis / angle;
	Eigen::Vector3f ax3 = axis.head(3);
	
	transMatrix3D R = RotMatrix( ax3, angle );
	
	
	//
	// The only thing we don't know now is what way is "up". 
	// So... first of all, just apply this to all the cameras.
	//
	transMatrix3D M = T * R.inverse();
	
	for( unsigned sc = 0; sc < calibs.size(); ++sc )
	{
		Calibration &c = calibs[sc];
		c.L = c.L * M;
	}
	
	//
	// Now find the new centres, and find out the mean of them.
	//
	hVec3D meanCent; meanCent << 0,0,0,0;
	for( unsigned sc = 0; sc < calibs.size(); ++sc )
	{
		centres[sc]  = calibs[sc].GetCameraCentre();
		meanCent += centres[sc];
	}
	meanCent /= meanCent(3);
	
	if( centres[ highCamera ](2) < meanCent(2) )
	{
		Eigen::Vector3f ax; ax << 0,1,0;
		transMatrix3D R = RotMatrix( ax, 3.1415 );
		for( unsigned sc = 0; sc < calibs.size(); ++sc )
		{
			Calibration &c = calibs[sc];
			c.L = c.L * R;
		}
	}
	
}
