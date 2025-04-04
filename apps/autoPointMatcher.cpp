#include "imgio/sourceFactory.h"
#include "commonConfig/commonConfig.h"

#include "renderer2/showImage.h"
#include "imgproc/idiapMSER/mser.h"

#include "math/distances.h"

#include "libconfig.h++"
#include "nanoflann.hpp"

#include <opencv2/xfeatures2d.hpp>

struct SFeatures
{
	std::vector< cv::KeyPoint > kps;
	cv::Mat descriptors;
};

struct SMatches
{
	std::map< unsigned, std::vector<unsigned> > f;
	std::map< unsigned, std::vector<unsigned> > b;
};

typedef std::pair< unsigned, unsigned > fid_t;
struct SClusters
{
	std::map< unsigned long, std::vector< fid_t > > pts;
};


SFeatures GetFeatures( cv::Mat img );
SMatches  GetMatches( SFeatures f0, SFeatures f1 );
SMatches FilterMatches( SMatches matches,  SFeatures f0, SFeatures f1, Calibration c0, Calibration c1 );
SClusters ConsolidateMatches( std::vector<SFeatures> &features, std::map< std::pair<unsigned, unsigned>, SMatches > &matches );


int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		cout << "Tool to perform automatic point matches to aide calibration" << endl;
		cout << "I'm not trying to replace colmap. I'm _not_. Honest I'm _really_ _not_." << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <calibration config file> " << endl;
		return(1);
	}
	
	CommonConfig ccfg;
	
	//
	// Read the calibration config files to get the basic info we need,
	// i.e. image source paths, which frame number to use, and where to write the 
	// matches to.
	//
	libconfig::Config cfg;
	std::string dataRoot = ccfg.dataRoot;
	std::string testRoot;
	
	std::map< std::string, std::string > imgDirs;
	
	std::string matchesFile;
	std::vector< std::string > sourcePaths;
	int matchFrame;
	
	try
	{
		cfg.readFile( argv[1] );
		
		if( cfg.exists("dataRoot") )
			dataRoot = (const char*)cfg.lookup("dataRoot");
		testRoot = (const char*)cfg.lookup("testRoot");
		
		
		matchesFile = dataRoot + testRoot + (const char*)  cfg.lookup("matchesFile");
		
		
		libconfig::Setting &idirs  = cfg.lookup("imgDirs");
		for( unsigned ic = 0; ic < idirs.getLength(); ++ic )
		{
			std::string s;
			s = dataRoot + testRoot + (const char*) idirs[ic];
			sourcePaths.push_back(s);
		}
		
		matchFrame = cfg.lookup("matchFrame");
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
	
	
	
	
	//
	// Load up the images.
	//
	cout << "Loading images." << endl;
	std::vector< cv::Mat > imgs( sourcePaths.size() );
	std::vector< Calibration > calibs( sourcePaths.size() );
	std::vector< std::string > srcNames( sourcePaths.size() );
	for( unsigned sc = 0; sc < sourcePaths.size(); ++sc )
	{
		cout << "\t" << std::setw(5) << sc << " : " << sourcePaths[sc] << endl;
		auto s = CreateSource( sourcePaths[sc] );
		srcNames[ sc ] = s.name;
		
		s.source->JumpToFrame( matchFrame );
		imgs[sc]   = s.source->GetCurrent();
		calibs[sc] = s.source->GetCalibration();
	}
	
	
	
	//
	// Now we do the hard work.
	//
	
	
	//
	// Feature extraction.
	//
	cout << "Getting features" << endl;
	std::vector<SFeatures> features( imgs.size() );
	for( unsigned sc = 0; sc < imgs.size(); ++sc )
	{
		cout << "\t" << std::setw(5) << sc << " : " << std::flush;
		features[sc] = GetFeatures( imgs[sc] );
		cout << features[sc].kps.size() << endl;
		
		// cv::Mat i0 = imgs[ sc ].clone();
		// for( unsigned pc = 0; pc < features[ sc ].kps.size(); ++pc )
		// {
		// 	cv::Point p0 = features[ sc ].kps[ pc ].pt;
		// 	cv::circle( i0, p0, 7, cv::Scalar( 255, 0, 0 ) );
		// }
		// Rendering::ShowImage( i0 );
	}
	
	
	
	
	
	
	//
	// two-camera feature matching.
	//
	cout << "Brute force matching between pairs of images:" << endl;
	std::map< std::pair<unsigned, unsigned>, SMatches > matches;
	for( unsigned sc0 = 0; sc0 < imgs.size(); ++sc0 )
	{
		for( unsigned sc1 = sc0+1; sc1 < imgs.size(); ++sc1 )
		{
			cout << "\t" << std::setw(4) << sc0 << " <-> " << std::setw(4) << sc1 << " : " << std::flush;
			matches[ std::make_pair( sc0, sc1 ) ] = GetMatches( features[sc0], features[sc1] );
			cout << matches[ std::make_pair( sc0, sc1 ) ].f.size() << " " << matches[ std::make_pair( sc0, sc1 ) ].b.size() << endl;
		}
		cout << endl;
	}
	
	
	
	
	//
	// Match filtering.
	//
	float totalMatches = 0;
	std::map< std::pair<unsigned, unsigned>, SMatches > fMatches;
	for( unsigned sc0 = 0; sc0 < imgs.size(); ++sc0 )
	{
		for( unsigned sc1 = sc0+1; sc1 < imgs.size(); ++sc1 )
		{
			auto mp = std::make_pair( sc0, sc1 );
			
			fMatches[ mp ] = FilterMatches( matches[mp], features[ sc0 ], features[ sc1 ], calibs[sc0], calibs[sc1]  );
			cout <<  matches[ mp ].f.size() << " " <<  matches[ mp ].b.size() << " -> " << 
			        fMatches[ mp ].f.size() << " " << fMatches[ mp ].b.size() << endl;
			
			totalMatches += fMatches[ mp ].f.size();
			
// 			cv::Mat i0 = imgs[ sc0 ].clone();
// 			for( auto mi = matches[mp].f.begin(); mi != matches[mp].f.end(); ++mi )
// 			{
// 				cout << mi->first << " " << mi->second.size() << endl;
// 				cv::Point p0 = features[ sc0 ].kps[ mi->first ].pt;
// 				cv::circle( i0, p0, 7, cv::Scalar( 128, 0, 128 ) );
// 				for( unsigned mc = 0; mc < mi->second.size(); ++mc )
// 				{
// 					cv::Point p1 = features[ sc1 ].kps[ mi->second[mc] ].pt;
// 					cv::Point d  = p1 - p0;
// 					
// 					cv::line( i0, p0, p0 + d/10, cv::Scalar( 255,0,0 ), 3 );
// 				}
// 			}
// 			for( auto mi = fMatches[mp].f.begin(); mi != fMatches[mp].f.end(); ++mi )
// 			{
// 				cv::Point p0 = features[ sc0 ].kps[ mi->first ].pt;
// 				cv::circle( i0, p0, 7, cv::Scalar( 128, 0, 128 ) );
// 				for( unsigned mc = 0; mc < mi->second.size(); ++mc )
// 				{
// 					cv::Point p1 = features[ sc1 ].kps[ mi->second[mc] ].pt;
// 					cv::Point d  = p1 - p0;
// 					
// 					cv::line( i0, p0, p0 + d/10, cv::Scalar( 255,255,0 ), 1 );
// 				}
// 			}
			
		}
		
	}
	cout << "leaving: " << totalMatches << " matches" << endl;
	
	
	//
	// And consolidate that into clusters of matches corresponding to the same 3D point. Hopefully....
	//
	SClusters finalMatches = ConsolidateMatches( features, fMatches );
	cout << "clustered points: " << finalMatches.pts.size() << endl;
	
	//
	// Then just save a nice matches file. Ho ho ho.
	//
	SaveMatches( finalMatches, features, matchesFile );
	
}






