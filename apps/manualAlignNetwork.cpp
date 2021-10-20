#include "cv.hpp"
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

#include "libconfig.h++"

#include "commonConfig/commonConfig.h"


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

struct Settings
{
	std::string dataRoot;
	std::string testRoot;
	vector< std::string > imgDirs;
	vector< std::string > vidFiles;
	std::map< unsigned, std::string > calibFiles;
	std::map< std::string, unsigned > srcId2Indx;
	std::map< unsigned, std::string > srcIndx2Id;
	std::string matchesFile;
	float targetDepth = 0;
	unsigned originFrame = 0;
	bool negateXAxis = false;	// sometimes we may find that it is easier to click on a negative x or y point than a positive x or y point.
	bool negateYAxis = false;
	std::vector<unsigned> matchesOnGround;
	std::vector<unsigned> axisPoints;
};



Settings ParseConfig( std::string cfn )
{
	// parse config file
	libconfig::Config cfg;

	CommonConfig ccfg;
	
	Settings s;
	// the settings that we want...
	s.dataRoot = ccfg.dataRoot;
	s.matchesFile = "none";
	s.targetDepth = 0;
	s.originFrame = 0;
	s.negateXAxis = false;	// sometimes we may find that it is easier to click on a negative x or y point than a positive x or y point.
	s.negateYAxis = false;
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
				s.srcId2Indx[(const char*)idirs[ic]] = s.imgDirs.size();
				s.srcIndx2Id[ s.imgDirs.size() ] = (const char*)idirs[ic];
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
				
				s.srcId2Indx[(const char*)vfs[ic]] = s.vidFiles.size();
				s.srcIndx2Id[ s.vidFiles.size() ] = (const char*)vfs[ic];
			}
			
			libconfig::Setting &cfs = cfg.lookup("calibFiles");
			for( unsigned ic = 0; ic < cfs.getLength(); ++ic )
			{
				std::string str;
				str = s.dataRoot + s.testRoot + (const char*) cfs[ic];
				s.calibFiles[ic] = str;
			}
		}

		s.originFrame  = cfg.lookup("originFrame");
		s.targetDepth  = cfg.lookup("targetDepth");
		
		if( cfg.exists("alignXisNegative") )
		{
			s.negateXAxis = cfg.lookup("alignXisNegative");
		}
		if( cfg.exists("alignYisNegative") )
		{
			s.negateYAxis = cfg.lookup("alignYisNegative");
		}
		
		if( cfg.exists("matchesFile") )
		{
			s.matchesFile = s.dataRoot + s.testRoot + (const char*)cfg.lookup("matchesFile");
		}
		
		if( cfg.exists("matchesOnGround") )
		{
			libconfig::Setting &mog = cfg.lookup("matchesOnGround");
			for( unsigned mc = 0; mc < mog.getLength(); ++mc )
			{
				s.matchesOnGround.push_back( mog[mc] );
			}
		}
		
		if( cfg.exists("axisPoints") )
		{
			libconfig::Setting &ap = cfg.lookup("axisPoints");
			for( unsigned mc = 0; mc < ap.getLength(); ++mc )
			{
				s.axisPoints.push_back( ap[mc] );
			}
		}
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
		sources[isc]->JumpToFrame( s.originFrame );
	}
}

struct AxisPoints
{
	PointMatch x,o,y;
};
AxisPoints GetAxisObservationsInteractive( Settings &s, std::vector< ImageSource* > &sources)
{
	cout << "Interactive setting of axis keypoints is deprecated." << endl;
	cout << "Please use the pointMatcher programme to specify relevant image points" << endl;
	cout << "and indicate which point IDs correspond to the axis points in the config file." << endl;
	throw std::runtime_error("deprecated code path");
}


void GetPointMatch3D( Settings &s, std::vector< ImageSource* > &sources, PointMatch &m )
{
	vector< hVec3D > rays, starts;
	
	for( auto obsi = m.obs.begin(); obsi != m.obs.end(); ++obsi )
	{
		unsigned isc = s.srcId2Indx[ obsi->first ];
		cout << isc << " " << sources.size() << " " << std::endl;
		const Calibration &cal = sources[isc]->GetCalibration();
		
		hVec3D r;
		r = cal.Unproject( obsi->second );
		
		hVec3D p, camCent;
		p << 0,0,0,1;
		camCent = cal.TransformToWorld(p);
		
		
		rays.push_back(r);
		starts.push_back( camCent );
	}
	
	assert( rays.size() >= 2 );
	m.p3d = IntersectRays( starts, rays );
	for( auto obsi = m.obs.begin(); obsi != m.obs.end(); ++obsi )
	{
		unsigned isc = s.srcId2Indx[ obsi->first ];
		const Calibration &cal = sources[isc]->GetCalibration();
		
		m.proj[ obsi->first ] = cal.Project( m.p3d );
	}
	
}

