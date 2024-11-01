#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "imgio/loadsave.h"
#include "math/mathTypes.h"

#include "misc/tokeniser.h"

#include "math/distances.h"
#include "math/intersections.h"
#include "misc/aggCluster.h"

#include <iostream>
using std::cout;
using std::endl;
#include <sstream>

struct Match
{
	std::string s0;
	std::string s1;
	int kp0;
	int kp1;
	float error;
	
	bool operator<( const Match &oth) const
	{
		return error < oth.error;
	}
};

void ReadData( std::string fn, std::map< std::string, std::vector< hVec2D > > &kp, std::vector< Match > &matches );

void GetMatchErrors( std::map< std::string, Calibration > calibs, std::map< std::string, std::vector< hVec2D > > &kps, std::vector< Match > &matches );

genMatrix GetPoints( std::map< std::string, std::shared_ptr< ImageDirectory > > &sources, std::map< std::string, Calibration > calibs, std::map< std::string, std::vector< hVec2D > > &kps, std::vector< Match > &matches, float maxErr );

int main( int argc, char *argv[] )
{
	if( argc != 4 )
	{
		cout << "Tool to triangulate if colmap fucks up using pre-calibrated cameras." << endl;
		cout << "expect input of matches.unfiltered from colmapMatchSources.py script" << endl;
		cout << argv[0] << " <root path to file> <frame number> <max match err>" << endl;
		exit(0);
	}
	
	std::string root( argv[1] );
	std::stringstream fn; fn << root << "matches.unfiltered";
	int fc = atoi( argv[2] );
	float maxErr = atof( argv[3] );
	
	std::map< std::string, std::vector< hVec2D > > keypoints;
	std::vector< Match > matches;
	ReadData( fn.str(), keypoints, matches );
	
	
	std::map< std::string, std::shared_ptr< ImageDirectory > > sources;
	std::map< std::string, Calibration > calibs;
	for( auto sni = keypoints.begin(); sni != keypoints.end(); ++sni )
	{
		std::stringstream fn;
		fn << root << "/" << sni->first;
		cout << fn.str() << endl;
		sources[ sni->first ].reset( new ImageDirectory( fn.str() ) );
		calibs[ sni->first ] = sources[ sni->first ]->GetCalibration();

		sources[ sni->first ]->JumpToFrame( fc );
	}
	
	
	GetMatchErrors( calibs, keypoints, matches );
	for( unsigned mc = 0; mc < 20; ++mc )
	{
		cout << mc << " " <<  matches[mc].error << endl;
	}
	cout << "       . - . - . - .        "  << endl;
	for( unsigned mc = matches.size()-21; mc < matches.size(); ++mc )
	{
		cout << mc << " " << matches[mc].error << endl;
	}
		
	genMatrix p3d = GetPoints( sources, calibs, keypoints, matches, maxErr );
	
	
	std::stringstream ss;
	ss << "cmap/frame-" << std::setw(4) << std::setfill('0') << fc << "/points3D.ply";
	
	std::ofstream outfi( ss.str() );
	
	outfi << "ply" << endl;
	outfi << "format ascii 1.0" << endl;
	outfi << "element vertex " << p3d.cols() << endl;
	outfi << "property float x" << endl;
	outfi << "property float y" << endl;
	outfi << "property float z" << endl;
	outfi << "property uint8 red" << endl;
	outfi << "property uint8 green" << endl;
	outfi << "property uint8 blue" << endl;
	outfi << "end_header" << endl;
	
	for( unsigned pc = 0; pc < p3d.cols(); ++pc )
	{
		outfi << p3d(0,pc) << " " << p3d(1,pc) << " " << p3d(2,pc) << " " <<  
		         p3d(6,pc) << " " << p3d(5,pc) << " " << p3d(4,pc) << endl;
	}
	outfi.close();
	
	
	
	
}


void ReadData( std::string fn, std::map< std::string, std::vector< hVec2D > > &kps, std::vector< Match > &matches )
{
	std::ifstream infi( fn );
	
	std::string s;
	std::getline( infi, s );
	auto sourceNames = SplitLine(s, " ");
	
	for( unsigned snc = 0; snc < sourceNames.size(); ++snc )
	{
		std::string sn;
		infi >> sn;
		
		int nkp;
		infi >> nkp;
		
		kps[ sn ].resize( nkp );
		for( unsigned kpc = 0; kpc < nkp; ++kpc )
		{
			float x, y;
			infi >> x;
			infi >> y;
			kps[ sn ][ kpc ] << x, y, 1.0;
		}
		
		cout << sn << " " << kps[ sn ].size() << endl;
	}
	
	while( infi )
	{
		std::string sn0, sn1;
		infi >> sn0;
		infi >> sn1;
		
		int nm;
		infi >> nm;
		
		for( unsigned mc = 0; (mc < nm) && infi; ++mc )
		{
			Match M;
			M.s0 = sn0;
			M.s1 = sn1;
			infi >> M.kp0;
			infi >> M.kp1;
			
			if( infi )
				matches.push_back( M );
		}
	}
	cout << matches.size() << endl;
}