SFeatures GetFeatures( cv::Mat img )
{
	// ahhh... which detector to use?
	SFeatures f;
	
	// SURF is fast and known
	cv::Ptr<cv::xfeatures2d::SURF> surf = cv::xfeatures2d::SURF::create( 500.0f );
	surf->detect( img, f.kps );
	
	
	// // MSD is slow but interesting.
	// cv::Ptr< cv::xfeatures2d::MSDDetector > msd = cv::xfeatures2d::MSDDetector::create( 7, 11, 5, 0, 250.0, 4, 1.25, -1, true );
	// msd->detect( img, f.kps );
	
	
	
// 	cv::Mat grey0, grey1;
// 	cv::cvtColor( img, grey0, cv::COLOR_BGR2GRAY );
// 	grey1 = 255-grey0;
// 	
// 	MSER mser8( 2.0, 10.0 / (float)(grey0.rows*grey0.cols), 5000.0 / (float)(grey0.rows*grey0.cols), 0.5, 0.33, true);
// 	std::vector<MSER::Region> regions0, regions1, regions;
// 	mser8(grey0.data, grey0.cols, grey0.rows, regions0);
// 	mser8(grey1.data, grey1.cols, grey1.rows, regions1);
// 	
// 	regions.insert( regions.end(), regions0.begin(), regions0.end() );
// 	regions.insert( regions.end(), regions1.begin(), regions1.end() );
// 	f.kps.resize( regions.size() );
// 	for( unsigned rc = 0; rc < regions.size(); ++rc )
// 	{
// 		f.kps[rc].size = sqrt( regions[rc].area_ );
// 		
// 		f.kps[rc].pt.x = regions[rc].moments_[0] / regions[rc].area_;
// 		f.kps[rc].pt.y = regions[rc].moments_[1] / regions[rc].area_;
// 	}
	
	
	
	// ORB is likely to be a good descriptor.
	cv::Ptr<cv::ORB> orb = cv::ORB::create(
	                                          500,        // nfeatures (number of features to retain)
	                                          1.2f,       // scaleFactor (pyramid decimation ratio)
	                                          8,          // nlevels (number of pyramid levels)
	                                          31,         // edgeThreshold (size of the border where features are not detected)
	                                          0,          // firstLevel (level of pyramid to start from)
	                                          3,          // WTA_K (2, 3, or 4)
	                                          cv::ORB::HARRIS_SCORE, // scoreType (HARRIS_SCORE or FAST_SCORE)
	                                          31,         // patchSize (size of the patch used for orientation)
	                                          20          // fastThreshold (FAST corner detection threshold)
	                                      );
	
	orb->compute(img, f.kps, f.descriptors);
	
	return f;
	
}