void GetAxisPoints3D( Settings &s, std::vector< ImageSource* > &sources, AxisPoints &axisPoints )
{
	// target points has 3 image points for each camera view that could see them.
	// these 3 points should be in the order x-axis point, origin, y-axis point.
	// First thing we need are each of those points in 3D.
	GetPointMatch3D( s, sources, axisPoints.x );
	GetPointMatch3D( s, sources, axisPoints.o );
	GetPointMatch3D( s, sources, axisPoints.y );
}




transMatrix3D ComputeTransformation( Settings &s, AxisPoints axisPoints )
{
	hVec3D x3,o3,y3;
	x3 = axisPoints.x.p3d;
	o3 = axisPoints.o.p3d;
	y3 = axisPoints.y.p3d;
	
	// 1) what is the translation that moves o to the origin?
	//    well that's pretty obvious...
	transMatrix3D T;
	T = transMatrix3D::Identity();
	T(0,3) = -o3(0);
	T(1,3) = -o3(1);
	T(2,3) = -o3(2);

	
	
	// 2) what is the rotation that makes x3-x0 == [1,0,0,0]?
	//    (note, it may be [-1,0,0,0] if the config file said we had to click on a point on the -ve x axis)

	// the rotation axis is the cross product...
	hVec3D x0; 
	if( s.negateXAxis )
		x0 << -1,0,0,0;
	else
		x0 <<   1,0,0,0;
	
	hVec3D x3mo3 = x3-o3;
	x3mo3 /= x3mo3.norm();
	
	hVec3D raxis = Cross(x3mo3, x0);
	raxis /= raxis.norm();


	// and the angle is the dot product...
	float ang    = acos( (x3mo3).dot(x0) );

	transMatrix3D Rx0 = transMatrix3D::Identity();
	Rx0.block(0,0,3,3) = Eigen::AngleAxisf(ang, raxis.head(3)).matrix();

	transMatrix3D M = Rx0*T;

	
	
	// 3) what is the rotation that ensures y3, o3 and x3
	//    form the z=0 plane with y3 on the positive side at
	//    at approximately y3 ~ [0,1,0,1]?

	// easiest for my brain if o3 actually is the origin...
	hVec3D xc = M*x3;
	hVec3D yc = M*y3;
	hVec3D oc = M*o3;

	// we should now have oc = 0,0,0,1;
	//                    xc = ?,0,0,1;  (or xc = -?,0,0,1)
	cout << "Should be: (x,0,0,1) or (-x,0,0,1)  and (0,0,0,1) and (something) : " << endl;
	cout << xc.transpose() << endl;
	cout << oc.transpose() << endl;
	cout << yc.transpose() << endl;

	cout << "----------" << endl << endl << endl;

	
	// yc however is the last fix.
	hVec3D ycmoc = yc-oc;

	// we only want to rotate around the x-axis,
	// such that ycmoc has no z component, and its y component
	// is +ve (or -ve if requested in config)
	// how much rotation does that mean we need?
	// the bearing of the current y line is easy enough to get...
	float bearing = atan2( ycmoc(2), ycmoc(1) );
	ang = 0;
	if( s.negateYAxis )
	{
		// we want to get to a bearing of 180, which means we need to rotate by pi - bearing.... right?
		ang = 3.14159 - bearing;
	}
	else
	{
		// simply reverse ang because we just want to go to a bearing of 0 degrees.
		ang = -bearing;
	}
	
	transMatrix3D Rz0 = transMatrix3D::Identity();
	Rz0.block(0,0,3,3) = Eigen::AngleAxisf(ang, Eigen::Vector3f::UnitX()).matrix();

	cout << "Rz0:" << endl;
	cout << Rz0 << endl;
	cout << "(ang = " << ang << " )" << endl;

	M = Rz0 * M;
	
	cout << "Should be: (x,0,0,1) or (-x,0,0,1)  and (0,0,0,1) and (0,y,0,1) : " << endl;
	cout << (M*x3).transpose() << endl;
	cout << (M*o3).transpose() << endl;
	cout << (M*y3).transpose() << endl << endl << endl;
	

	// the very last thing is to account for the
	// depth of the calibration target,
	// which we assume means a simple translation on z.
	transMatrix3D T2;
	T2 = transMatrix3D::Identity();
	T2(2,3) = s.targetDepth;

	M = T2 * M;

	cout << "fin (modified for target depth)?" << endl;
	cout << (M*x3).transpose() << endl;
	cout << (M*o3).transpose() << endl;
	cout << (M*y3).transpose() << endl << endl << endl;
	
	return M;
	
	
}




