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
	
	ss.str("");
	
	ss << "cmap/frame-" << std::setw(4) << std::setfill('0') << fc << "/points3D.txt";
	outfi.open( ss.str() );
	outfi << "# 3D point list with one line of data per point:\n#   POINT3D_ID, X, Y, Z, R, G, B, ERROR, TRACK[] as (IMAGE_ID, POINT2D_IDX)" << endl;
	for( unsigned pc = 0; pc < p3d.cols(); ++pc )
	{
		outfi << pc << " " 
		      << p3d(0,pc) << " " << p3d(1,pc) << " " << p3d(2,pc) << " "   // x, y, z
		      << p3d(6,pc) << " " << p3d(5,pc) << " " << p3d(4,pc) << " "   // r, g, b
		      << p3d(7,pc) << endl;                                         // error.
		// we're ignoring track and hoping to get away with it.
	}
	outfi << " Number of points: " << p3d.cols() << ", mean track length: 0" << endl;
	
	
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
	std::vector< Eigen::Vector4f > colours;
	std::vector< int > usedClusts;
	std::vector< float > errors;
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
			
			Eigen::Vector4f colour;
			colour << 0,0,0,0;
			float error = 0.0f;
			for( unsigned pc = 0; pc < cli->second.size(); ++pc )
			{
				int row, col;
				auto k   = cli->second[ pc ];
				auto p2d = kps[ k.sn ][ k.kpc ];
				row = p2d(1);
				col = p2d(0);
				
				cv::Mat i = sources[ k.sn ]->GetCurrent();
				cv::Vec3b px = i.at< cv::Vec3b > (row,col);
				
				colour(0) += px[0];
				colour(1) += px[1];
				colour(2) += px[2];
				colour(3) += 1.0f;
				
				hVec2D pp = calibs[ k.sn ].Project( p );
				error += (pp - p2d).norm();
			}
			error /= colour(3);
			colour /= colour(3);
			colours.push_back( colour );
			errors.push_back( error );
			
			
			
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
	
	genMatrix pts( 4+3+1, points.size() );
	for( unsigned pc = 0; pc < points.size(); ++pc )
	{
		pts.col( pc ) << points[pc](0), points[pc](1), points[pc](2), 1.0, colours[pc].head(3), errors[pc];
	}
	
	return pts;
}