SMatches  GetMatches( SFeatures f0, SFeatures f1 )
{
	// NOTE: if use ORB with WTA_K of 3 or 4, use HAMMING2, otherwise use HAMMING.
	//       We also enable cross-checking as that's generally good for robustness.
	cv::BFMatcher matcher(cv::NORM_HAMMING2, false);
	
	
	// match forwards
	SMatches m;
	std::vector< std::vector< cv::DMatch > > mf;
	matcher.knnMatch( f0.descriptors, f1.descriptors, mf, 5 );
	assert( mf.size() == f0.kps.size() );
	for( unsigned fc = 0; fc < mf.size(); ++fc )
	{
		assert( mf[fc][0].queryIdx == fc );
		for( unsigned mc = 0; mc < mf[fc].size(); ++mc )
		{
			m.f[fc].push_back( mf[fc][mc].trainIdx );
		}
	}
	
	// match backwards
	std::vector< std::vector< cv::DMatch > > mb;
	matcher.knnMatch( f1.descriptors, f0.descriptors, mb, 5 );
	assert( mb.size() == f1.kps.size() );
	for( unsigned fc = 0; fc < mb.size(); ++fc )
	{
		assert( mb[fc][0].queryIdx == fc );
		for( unsigned mc = 0; mc < mb[fc].size(); ++mc )
		{
			m.b[fc].push_back( mb[fc][mc].trainIdx );
		}
	}
	
	
	return m;
}