int main(int argc, char *argv[] )
{
	if( argc != 2 )
	{
		cout << "Usage: " << endl;
		cout << argv[0] << " < calibration config file > " << endl;
		exit(1);
	}
	
	Settings settings = ParseConfig( argv[1] );
	
	std::vector< ImageSource* > sources;
	CreateSources( settings, sources );
	
	std::map< unsigned, PointMatch > matches;
	if( settings.matchesFile.compare("none") != 0)
	{
		LoadPointMatches( settings.matchesFile, matches );
	}
	
	AxisPoints axisPoints;
	if( settings.axisPoints.size() != 3 )
	{
		axisPoints = GetAxisObservationsInteractive( settings, sources );
	}
	else
	{
		cout << "Using stated known points to form world axes..." << endl;
		
		auto xi = matches.find( settings.axisPoints[0] );
		auto oi = matches.find( settings.axisPoints[1] );
		auto yi = matches.find( settings.axisPoints[2] );
		
		if( xi != matches.end() && oi != matches.end() && yi != matches.end() )
		{
			axisPoints.x = xi->second;
			axisPoints.y = yi->second;
			axisPoints.o = oi->second;
		}
	}
	
	
	GetAxisPoints3D( settings, sources, axisPoints );
	
	
	
	cout << "original camera centres: " << endl;
	hVec3D o; o << 0, 0, 0, 1;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		const Calibration &cal = sources[isc]->GetCalibration();
		hVec3D c;
		c = cal.TransformToWorld(o);
		cout << "[" << c.transpose() << "]" << endl;
	}
	cout << endl;

	cout << "initial target points:" << endl;
	cout << axisPoints.x.p3d.transpose() << endl;
	cout << axisPoints.o.p3d.transpose() << endl;
	cout << axisPoints.y.p3d.transpose() << endl;
	cout << endl;

	cout << " target point observations and reprojections: " << endl;
	for( auto obsi = axisPoints.x.obs.begin(); obsi != axisPoints.x.obs.end(); ++obsi )
	{
		cout << "Camera id: " << obsi->first << endl;
		cout << obsi->second.transpose() << "     -- --      " << axisPoints.x.proj[ obsi->first ].transpose() << endl;
		cout << axisPoints.o.obs[ obsi->first ].transpose() << "     -- --      " << axisPoints.o.proj[ obsi->first ].transpose() << endl;
		cout << axisPoints.y.obs[ obsi->first ].transpose() << "     -- --      " << axisPoints.y.proj[ obsi->first ].transpose() << endl;
		cout << " ----- " << endl;
	}
	
	
	
	transMatrix3D M = ComputeTransformation( settings, axisPoints );
	
	
	// we can now apply that to all of the calibrations.
	vector< transMatrix3D > nLs;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		transMatrix3D nL = sources[isc]->GetCalibration().L * M.inverse() ;
		
		sources[isc]->GetCalibration().L = nL;
		sources[isc]->SaveCalibration();

		nLs.push_back(nL);
	}
	
	
	
	
	
	
	
	// now we can use points on the ground to make sure our ground is level.
	std::map< unsigned, PointMatch > mog;
	if( settings.matchesFile.compare("none") != 0 && settings.matchesOnGround.size() > 0)
	{
		
		
		cout << "=========== Using matches on ground ===========" << endl << endl << endl;
		
		
		for( unsigned mogc = 0; mogc < settings.matchesOnGround.size(); ++mogc )
		{
			mog[ settings.matchesOnGround[mogc] ] = matches[ settings.matchesOnGround[mogc] ];
		}
		
		// get 3D point for all matches.
		cout << "Initial position of ground points: " << endl;
		for( auto mogi = mog.begin(); mogi != mog.end(); ++mogi )
		{
			GetPointMatch3D( settings, sources, mogi->second );
			
			cout << "\t" << mogi->first << " : " << mogi->second.p3d.transpose() << endl;
		}
		
		
		cout << endl << endl << endl;
		cout << "camera centres before ground plane helper: " << endl;
		o << 0, 0, 0, 1;
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
		
		
		
		// we want to work out a plane through the ground points.
		// We do that through an SVD, so assemble all the points into one matrix,
		// and also, get the mean of the points, and subtract it from the matrix.
		genMatrix P = genMatrix::Zero( 3, mog.size() );
		hVec3D mean; mean << 0,0,0,0;
		unsigned cc = 0;
		for( auto mogi = mog.begin(); mogi != mog.end(); ++mogi )
		{
			P.block( 0, cc, 3, 1 ) = mogi->second.p3d.head(3);
			mean += mogi->second.p3d;
			++cc;
		}
		mean /= mean(3);
		
		cout << P.transpose() << endl << endl;
		cout << mean.transpose() << endl;
		
		for( unsigned cc = 0; cc < P.cols(); ++cc )
		{
			P.col(cc) -= mean.head(3);
		}
		
		Eigen::JacobiSVD<Eigen::MatrixXf> svd(P, Eigen::ComputeFullU | Eigen::ComputeFullV);
		
		
		// the normal of the plane we're interested in is the last column of U
		// and we want to make sure it points up
		cout << svd.matrixU() << endl;
		hVec3D norm; norm.head(3) = svd.matrixU().col(2);
		if( norm(2) < 0 )
			norm *= -1.0f;
		
		
		// there are two transformations that we need to make sure this plane ends up being
		// out z=0 plane.
		
		// The first is a translation, which comes from the mean of the points. However, that 
		// mean will take us away from the origin - the only part of the mean that really interests us
		// is the height of the mean, i.e. the z component.
		transMatrix3D T = transMatrix3D::Identity();
		T(2,3) = -mean(2);
		
		// The second things we want is a rotation that makes the plane normal (0,1,0) - but we must make
		// sure that the rotation we use does not rotate around the z-axis at all.
		hVec3D idealNorm;
		idealNorm << 0,0,1,0;
		
		hVec3D rax = Cross( idealNorm, norm );
		
		// wonderful.
		// the magnitude of rax is the sine of the rotation angle.
		float rang = asin( rax.norm() );
		rax = rax / rax.norm();
		
		transMatrix3D R = RotMatrix( rax, -rang );
		
		
		
		// so, we can see what happens when we apply this to each of the ground points...
		cout << "Updated position of ground points: " << endl;
		for( auto mogi = mog.begin(); mogi != mog.end(); ++mogi )
		{
			hVec3D p = R * T * mogi->second.p3d;
			cout << "\t" << mogi->first << " : " << p.transpose() << endl;
		}
		
		
		// and we can now update the calibrations too.
		transMatrix3D M2 = R * T;
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			transMatrix3D nL = sources[isc]->GetCalibration().L * M.inverse() ;
			
			sources[isc]->GetCalibration().L = nL;
			sources[isc]->SaveCalibration();
		}
	}
	
	DebugOutput(nLs);
}