void GetMatchErrors( std::map< std::string, Calibration > calibs, std::map< std::string, std::vector< hVec2D > > &kps, std::vector< Match > &matches )
{
	//
	// Because we already "know" the calibration, we can get some idea of how good each match is
	// in a very trivial way.
	//
	for( unsigned mc = 0; mc < matches.size(); ++mc )
	{
		Match &m = matches[mc];
		
		hVec3D c0, c1;
		c0 = calibs[ m.s0 ].GetCameraCentre();
		c1 = calibs[ m.s1 ].GetCameraCentre();
		
		hVec2D o0, o1;
		o0 = kps[ m.s0 ][ m.kp0 ];
		o1 = kps[ m.s1 ][ m.kp1 ];
		
		hVec3D d0, d1;
		d0 = calibs[ m.s0 ].Unproject( o0 );
		d1 = calibs[ m.s1 ].Unproject( o1 );
		
/*
		m.error = DistanceBetweenRays( c0, d0, c1, d1 );
/*/
		hVec3D p = IntersectRays( {c0,c1}, {d0,d1} );
		hVec2D p0 = calibs[ m.s0 ].Project( p );
		hVec2D p1 = calibs[ m.s1 ].Project( p );

		m.error = (p0 - o0).norm() + (p1 - o1).norm();
//*/
		cout << m.s0 << " " << m.s1 << " : " << m.kp0 << " " << m.kp1 << " : " << m.error << endl;
	}
	
	std::sort( matches.begin(), matches.end() );
	return;
}


genMatrix GetPoints( std::map< std::string, std::shared_ptr< ImageDirectory > > &sources, std::map< std::string, Calibration > calibs, std::map< std::string, std::vector< hVec2D > > &kps, std::vector< Match > &matches, float maxErr )
{
	struct Skpid
	{
		std::string sn;
		int kpc;
		
		bool operator<( const Skpid &oth) const
		{
			if( sn < oth.sn )
				return true;
			else if( sn == oth.sn )
				return kpc < oth.kpc;
			else
				return false;
		}
	};
	std::map< Skpid, int> clustIds;
	
	// each keypoint that's part of a match into it's own cluster...
	int cid = 0;
	for( unsigned mc = 0; mc < matches.size(); ++mc )
	{
		Skpid kpid0; kpid0.sn = matches[mc].s0; kpid0.kpc = matches[mc].kp0;
		Skpid kpid1; kpid1.sn = matches[mc].s1; kpid1.kpc = matches[mc].kp1;
		
		clustIds[ kpid0 ] = cid++;
		clustIds[ kpid1 ] = cid++;
		
		cout << cid << " " << clustIds.size() << endl;
	}
	
	
	for( unsigned mc = 0; mc < matches.size(); ++mc )
	{
		Match &m = matches[mc];
		Skpid kpid0; kpid0.sn = matches[mc].s0; kpid0.kpc = matches[mc].kp0;
		Skpid kpid1; kpid1.sn = matches[mc].s1; kpid1.kpc = matches[mc].kp1;
		
		if( m.error < maxErr )
		{
			// put second keypoint in cluster of first keypoint.
			clustIds[ kpid1 ] = clustIds[ kpid0 ];
		}
	}
	
	std::map< int, std::vector< Skpid > > clusters;
	for( auto ci = clustIds.begin(); ci != clustIds.end(); ++ci )
	{
		clusters[ ci->second ].push_back( ci->first );
	}
	
	
	std::vector< hVec3D > points;
	std::vector< cv::Vec3b > colours;
	std::vector< int > usedClusts;
	for( auto cli = clusters.begin(); cli != clusters.end(); ++cli )
	{
		std::vector< hVec3D > cents, dirs;
		for( unsigned pc = 0; pc < cli->second.size(); ++pc )
		{
			Skpid k = cli->second[ pc ];
			cents.push_back( calibs[ k.sn ].GetCameraCentre() );
			dirs.push_back( calibs[ k.sn ].Unproject( kps[ k.sn ][ k.kpc ] ) );
		}
		
		if( cents.size() >= 2 )
		{
			hVec3D p = IntersectRays( cents, dirs );
			points.push_back( p );

			int row, col;
			auto k   = cli->second[ 0 ];
			auto p2d = kps[ k.sn ][ k.kpc ];
			row = p2d(1);
			col = p2d(0);

			cv::Mat i = sources[ k.sn ]->GetCurrent();
			cv::Vec3b px = i.at< cv::Vec3b > (row,col);
			colours.push_back( px );

			//TODO: Robustness.
			
			usedClusts.push_back( cli->first );
		}
	}
	
	std::ofstream outfi("matches");
	outfi << sources.size() << endl;
	for( auto si = sources.begin(); si != sources.end(); ++si )
		outfi << si->first << "/" << endl;
	
	cid = 0;
	outfi << usedClusts.size() << endl;
	for( unsigned uc = 0; uc < usedClusts.size(); ++uc )
	{
		int ui = usedClusts[ uc ];
		
		outfi << cid << " " << clusters[ ui ].size() << endl;
		for( unsigned kpc = 0; kpc < clusters[ ui ].size(); ++kpc )
		{
			Skpid k = clusters[ui][ kpc ];
			hVec2D o = kps[ k.sn ][ k.kpc ];
			outfi << k.sn << "/ " << o.transpose() << endl;
		}
		
		++cid;
	}
	outfi.close();
	
	genMatrix pts( 4+3, points.size() );
	for( unsigned pc = 0; pc < points.size(); ++pc )
	{
		pts.col( pc ) << points[pc](0), points[pc](1), points[pc](2), 1.0, colours[pc][0], colours[pc][1], colours[pc][2];
	}
	
	return pts;
}