// TODO config option.
const float DIST_THRESH = 20.0f / 1000.0f; 
SMatches FilterMatchesWithCalib( SMatches matches, SFeatures f0, SFeatures f1, Calibration c0, Calibration c1  )
{
	SMatches fMatches;
	
	//
	// Look at the forward matches first...
	//
	hVec3D cc0 = c0.GetCameraCentre();
	hVec3D cc1 = c1.GetCameraCentre();
	for( auto mi = matches.f.begin(); mi != matches.f.end(); ++mi )
	{
		unsigned fi0 = mi->first;
		hVec2D kp0;  kp0 << f0.kps[ fi0 ].pt.x, f0.kps[ fi0 ].pt.y, 1.0;
		hVec3D r0 = c0.Unproject( kp0 );
		
		
		float bestErr = DIST_THRESH;
		int bestfi1 = mi->second.size();
		std::vector< float > dists( mi->second.size() );
		for( unsigned f1c = 0; f1c < mi->second.size(); ++f1c )
		{
			unsigned fi1 = mi->second[ f1c ];
			if( fi1 < f1.kps.size() )
			{
				hVec2D kp1;  kp1 << f1.kps[ fi1 ].pt.x, f1.kps[ fi1 ].pt.y, 1.0;
				
				
				hVec3D r1 = c1.Unproject( kp1 );
				
				dists[f1c] = DistanceBetweenRays( cc0, r0,   cc1, r1 );
				if( dists[f1c] < bestErr )
				{
					bestErr = dists[f1c];
					bestfi1 = fi1;
				}
			}
			else
			{
				// why would this be the case?
				std::cout << "CHECK ME" << endl;
			}
		}
		
		if( bestErr < DIST_THRESH )
		{
			fMatches.f[ fi0 ].push_back( bestfi1 );
		}
	}
	
	//
	// Aaaannnd... that's probably enough.
	//
	
	
	return fMatches;
}









SMatches FilterMatches( SMatches matches, SFeatures f0, SFeatures f1, Calibration c0, Calibration c1  )
{
	SMatches fMatches;
	
	//
	// The filtering should ensure:
	//   1) filtered matches are 1-to-1
	//   2) filtered matches are consistent with epipolar geometry constraints.
	//
	// Epipolar geometry constraints could be found by:
	//   1) Actually computing epipolar geometry (e.g. F) with robust RANSAC approach and keeping inliers
	//   2) Checking the local match direction is consistent in any one part of the image.
	//
	// We know that (2) is generally quite valuable - but if we _already_ have calibration we can use that too.
	//
	
	
	// if we have calibration already... Great!
	if( c0.K.diagonal().squaredNorm() > 0 && c1.K.diagonal().squaredNorm() > 0 )
	{
		fMatches = FilterMatchesWithCalib( matches,       f0, f1,       c0, c1 );
	}
	
	
	return fMatches;
}


SClusters ConsolidateMatches( std::vector<SFeatures> &features,  std::map< std::pair<unsigned, unsigned>, SMatches > &matches )
{
	//
	// Now we need to bring all of these matches together into points.
	// This is fundamentally a clustering process.
	// 
	
	// start with every feature in its own cluster.
	std::map< fid_t, unsigned long > f2c;
	
	unsigned long cid = 0;
	for( unsigned sc = 0; sc < features.size(); ++sc )
	{
		for( unsigned fc = 0; fc < features[sc].kps.size(); ++fc )
		{
			auto p = std::make_pair( sc, fc );
			f2c[ p ] = cid;
			++cid;
		}
	}
	
	// Now we go through the matches and shift features into clusters based on matches.
	// At this point, we assume matches are good.
	for( auto smi = matches.begin(); smi != matches.end(); ++smi )
	{
		// so these are matches between these two sources...
		unsigned sc0 = smi->first.first;
		unsigned sc1 = smi->first.second;
		
		// and these are the forward matches...
		for( auto mi = smi->second.f.begin(); mi != smi->second.f.end(); ++mi )
		{
			// there should only be one match per feature... but make sure there _is_ a match...
			if( mi->second.size() > 0 )
			{
				// so these are the two features in this match.
				auto f0 = std::make_pair( sc0, mi->first );
				auto f1 = std::make_pair( sc1, mi->second[0] );
				
				// and they are in these clusters...
				auto cid0 = f2c[ f0 ];
				auto cid1 = f2c[ f1 ];
				
				// it should be enough to do this... (?)
				f2c[ f1 ] = std::min( cid0, cid1 );
			}
		}
	}
	
	// and now we can just... you know.. consolidate that...
	SClusters res;
	for( auto fi = f2c.begin(); fi != f2c.end(); ++fi )
	{
		res.pts[ fi->second ].push_back( fi->first );
	}
	
	return res;
	
}