void DebugOutput(vector< transMatrix3D > Ls)
{
	// get location of all _set_ cameras
	vector< hVec3D > centres;

	cout << "centres: " << endl;
	for( unsigned cc = 0; cc < Ls.size(); ++cc )
	{

		hVec3D o,c ;
		o << 0,0,0,1;
		c = Ls[cc].inverse() * o;
		centres.push_back( c );

		cout << cc << ": " << c.transpose() << endl;

	}

	std::ofstream outfi("debug.py");
	outfi << "cxs = [";
	for( unsigned cc = 0; cc < centres.size(); ++cc )
	{
		outfi << centres[cc](0) << ", ";
	}
	outfi << " ] " << endl;
	outfi << "cys = [";
	for( unsigned cc = 0; cc < centres.size(); ++cc )
	{
		outfi << centres[cc](1) << ", ";
	}
	outfi << " ] " << endl;
	outfi << "czs = [";
	for( unsigned cc = 0; cc < centres.size(); ++cc )
	{
		outfi << centres[cc](2) << ", ";
	}
	outfi << " ] " << endl;



	outfi << "import plotly" << endl;
	outfi << "import plotly.graph_objs as go" << endl;
	outfi << "cctrace = go.Scatter3d(x=cxs,y=cys,z=czs)" << endl;
	outfi << "data = [cctrace]" << endl;
	outfi << "fig = go.Figure(data=data)" << endl;
	outfi << "plotly.offline.plot(fig)" << endl;
	outfi.close();

}
