#include "calib/circleGridTools.h"
#include "math/distances.h"
#include "math/intersections.h"
#include "commonConfig/commonConfig.h"

#include <iostream>
#include <set>
using std::cout;
using std::endl;

#include "nanoflann.hpp"

#include "libconfig.h++"

#include <chrono>

#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"


class CircleDetector : public cv::Feature2D
{
public:

	CircleDetector(unsigned minSize)
	{
		fdet = cv::MSER::create(3,  // delta. def=5
								minSize, // minArea. def=60
								14400,
								0.1, // max variation
								0.2, // color only from here... \/
								200,
								1.01,
								0.03,
								5);
	}

	CV_WRAP static cv::
	Ptr<CircleDetector> create(unsigned minSize)
	{
		return cv::makePtr<CircleDetector>(minSize);
	}

	void detect(cv::InputArray image, std::vector<cv::KeyPoint>& keypoints, cv::InputArray)
	{
		keypoints.clear();
		cv::Mat gs;
		if (image.channels() == 3)
			cvtColor(image, gs, cv::COLOR_BGR2GRAY);
		else
			gs = image.getMat();

		std::vector< cv::KeyPoint > kps;
		fdet->detect(gs, kps);

		// make sure each keypoint has a centre darker than its surround...
		std::vector< cv::KeyPoint > kps2;
		for( unsigned kc = 0; kc < kps.size(); ++kc )
		{
			cv::Rect r;
			r.x = kps[kc].pt.x - kps[kc].size/2;
			r.y = kps[kc].pt.y - kps[kc].size/2;
			r.width = kps[kc].size;
			r.height = kps[kc].size;

			if( r.x < 0 || r.y < 0 || r.x > gs.cols-r.width-1 || r.y > gs.rows-r.height-1 )
			{
				continue;
			}

			cv::Mat w = gs(r);
			double m,M;
			cv::minMaxIdx(w, &m, &M);


			if( gs.at<unsigned char>(kps[kc].pt.y, kps[kc].pt.x) < (m+M)/2.0 )
			{
				kps2.push_back( kps[kc] );
			}
		}

		// make sure keypoints don't overlap...
		for(unsigned kc = 0; kc < kps2.size(); ++kc)
		{
			bool keep = true;
			for( unsigned kc2 = 0; kc2 < keypoints.size(); ++kc2 )
			{
				cv::Point2f d = kps2[kc].pt - keypoints[kc2].pt;
				if( cv::norm(d) < kps2[kc].size )
					keep = false;
			}
			if( keep )
			{
				keypoints.push_back( kps2[kc] );
			}
		}
	}


protected:
	cv::Ptr<cv::FeatureDetector> fdet;

};

// given an image source, find the circle centres
// for a circle-grid calibration board in the images.
// specify the number of rows and number of columns in the grid.
//
// Get grids for all images in the source.
void GetCircleGrids(ImageSource *imgSrc, vector< vector<cv::Point2f> > &centers, unsigned rows, unsigned cols, bool visualise)
{
	centers.clear();
	do
	{
		vector< cv::Point2f > c;
		bool found = GetCircleGrids(imgSrc, c, rows, cols, visualise);
		if( found && c.size() == (rows*cols) )
			centers.push_back(c);
	}while( imgSrc->Advance() );
}

// Get grid for the current image in the source.
// Returns false if grid not detected.
bool GetCircleGrids(ImageSource *imgSrc, vector<cv::Point2f> &centers, unsigned rows, unsigned cols, bool visualise)
{
	cv::Size patternsize(rows, cols);

	// downscaling the image is a bad idea because we'll lose
	// accuracy. However, the circle detection runs much much much quicker on
	// smaller images.
	// so, if our image is particularly large, then we can downsize it to
	// something smaller, try to find the circle grid, and if we succeed,
	// only then run the circle finder on the full size image.
	cv::Mat img = imgSrc->GetCurrent();

	float aspectRat = img.rows / (float)img.cols;
	vector<cv::Point2f> smallImgRes;
	cv::Mat imgSmall, graySmall;
	if( img.cols > 800 )
	{
		cv::resize(img, imgSmall,  cv::Size(800, 800*aspectRat ));
		cv::cvtColor(imgSmall, graySmall, CV_BGR2GRAY );

		// cv::SimpleBlobDetector::Params params;
		// params.minThreshold = 28;
		// params.thresholdStep = 1;
		// params.maxThreshold = 52;

		// cv::Ptr<cv::FeatureDetector> blobDetector = cv::SimpleBlobDetector::create(params);

		cv::Ptr<cv::FeatureDetector> blobDetector = CircleDetector::create(20);
		bool found = cv::findCirclesGrid(graySmall, patternsize, smallImgRes, cv::CALIB_CB_SYMMETRIC_GRID | cv::CALIB_CB_CLUSTERING, blobDetector);
		if(!found)
		{
			if( visualise )
			{
				cv::Mat visImg = img.clone();
				cv::resize( visImg, visImg, cv::Size(800, 800*aspectRat));
				cv::imshow("img", visImg);
				cv::waitKey(1);
			}
			return false;
		}
	}

	// either the image was small enough to go at it full size,
	// or we found the grid in the smaller image and thus
	// now consider it worth attacking at full resolution.
	cv::Mat gray;
	cv::cvtColor(img, gray, CV_BGR2GRAY);

	// cv::SimpleBlobDetector::Params params;
	// if(img.cols > 1000)
	// 	params.maxArea = 10e4; //TODO: Better control over this parameter.

	// params.minThreshold = 28;
	// params.thresholdStep = 1;
	// params.maxThreshold = 52;
	// cv::Ptr<cv::FeatureDetector> blobDetector = cv::SimpleBlobDetector::create(params);

	cv::Ptr<cv::FeatureDetector> blobDetector = CircleDetector::create(400);

	bool found = cv::findCirclesGrid(gray, patternsize, centers, cv::CALIB_CB_SYMMETRIC_GRID , blobDetector);
	if(!found)
	{
		if( visualise )
		{
			cv::Mat visImg = img.clone();
			cv::drawChessboardCorners(visImg, patternsize, cv::Mat(centers), found);
			cv::resize( visImg, visImg, cv::Size(800, 800*aspectRat));
			cv::imshow("img", visImg);
			cv::waitKey(1);
		}

		centers.clear();

		// rescale the small image result - at least it was something!
		float scale = img.cols / (float)imgSmall.cols;
		for( unsigned pc = 0; pc < smallImgRes.size(); ++pc )
		{
			centers.push_back( smallImgRes[pc] * scale );
		}



		if( visualise )
		{
			std::cout << "Failed at full res, but this was low res detections: " << std::endl;
			cv::Mat visImg = img.clone();
			cv::drawChessboardCorners(visImg, patternsize, cv::Mat(centers), found);
			cv::resize( visImg, visImg, cv::Size(800, 800*aspectRat));
			cv::imshow("img", visImg);
			cv::waitKey(1);
		}

		return false;
	}
	// else
	{
		if( rows == cols )
		{
			// if rows == cols then it can be unclear in the image which
			// orientation the board is in, which would then make
			// matching corners between different cameras observing the same
			// board a little bit confusing.
			// it seems like some of the time the bottom left circle is
			// the board origin, and some of the time, the bottom right
			// circle is the board origin. This code only handles
			// naively testing for and switching between those two possibilities.
			auto d1 = centers[1] - centers[0];
			auto d2 = centers[rows] - centers[0];

			// I want d1.x larger than d2.x...
			if( fabs( d1.x ) < fabs( d2.x ))
			{
				// now we need to re-order the points in the vector.
				// argh...
				vector<cv::Point2f> reo( centers.size() );

				for( unsigned pc = 0; pc < centers.size(); ++pc )
				{
					unsigned dr = rows - 1 - (pc%rows);
					unsigned dc = pc / cols;

					unsigned nindx = dr * rows + dc;
					reo[nindx] = centers[pc];
				}

				centers.clear();
				centers = reo;
			}
		}
		if( visualise )
		{
			cv::Mat visImg = img.clone();
			cv::drawChessboardCorners(visImg, patternsize, cv::Mat(centers), found);
			cv::resize( visImg, visImg, cv::Size(800, 800*aspectRat));
			cv::imshow("img", visImg);
			cv::waitKey(1);
		}
		return true;
	}
	return false; // shouldn't get here.
}


// Build the "object" points for a grid of a specified size
// in terms of the number of rows and columns, and the spacing
// between rows and columns.
void BuildGrid(vector< cv::Point3f > &gridCorners, unsigned rows, unsigned cols, float rowSpace, float colSpace)
{
	gridCorners.clear();
	for( unsigned rc = 0; rc < rows; ++rc )
	{
		for( unsigned cc = 0; cc < cols; ++cc)
		{
			gridCorners.push_back( cv::Point3f(rc * rowSpace, cc*colSpace, 0.0f));
		}
	}
}






CircleGridDetector::CircleGridDetector(unsigned w, unsigned h, bool useHypothesis, bool visualise, blobDetector_t in_bdt)
{
	float ar = h / (float)w;
	maxGridlineError = 32.0f;
	
	showVisualiser = visualise;
	useGridPointHypothesis = useHypothesis;
	
	MSER_delta = 5;
	MSER_minArea = 8*8;
	MSER_maxArea = 70*70;
	MSER_maxVariation = 0.2;
	
	SURF_thresh = 3000;
	
	blobDetector = in_bdt;
	
	SURF_thresh = 5000;
	
	blobDetector = in_bdt;
	
	
	cd_minMagThresh = 0.1;
	cd_detThresh = 0.35;
	cd_rescale = 0.5;
	
	potentialLinesNumNearest = 21;
	parallelLineAngleThresh  = 5;      //(degrees)
	parallelLineLengthRatioThresh = 0.7;  
	
	gridLinesParallelThresh = 25;      // (degrees)
	gridLinesPerpendicularThresh = 60; // (degrees)
	gapThresh = 0.8;                   // 16 (pixels)
	
	alignDotSizeDiffThresh = 50; // pixels (?)
	alignDotDistanceThresh = 10; // pixels

	if (showVisualiser)
	{
		CommonConfig ccfg;
		
		float winW, winH;
		winW = w; winH = h;
		if( winW > ccfg.maxSingleWindowWidth )
		{
			winW = ccfg.maxSingleWindowWidth;
			winH = ar * winW;
		}
		if( h > ccfg.maxSingleWindowHeight )
		{
			winH = ccfg.maxSingleWindowHeight;
			winW = winH / ar;
		}
		
		Rendering::RendererFactory::Create( ren, winW, winH, "circle detector debug" );

		ren->Get2dBgCamera()->SetOrthoProjection(0, w, 0, h, -100, 100 );
		ren->Get2dFgCamera()->SetOrthoProjection(0, w, 0, h, -100, 100 );

	}
}



CircleGridDetector::CircleGridDetector( unsigned w, unsigned h, libconfig::Setting &cfg )
{
	cd_minMagThresh = 0.1;
	cd_detThresh = 0.35;
	cd_rescale = 0.5;
	
	try
	{
		if( cfg.exists("visualise") )
		{
			showVisualiser = cfg.lookup("visualise");
		}
		
		
		MSER_delta = 3;
		if( cfg.exists("MSER_delta") )
		{
			MSER_delta = cfg.lookup("MSER_delta");
		}
		
		MSER_minArea = 50;
		if( cfg.exists("MSER_minArea") )
		{
			MSER_minArea = cfg.lookup("MSER_minArea");
		}
		
		MSER_maxArea = 200*200;
		if( cfg.exists("MSER_maxArea") )
		{
			MSER_maxArea = cfg.lookup("MSER_maxArea");
		}
		
		MSER_maxVariation = 1.0;
		if( cfg.exists("MSER_maxVariation") )
		{
			MSER_maxVariation = cfg.lookup("MSER_maxVariation");
		}
		
		
		SURF_thresh = 5000.0f;
		if( cfg.exists("SURF_thresh") )
		{
			SURF_thresh = cfg.lookup("SURF_thresh");
		}
		
		blobDetector = MSER_t;
		if( cfg.exists("blobDetector") )
		{
			std::string bds = (const char*) cfg.lookup("blobDetector");
			if( bds.compare("MSER") )
			{
				blobDetector = MSER_t;
			}
			else if( bds.compare("SURF") )
			{
				blobDetector = SURF_t;
			}
			else if( bds.compare("CIRCD") )
			{
				blobDetector = CIRCD_t;
			}
		}
		
		
		
		potentialLinesNumNearest = 8;
		if( cfg.exists("potentialLinesNumNearest") )
		{
			potentialLinesNumNearest = cfg.lookup("potentialLinesNumNearest");
		}
		
		parallelLineAngleThresh  = 5;      //(degrees)
		if( cfg.exists("parallelLineAngleThresh") )
		{
			parallelLineAngleThresh = cfg.lookup("parallelLineAngleThresh");
		}
		
		parallelLineLengthRatioThresh = 0.7;  
		if( cfg.exists("parallelLineLengthRatioThresh") )
		{
			parallelLineLengthRatioThresh = cfg.lookup("parallelLineLengthRatioThresh");
		}
		
		gridLinesParallelThresh = 25;      // (degrees)
		if( cfg.exists("gridLinesParallelThresh") )
		{
			gridLinesParallelThresh = cfg.lookup("gridLinesParallelThresh");
		}
		
		gridLinesPerpendicularThresh = 60; // (degrees)
		if( cfg.exists("gridLinesPerpendicularThresh") )
		{
			gridLinesPerpendicularThresh = cfg.lookup("gridLinesPerpendicularThresh");
		}
		
		gapThresh = 0.8;                    // 0.8 ( ratio of potential gap to current gap )
		if( cfg.exists("gapThresh") )
		{
			gapThresh = cfg.lookup("gapThresh");
		}
		
		
		alignDotSizeDiffThresh = 25; // pixels (?)
		if( cfg.exists("alignDotSizeDiffThresh") )
		{
			alignDotSizeDiffThresh = cfg.lookup("alignDotSizeDiffThresh");
		}
		
		
		alignDotDistanceThresh = 10; // pixels
		if( cfg.exists("alignDotDistanceThresh") )
		{
			alignDotDistanceThresh = cfg.lookup("alignDotDistanceThresh");
		}
		
		
		maxGridlineError = 32;
		if( cfg.exists("maxGridlineError") )
		{
			maxGridlineError = cfg.lookup("maxGridlineError");
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
	
	
}





bool CircleGridDetector::FindGrid( cv::Mat in_img, unsigned in_rows, unsigned in_cols, bool hasAlignmentDots, bool isGridLightOnDark, std::vector< GridPoint > &gridPoints )
{
	// Ensure grid points is empty.
	gridPoints.clear();

	grey = in_img;
	rows = in_rows;
	cols = in_cols;
	
	imgWidth = in_img.cols;
	imgHeight = in_img.rows;

	hVec2D tl;
	tl << 0,0,1.0f;

	maxHypPointDist = 10.0f;

	auto t0 = std::chrono::steady_clock::now();
	
	// Find keypoints. Return false if less than minimum number are found.
	FindKeypoints(isGridLightOnDark);
	if (kps.size() < rows*cols)
	{
		
		if( showVisualiser )
		{
			ren->SetActive();
			cout << "detection failed: insufficient keypoints" << endl;
			ren->ClearLinesAndGPs();
			Eigen::Vector4f c; c << 1.0, 0.0, 0.0, 0.75;
			ren->RenderKeyPoints( grey, kps, c );
			while( !ren->Step() );
			ren->SetInactive();
		}
		return false;
	}
	else
	{
		if( showVisualiser )
		{
			ren->SetActive();
			cout << "kps: " << kps.size() << " " << ikps.size() << endl;
			ren->ClearLinesAndGPs();
			Eigen::Vector4f c; c << 0.0, 1.0, 0.0, 0.75;
			ren->RenderKeyPoints( grey, kps, c );
			c << 1.0, 0.0, 0.0, 0.75;
			ren->RenderKeyPoints( grey, ikps, c );
			while( !ren->Step() );
			ren->SetInactive();
		}
		
	}
	
	auto t1 = std::chrono::steady_clock::now();
	
	// Get initial lines. Return false if no initial lines.
	GetInitialLines();
	if (initLines.empty())
	{
		if( showVisualiser )
		{
			ren->SetActive();
			ren->ClearLinesAndGPs();
			Eigen::Vector4f c; c << 1.0, 0.0, 0.0, 0.75;
			ren->RenderKeyPoints( grey, kps, c );
			
			cout << "detection failed: no initial lines" << endl;
			while( !ren->Step() );
			ren->SetInactive();
			
		}
		return false;
	}
	else
	{
		if( showVisualiser )
		{
			ren->SetActive();
			ren->ClearLinesAndGPs();
			Eigen::Vector4f c; c << 1.0, 0.0, 1.0, 0.75;
			ren->RenderKeyPoints( grey, kps, c );
			
			c << 0.5, 0.0, 0.5, 0.75;
			ren->RenderKeyPoints( grey, ikps, c );
			
			c << 1.0, 0.0, 0.0, 0.75;
			ren->RenderLines(grey, initLines, kps, c );
			
			cout << "initial lines" << endl;
			while( !ren->Step() );
			ren->SetInactive();
			
		}
	}
	
	auto t2 = std::chrono::steady_clock::now();
	
	bool gotGL = GetGridLines();
	
	//if( !gotGL && showVisualiser )
	if( showVisualiser )
	{
		ren->SetActive();
		ren->ClearLinesAndGPs();
		cout << "I: " << initLines.size() << endl;
		cout << "A: " << gridLinesA.size() << endl;
		cout << "B: " << gridLinesB.size() << endl;
		
		Eigen::Vector4f c;
// 		c << 0.0, 0.0, 0.6, 0.75;
// 		ren->RenderLines(grey, initLines, kps, c );
		c << 1.0, 0.0, 0.0, 0.75;
		ren->RenderLines(grey, gridLinesA, kps, c );
		c << 1.0, 1.0, 0.0, 0.75;
		ren->RenderLines(grey, gridLinesB, kps, c );
		
		if( !gotGL )
		{
			cout << "Failed getting grid lines" << endl;
		}
		else
		{
			cout << "got grid lines" << endl;
		}
		
		while( !ren->Step() );
		ren->SetInactive();
	}
	
	auto t3 = std::chrono::steady_clock::now();
	
	bool gotOrder = false;
	if( gotGL )
		gotOrder = SelectRowsCols(hasAlignmentDots);
	
	
// 	if( !gotOrder && showVisualiser )
	if( showVisualiser )
	{
		ren->SetActive();
		if( !gotOrder )
			cout << "Failed getting line order" << endl;
		else
			cout << "Got line order" << endl;
		ren->ClearLinesAndGPs();
		cout << "A: " << gridLinesA.size() << endl;
		cout << "B: " << gridLinesB.size() << endl;
		
		Eigen::Vector4f c;
		c << 0.0, 0.0, 0.6, 0.75;
		ren->RenderLines(grey, initLines, kps, c );
		c << 1.0, 0.0, 0.0, 0.75;
		ren->RenderLines(grey, gridLinesA, kps, c );
		c << 1.0, 1.0, 0.0, 0.75;
		ren->RenderLines(grey, gridLinesB, kps, c );

		
		while( !ren->Step() );
		ren->SetInactive();
	}
	
	auto t4 = std::chrono::steady_clock::now();
	
	
	if( gotOrder )
		GetGridVerts( gridPoints );

	// Visualise the process.
	if (showVisualiser)
	{
		ren->SetActive();
		cout << "A: " << gridLinesA.size() << endl;
		cout << "B: " << gridLinesB.size() << endl;
		
		ren->ClearLinesAndGPs();
		Eigen::Vector4f c;
		ren->RenderGridPoints( grey, gridPoints );
		
		for( unsigned gc = 0; gc < gridPoints.size(); ++gc )
		{
			gridPoints[gc].blobRadius = keyPointSize;
		}
		ren->RenderGridPoints( grey, gridPoints, "averageSize_" );
		
		if( !gotGL || !gotOrder)
		{
			// render lines red and yellow to show failure.
			c << 1.0, 0.0, 0.0, 0.75;
			ren->RenderLines(grey, gridLinesA, kps, c );
			c << 1.0, 1.0, 0.0, 0.75;
			ren->RenderLines(grey, gridLinesB, kps, c );
		}
		else
		{
			// green and cyan for success.
			c << 0.0, 1.0, 0.0, 0.75;
			ren->RenderLines(grey, gridLinesA, kps, c );
			c << 0.0, 1.0, 1.0, 0.75;
			ren->RenderLines(grey, gridLinesB, kps, c );
		}
		
		while( !ren->Step() );
		ren->SetInactive();
	}
	
	
	auto t5 = std::chrono::steady_clock::now();
	
	
	cout << "\tMSER     : " << std::chrono::duration <double, std::milli> (t1-t0).count() << "ms" << endl;
	cout << "\tinit lns : " << std::chrono::duration <double, std::milli> (t2-t1).count() << "ms" << endl;
	cout << "\tgrid lns : " << std::chrono::duration <double, std::milli> (t3-t2).count() << "ms" << endl;
	cout << "\trows/cols: " << std::chrono::duration <double, std::milli> (t4-t3).count() << "ms" << endl;
	cout << "\tverts    : " << std::chrono::duration <double, std::milli> (t5-t4).count() << "ms" << endl;
	
	return gotGL && gotOrder;
}


void CircleGridDetector::RoughClassifyKeypoints( cv::Mat grey, bool isGridLightOnDark, vector< cv::KeyPoint > &filtkps )
{
	vector< cv::KeyPoint > tmpkps;
	if( blobDetector == MSER_t )
	{
		// MSER makes an excellent blob detector for
		// black (white) circles on a white (black) background but is very slow.
		cv::Ptr< cv::MSER > mser = cv::MSER::create(MSER_delta, MSER_minArea, MSER_maxArea, MSER_maxVariation);
		mser->detect( grey, tmpkps );
	}
	else if( blobDetector == SURF_t )
	{
		// with the right tuning, SURF is much faster and can do a pretty good job.
		cv::Ptr<cv::xfeatures2d::SURF> surf = cv::xfeatures2d::SURF::create(SURF_thresh);
		surf->detect( grey, tmpkps );
		
		// one thing to watch out for though is that the size of a SURF features seems to be
		// about 4 times too large.
		for( unsigned kpc = 0; kpc < tmpkps.size(); ++kpc )
		{
			tmpkps[kpc].size *= 0.5;    // 1/4 == 0.25, but we want a wee-bit-larger.
		}
	}
	else if( blobDetector == CIRCD_t )
	{
		GridCircleFinder( grey, cd_minMagThresh, cd_detThresh, cd_rescale, tmpkps );
	}
	
	// put all the keypoint locations in one matrix.
	Eigen::MatrixXf kpM;
	kpM = Eigen::MatrixXf::Zero( filtkps.size(), 2 );
	for( unsigned ac = 0; ac < filtkps.size(); ++ac )
	{
		kpM(ac,0) = filtkps[ac].pt.x;
		kpM(ac,1) = filtkps[ac].pt.y;
	}
	
	// then make a KDtree of them.
	typedef nanoflann::KDTreeEigenMatrixAdaptor< Eigen::MatrixXf >  kd_tree_t;
	kd_tree_t kpIndx(2, kpM, 10);
	kpIndx.index->buildIndex();
	
	kps.clear();
	ikps.clear();
	std::vector<float> kpDists;
	std::vector< std::vector<int> > kpN;
	for( unsigned kpc = 0; kpc < filtkps.size(); ++kpc )
	{
		// find our n nearest neighbours
		std::vector<float> q(2);
		q[0] = filtkps[kpc].pt.x;
		q[1] = filtkps[kpc].pt.y;
		
		// find nearest
		vector<size_t>   ret_indexes(potentialLinesNumNearest);
		vector<float> out_dists_sqr(potentialLinesNumNearest);
		nanoflann::KNNResultSet<float> resultSet(potentialLinesNumNearest);

		resultSet.init(&ret_indexes[0], &out_dists_sqr[0] );
		kpIndx.index->findNeighbors(resultSet, &q[0], nanoflann::SearchParams(10));
		
		//
		// If we area grid point, then we should be able to find at least 3 neighbours such that 
		// the neighbour has almost the same greylevel as us, and mid-way to the neighbour is 
		// an opposite greylevel.
		// (Note: this is not true for an alignment point )
		//
		hVec2D a; a << q[0], q[1], 1.0f;
		bool print = false;
		if( a(0) > 790 && a(0) < 800 && a(1) > 760 && a(1) < 770 )
			print = true;
		if(print)
			cout << kpc << ":" << a.transpose() << endl;
		std::vector< int > goodNeighbours;
		std::vector< float > dists;
		for( unsigned nc = 0; nc < ret_indexes.size(); ++nc )
		{
			int kpc0 = ret_indexes[nc];
			
			
			hVec2D b; b << kpM(kpc0,0), kpM(kpc0,1), 1.0f;
			hVec2D c = (a+b)/2.0f;
			
			unsigned g0 = grey.at<unsigned char>( a(1), a(0) );
			unsigned g1 = grey.at<unsigned char>( b(1), b(0) );
			unsigned g2 = grey.at<unsigned char>( c(1), c(0) );
			
			// TODO:
			// can we do this without a threshold?
			//
			float m = (g0+g1)/2.0f;
			float g2rat = std::min( (float) g2, m)/std::max( (float) g2, m);
			float g01rat = std::abs( (float)g0 - (float)g1 ) / 255.0f;
			float szrat  = std::min( filtkps[kpc].size, filtkps[kpc0].size ) / (float)std::max( filtkps[kpc].size, filtkps[kpc0].size );
			if( print )
				cout << "\t" << b.transpose() << " - " << g01rat << " " << g2rat << " " << (int)g0 << " " << (int)g1 << " " << (int)g2 << endl;
			if(
			    g01rat < 0.2 &&
			    g2rat  < 0.8 &&
			    ( (isGridLightOnDark && g2 < m ) || (!isGridLightOnDark && g2 > m ) ) &&
			    szrat  > 0.8 
			  )
			{
				goodNeighbours.push_back( kpc0 );
				dists.push_back( (a-b).norm() );
			}
		}
		
		if( goodNeighbours.size() >= 3 )
		{
			kps.push_back( filtkps[kpc] );
			std::sort( dists.begin(), dists.end() );
			
			float d = 0;
			for( unsigned c = 0; c < 3; ++c )
				d += dists[c];
			
			kpDists.push_back( d / 3.0 );
			kpN.push_back( goodNeighbours );
		}
		else
		{
			ikps.push_back( filtkps[kpc] );
		}
	}
	
	
	
	//
	// Build another kd-tree, but this time with just the kps...
	//
	if( kps.size() > 0 )
	{
		kpM = Eigen::MatrixXf::Zero( kps.size(), 2 );
		for( unsigned ac = 0; ac < kps.size(); ++ac )
		{
			kpM(ac,0) = kps[ac].pt.x;
			kpM(ac,1) = kps[ac].pt.y;
		}
		
		kd_tree_t kpIndx2(2, kpM, 10);
		kpIndx2.index->buildIndex();
		
		//
		// Now look at the remaining features, as they _might_ be alignment dots
		//
		filtkps = ikps;
		ikps.clear();
		for( unsigned kpc = 0; kpc < filtkps.size(); ++kpc )
		{
			// find our 1 nearest neighbour in what we think are grid points.
			// an alignment dot should be the same distance away from the 
			// nearest grid dot as the shortest of the grid dots kdDists
			
			std::vector<float> q(2);
			q[0] = filtkps[kpc].pt.x;
			q[1] = filtkps[kpc].pt.y;
			
			// find nearest
			vector<size_t>   ret_indexes(1);
			vector<float> out_dists_sqr(1);
			nanoflann::KNNResultSet<float> resultSet(1);
			
			resultSet.init(&ret_indexes[0], &out_dists_sqr[0] );
			kpIndx2.index->findNeighbors(resultSet, &q[0], nanoflann::SearchParams(10));
			
			float d = sqrt(out_dists_sqr[0]);
			int kpc0 = ret_indexes[0];
			
			
			// what is the line from the potential alignment to the potential grid point?
			hVec2D a; a << filtkps[kpc].pt.x, filtkps[kpc].pt.y, 1.0f;
			hVec2D b; b << kps[kpc0].pt.x, kps[kpc0].pt.y, 1.0f;
			hVec2D ab = b-a;
			
			// we know that the line a+s(ab) must pass through every row of the grid if a
			// is an alignment dot. But we will only later know about grid lines.
			// what we're interested in here is merely the potential to be an alignment dot.
			
			// distance might be a good enough cue?
			float score = std::min(ab.norm(), kpDists[kpc0])/std::max(ab.norm(), kpDists[kpc0]);
			if( score > 0.75 )
			{
				ikps.push_back( filtkps[kpc] );
			}
			
		}
	}
}


void CircleGridDetector::TestKeypoints( cv::Mat grey, bool isGridLightOnDark, vector< cv::KeyPoint > &out_kps, std::vector<float> &scores )
{
	// initial keypoints
	vector< cv::KeyPoint > filtkps;
	InitKeypoints( grey, filtkps );
	
	RoughClassifyKeypoints( grey, isGridLightOnDark, filtkps );
	
	out_kps.clear();
	out_kps.insert( out_kps.end(), kps.begin(), kps.end() );
	scores.assign( kps.size(), 1.0 );
	
	out_kps.insert( out_kps.end(), ikps.begin(), ikps.end() );
	while( scores.size() != out_kps.size() )
		scores.push_back(0.0f);
	
	
}


void CircleGridDetector::InitKeypoints(cv::Mat &grey, std::vector<cv::KeyPoint> &filtkps)
{
	vector< cv::KeyPoint > tmpkps;
	
	if( blobDetector == MSER_t )
	{
		// MSER makes an excellent blob detector for
		// black (white) circles on a white (black) background but is very slow.
		cv::Ptr< cv::MSER > mser = cv::MSER::create(MSER_delta, MSER_minArea, MSER_maxArea, MSER_maxVariation);
		mser->detect( grey, tmpkps );
	}
	else if( blobDetector == SURF_t )
	{
		// with the right tuning, SURF is much faster and can do a pretty good job.
		cv::Ptr<cv::xfeatures2d::SURF> surf = cv::xfeatures2d::SURF::create(SURF_thresh);
		surf->detect( grey, tmpkps );
		
		// one thing to watch out for though is that the size of a SURF features seems to be
		// about 4 times too large.
		for( unsigned kpc = 0; kpc < tmpkps.size(); ++kpc )
		{
			tmpkps[kpc].size *= 0.5;    // 1/4 == 0.25, but we want a wee-bit-larger.
		}
	}
	else if( blobDetector == CIRCD_t )
	{
		GridCircleFinder( grey, cd_minMagThresh, cd_detThresh, cd_rescale, tmpkps );
	}
	
	//
	// Remove duplicate filters.
	//
	filtkps.clear();
	std::vector<unsigned> count;
	for( unsigned kc = 0; kc < tmpkps.size(); ++kc )
	{
		hVec2D a; a << tmpkps[kc].pt.x, tmpkps[kc].pt.y, 1.0f;
		bool add = true;
		
		for( unsigned kc0 = 0; kc0 < filtkps.size(); ++kc0 )
		{
			hVec2D b; b << filtkps[kc0].pt.x, filtkps[kc0].pt.y, 1.0f;
			
			float d = (a-b).norm();
			
			// overlap?
			if( d < std::min( tmpkps[kc].size/2, filtkps[kc0].size/2 ) )
			{
				// size too simillar?
				float minSize = std::min( tmpkps[kc].size, filtkps[kc0].size );
				float maxSize = std::max( tmpkps[kc].size, filtkps[kc0].size );
				if( minSize / maxSize > 0.2 )
				{
					add = false;
					
					// update size
					float v = count[kc0] / ((float)count[kc0]+1.0f);
					filtkps[kc0].size = v * filtkps[kc0].size + (1.0-v) * tmpkps[kc].size;
				}
			}
		}
		
		if( add )
		{
			filtkps.push_back( tmpkps[kc] );
			count.push_back(1);
		}
	}
}

void CircleGridDetector::FindKeypoints(bool isGridLightOnDark)
{
	auto t0 = std::chrono::steady_clock::now();
	
	std::vector<cv::KeyPoint> filtkps;
	InitKeypoints(grey, filtkps);
	
	
	
	auto t1 = std::chrono::steady_clock::now();
	
	RoughClassifyKeypoints( grey, isGridLightOnDark, filtkps );
	
	auto t2 = std::chrono::steady_clock::now();
	
	
	cout << "\t\tdetect   : " << std::chrono::duration <double, std::milli> (t1-t0).count() << "ms" << endl;
	cout << "\t\tpost : " << std::chrono::duration <double, std::milli> (t2-t1).count() << "ms" << endl;
	
}


void CircleGridDetector::GetInitialLines()
{
	initLines.clear();
	
	
	// put all the keypoint locations in one matrix.
	Eigen::MatrixXf kpM;
	kpM = Eigen::MatrixXf::Zero( kps.size(), 2 );
	for( unsigned ac = 0; ac < kps.size(); ++ac )
	{
		kpM(ac,0) = kps[ac].pt.x;
		kpM(ac,1) = kps[ac].pt.y;
	}
	
	// then make a KDtree of them.
	typedef nanoflann::KDTreeEigenMatrixAdaptor< Eigen::MatrixXf >  kd_tree_t;
	kd_tree_t kpIndx(2, kpM, 10);
	kpIndx.index->buildIndex();
	
	// find the initial lines, which are the lines between each keypoint and its n (4?) closest neighbours
	std::vector<float> q(2);
	std::vector< std::vector< kpLine > > potentials;
	potentials.resize( kps.size() );
	unsigned kpWithPot = 0;
	for( unsigned ac = 0; ac < kps.size(); ++ac )
	{
		q[0] = kps[ac].pt.x;
		q[1] = kps[ac].pt.y;
		
		// find nearest
		vector<size_t>   ret_indexes(potentialLinesNumNearest);
		vector<float> out_dists_sqr(potentialLinesNumNearest);
		nanoflann::KNNResultSet<float> resultSet(potentialLinesNumNearest);

		resultSet.init(&ret_indexes[0], &out_dists_sqr[0] );
		kpIndx.index->findNeighbors(resultSet, &q[0], nanoflann::SearchParams(10));
		
		// iterate from 0, but first result should be ourselves?
		for( unsigned bc = 0; bc < potentialLinesNumNearest; ++bc )
		{
			if( ac == ret_indexes[bc] )
				continue;
			
			kpLine l;
			l.a = ac;
			l.b = ret_indexes[bc];
						
			// keypoints of gridlines should be very similar sizes.
			if( std::min( kps[l.a].size, kps[l.b].size ) / std::max( kps[l.a].size, kps[l.b].size ) < 0.7 )
				continue;
			
			// we're not computing an error here, just the distance between points.
			// but we can abuse the use of l.err so we can sort these lines on their
			// distance from kps[ac] and keep the shortest ones.
			auto d = kps[l.b].pt - kps[l.a].pt;
			l.err = cv::norm( d );

			l.p0 << kps[l.a].pt.x, kps[l.a].pt.y, 1.0;
			l.d  << d.x/l.err, d.y/l.err, 0.0f;

			potentials[ac].push_back(l);
		}
		
		if( potentials[ac].size() > 0 )
			++kpWithPot;
	}
//	cout << "kpwp: " << kpWithPot << endl;
	
	// filter the potentials of each keypoint by considering the lines at 
	// the keypoint, and the lines at the other keypoint on the line.
	couldBeGrid.assign( kps.size(), false );
	unsigned cbgc = 0;
	for( unsigned ac = 0; ac < potentials.size(); ++ac )
	{
		
		if( potentials[ac].size() == 0 )
			continue;
		
		for( unsigned pc = 0; pc < potentials[ac].size() && couldBeGrid[ac] == false; ++pc )
		{
			// Another useful filter at this early stage is to observe that for 99% of grid keypoints, at least 
			// one of the keypoints it links to will have a potential line that is near enough the same angle and 
			// length of this keypoint.
			
			kpLine &la = potentials[ac][pc];
			unsigned bc = la.b;
					
			for( unsigned pc2 = 0; pc2 < potentials[bc].size(); ++pc2 )
			{
				kpLine &lb = potentials.at(bc).at(pc2);
				
				// make sure lb does not just come directly back to a
				if( lb.b == ac )
					continue;
				
				// compare direction/angle and length of lb vs. la.
				float lena = la.err;
				float lenb = lb.err;
				
				float cang = std::max(-1.0f, std::min(1.0f, la.d.dot(lb.d)));	// min and max avoid rounding errors going outside -1->1 range
				float ang = acos( fabs(cang) );
				
//				if( fabs(la.d(0)) < 0.1 * fabs(la.d(1)) )
//				{
//					cout << la.d.transpose() << " | " << lb.d.transpose() << endl;
//					cout << lena << " " << lenb << " " << ang << " " << (std::min(lena, lenb) / std::max(lena, lenb) > 0.7) << " " << (ang < 5 * 3.14 / 180.0) << endl;
//				}
				
				if( std::min(lena, lenb) / std::max(lena, lenb) > parallelLineLengthRatioThresh && ang < parallelLineAngleThresh * 3.14 / 180.0)
				{
					// if similar enough, this keypoint passes muster as a potential grid keypoint.
					couldBeGrid[ac] = true;
					++cbgc;
				}
								
			}			
		}
		
		if( !couldBeGrid[ac] )
			continue;

//		std::sort( potentials[ac].begin(), potentials[ac].end() );
//		
//		// these lines should be mostly the same length...
//		vector<float> ds( std::min(4, (int)potentials[ac].size() ) );
//		
//		// these are the lengths
//		float mean = 0.0f;
//		for( unsigned c = 0; c < ds.size(); ++c )
//		{
//			ds[c] = potentials[ac][c].err;
//			mean += ds[c];
//		}
//		mean /= ds.size();
//		float var = 0.0f;
//		for( unsigned c = 0; c < ds.size(); ++c )
//		{
//			var += pow( ds[c]-mean, 2 );
//		}
//		var /= ds.size();
//		
//		// if the variance is larger than some percentage of the mean length, this might not be a good set of lines, so maybe, this is not a good keypoint.
//		if( sqrt(var) > 0.18 * mean )
//		{
//			couldBeGrid[ac] = false;
//		}
	}
//	cout << "cbgc: " << cbgc << endl;
	// now keep any potential line that goes between keypoints that have passed our tests...
	keyPointSize = 0;
	unsigned cnt = 0;
	for( unsigned ac = 0; ac < potentials.size(); ++ac )
	{
		
		if( couldBeGrid[ac] == false )
			continue;
		
		for( unsigned pc = 0; pc < potentials[ac].size(); ++pc )
		{
			
			kpLine &la = potentials[ac][pc];
			unsigned bc = la.b;
			
			if( couldBeGrid[ac] && couldBeGrid[bc] )
			{
				assert( potentials[ac][pc].a < kps.size() );
				assert( potentials[ac][pc].b < kps.size() );
				initLines.push_back( potentials[ac][pc] );
				
				keyPointSize += kps[ potentials[ac][pc].a ].size;	// currently confused as to whether size is radius or diameter. Going for radius...
				keyPointSize += kps[ potentials[ac][pc].b ].size;
				cnt += 2;
			}
		}
	}
	
	keyPointSize /= cnt;
}

bool CircleGridDetector::GetGridLines()
{
	gridLinesA.clear();
	gridLinesB.clear();

	// We know what size the grid is meant to be.
	// as such, we can estimate which initLines might be grid lines.
	// A grid line will pass through at least min(row,cols) keypoints,
	// assuming we detected all the grid circles.
	// as such, we find the distance between each line and
	// its closest min(row,col) keypoints.
	float maxErr = 0;
	for( unsigned lc = 0; lc < initLines.size(); ++lc )
	{
		// get the line and squared length
		cv::KeyPoint &a = kps[ initLines[lc].a ];
		cv::KeyPoint &b = kps[ initLines[lc].b ];

		auto ab = b.pt - a.pt;
		float n = cv::norm(ab);
		n = n*n;

		// look at each keypoint and find the distance from
		// the keypoint to the line.
		vector< kpLineDist > errs;
		unsigned numPotGPs = 0;
		for( unsigned kc = 0; kc < kps.size(); ++kc )
		{
			cv::KeyPoint &c = kps[kc];

			auto ac = c.pt - a.pt;

			float top = (ac.x * ab.x) + (ac.y * ab.y);

			float u = top / n;

			auto x = a.pt + u * ab;

			float err = cv::norm(c.pt-x);


			kpLineDist kpld;
			kpld.err = err;
			kpld.kc = kc;
			errs.push_back(  kpld  );

		}

		std::sort( errs.begin(), errs.end() );

		// if there are r rows and c cols, then
		// we can expect at least min(r,c) points
		// to be on the lines. Thus, the best lines
		// will have the lowest error for at least
		// min(r,c) points.

		if( errs.size() < std::min(rows,cols) )
		{
			continue;
		}

		initLines[lc].err = 0.0f;
		unsigned ncbg = 0;
		for( unsigned ec = 0; ec < std::min(rows,cols); ++ec )
		{
			initLines[lc].err += errs[ec].err;
			initLines[lc].closestVerts.push_back( errs[ec].kc );
			
			if( couldBeGrid[errs[ec].kc] )
				++ncbg;
		}
		
		// penalise lines that use keypoints that we think are less likely to be gridpoints.
		initLines[lc].err += std::max(0, (int)std::min(rows,cols) - (int)ncbg) * 10.0f;

		if( initLines[lc].err > maxErr )
			maxErr = initLines[lc].err;
	}

	std::sort(initLines.begin(), initLines.end() );


	// now we need to extract the actual grid lines, rather than
	// point to point lines. If we assume the best scoring point-to-point
	// line is a good grid line, then I think we can add lines that don't
	// use already used vertices...
	vector< bool > used( kps.size(), false );
	cv::KeyPoint &a = kps[ initLines[0].a ];
	cv::KeyPoint &b = kps[ initLines[0].b ];
	auto d = a.pt - b.pt;
	d = d / cv::norm(d);
	for( unsigned lc = 0; lc < initLines.size(); ++lc )
	{
		if( initLines[lc].err > maxGridlineError )
			break;
		cv::KeyPoint &a = kps[ initLines[lc].a ];
		cv::KeyPoint &b = kps[ initLines[lc].b ];

		// does this line have a near-enough to parallel direction?
		auto d2 =  a.pt - b.pt;
		d2 = d2 / cv::norm(d2);

		float cang = std::max(-1.0f, std::min(1.0f, d.dot(d2)));	// std::min avoids rounding errors going outside -1->1 range
		float ang = acos( fabs(cang) );
		if( ang < gridLinesParallelThresh * 3.1415 / 180.0f )
		{
			unsigned numUsed = 0;
			for( unsigned vc = 0; vc < initLines[lc].closestVerts.size(); ++vc )
			{
				if(used[ initLines[lc].closestVerts[vc] ] )
					++numUsed;
			}

			// we're only interested in lines that have no used vertices on them.
			if( numUsed == 0 )
			{
				for( unsigned vc = 0; vc < initLines[lc].closestVerts.size(); ++vc )
					used[ initLines[lc].closestVerts[vc] ] = true;

				//cout << cang << " " << ang << endl;
				gridLinesA.push_back( initLines[lc] );
			}
		}
	}

//	 for( unsigned ac = 0; ac < gridLinesA.size(); ++ac )
//	 {
//	 	cout << gridLinesA[ac].err << endl;
//	 }
//
//	 cout << "A ---" << gridLinesA.size() << endl;


	// find the first line that could be in the B direction
	// of the grid...
	// cout << endl << endl << endl << endl;
	int b0 = -1;
	float ang = 0.0f;
	bool good = false;
	while(ang < gridLinesPerpendicularThresh * 3.1415 / 180.0f && b0 < (int)initLines.size()-1)
	{
		++b0;
		cv::KeyPoint &a = kps[ initLines[b0].a ];
		cv::KeyPoint &b = kps[ initLines[b0].b ];


		auto d2 =  a.pt - b.pt;
		if( cv::norm(d2) > 0 )
		{
			d2 = d2 / cv::norm(d2);

			float cang = std::max(-1.0f, std::min(1.0f, d.dot(d2)));
			ang = acos( fabs(cang) );
			good = true;

		}
	}
	if(!good)
		return false;

	vector< bool > usedb( kps.size(), false );
	cv::KeyPoint &ab = kps[ initLines[b0].a ];
	cv::KeyPoint &bb = kps[ initLines[b0].b ];
	d = ab.pt - bb.pt;
	d = d / cv::norm(d);
	for( unsigned lc = b0; lc < initLines.size(); ++lc )
	{
		if( initLines[lc].err > maxGridlineError )
			break;

		cv::KeyPoint &a = kps[ initLines[lc].a ];
		cv::KeyPoint &b = kps[ initLines[lc].b ];

		// does this line have a near-enough to parallel direction?
		auto d2 =  a.pt - b.pt;
		d2 = d2 / cv::norm(d2);

		float cang = std::max(-1.0f, std::min(1.0f, d.dot(d2)));	// std::min avoids rounding errors going outside -1->1 range
		float ang = acos( fabs(cang) );
		if( ang < gridLinesParallelThresh * 3.1415 / 180.0f )
		{
			unsigned numUsed = 0;
			for( unsigned vc = 0; vc < initLines[lc].closestVerts.size(); ++vc )
			{
				if(usedb[ initLines[lc].closestVerts[vc] ] )
					++numUsed;
			}

			// we're only interested in lines that have no used vertices on them.
			if( numUsed == 0 )
			{
				for( unsigned vc = 0; vc < initLines[lc].closestVerts.size(); ++vc )
					usedb[ initLines[lc].closestVerts[vc] ] = true;

				gridLinesB.push_back( initLines[lc] );
			}
		}
	}

	// but what if we've found too many A and B lines?
	// what do we do then? We have to assume that somewhere
	// in those lines is the correct set of lines, meaning
	// there will have to be a later stage of sorting :)
	// if we don't have enough lines, then that is a clear
	// failure.
	if( gridLinesA.size() < std::min(rows,cols) || gridLinesB.size() < std::min(rows,cols) )
		return false;

	return true;
}

bool CircleGridDetector::SelectRowsCols(bool hasAlignmentDots)
{
	rowLines.clear();
	colLines.clear();
	// which of the two sets of grid lines
	// is the rows, and which is the columns?
	// this matters because it determines the identity
	// of each grid vertex that we return.


	// what is the mean direction of the grid lines?
	hVec2D meanADir;
	hVec2D meanBDir;

	meanADir << 0,0,0.0;
	meanBDir << 0,0,0.0;

	// cout << "A" << endl;
	for( unsigned ac = 0; ac < gridLinesA.size(); ++ac )
	{
		// cout << meanADir.transpose() << " | " << gridLinesA[ac].d.transpose() << endl;
		// the direction could be either way along the line, left or right in the image.
		// make sure it is consistent...
		if( ac > 0 )
		{
			float ang = acos( meanADir.dot( gridLinesA[ac].d ) / (meanADir.norm()*gridLinesA[ac].d.norm()));
			if( ang > 3.14/2.0 )
				gridLinesA[ac].d *= -1.0f;
		}
		meanADir += gridLinesA[ac].d;
	}
	meanADir /= gridLinesA.size();
	meanADir /= meanADir.norm();

	// cout << meanADir.transpose() << endl;

	// cout << "B" << endl;
	for( unsigned bc = 0; bc < gridLinesB.size(); ++bc )
	{
		// cout << meanBDir.transpose() << " | " << gridLinesB[bc].d.transpose() << endl;
		if( bc > 0 )
		{
			float ang = acos( meanBDir.dot( gridLinesB[bc].d ) / (meanBDir.norm()*gridLinesB[bc].d.norm()));
			if( ang > 3.14/2.0 )
				gridLinesB[bc].d *= -1.0f;
		}
		meanBDir += gridLinesB.at(bc).d;
	}
	meanBDir /= gridLinesB.size();
	meanBDir /= meanBDir.norm();

	// cout << meanBDir.transpose() << endl;

	// find a point in the middle of all the grid points
	hVec2D meanStart; meanStart << 0,0,1.0f;
	for( unsigned ac = 0; ac < gridLinesA.size(); ++ac )
	{
		meanStart += gridLinesA.at(ac).p0;
	}
	for( unsigned bc = 0; bc < gridLinesB.size(); ++bc )
	{
		meanStart += gridLinesB.at(bc).p0;
	}
	meanStart /= meanStart(2);

	// order the A-lines along meanB direction.
	// that means intersecting A lines with meanB...
	vector< lineInter > aInters;
	for( unsigned ac = 0; ac < gridLinesA.size(); ++ac )
	{
		// intersect the two rays...
		// a = a0 + ta*ra
		// b = meanStart + tb*meanDir
		//
		// we ultimately just want tb (sorting a lines along b direction)
		//
		float bdx = meanBDir(0);  float bdy = meanBDir(1);
		float mx  = meanStart(0); float my = meanStart(1);
		float ax  = gridLinesA[ac].p0(0); float ay  = gridLinesA[ac].p0(1);
		float adx = gridLinesA[ac].d(0);  float ady = gridLinesA[ac].d(1);
		float top = adx * (ay - my)  -  ady * (ax - mx);
		float bot = bdy * adx        -  bdx * ady;

		lineInter li;
		li.lineIdx = ac;
		li.scalar = top/bot;
		aInters.push_back( li );
	}
	std::sort( aInters.begin(), aInters.end() );


	// order the B-lines along meanA direction.
	vector< lineInter > bInters;
	for( unsigned bc = 0; bc < gridLinesB.size(); ++bc )
	{
		// intersect the two rays...
		// a = a0 + ta*ra
		// b = meanStart + tb*meanDir
		//
		// we ultimately just want tb (sorting a lines along b direction)
		//
		float bdx = meanADir(0);  float bdy = meanADir(1);
		float mx  = meanStart(0); float my = meanStart(1);
		float ax  = gridLinesB[bc].p0(0); float ay  = gridLinesB[bc].p0(1);
		float adx = gridLinesB[bc].d(0);  float ady = gridLinesB[bc].d(1);
		float top = adx * (ay - my)  -  ady * (ax - mx);
		float bot = bdy * adx        -  bdx * ady;

		lineInter li;
		li.lineIdx = bc;
		li.scalar = top/bot;
		bInters.push_back( li );
	}
	std::sort( bInters.begin(), bInters.end() );

	// we will probably currently have more row or column lines than the geometry of the 
    // board tells us we should have. As such, we need to now filter out those extra lines
	if( showVisualiser )
	{
		cout << "num lines pre filter: " << gridLinesA.size() << " " << gridLinesB.size() << endl;
		ren->SetActive();
		ren->ClearLinesAndGPs();
		
		Eigen::Vector4f c;
		c << 0.6, 0.0, 0.0, 0.75;
		ren->RenderLines(grey, gridLinesA, kps, c );
		c << 0.0, 0.0, 0.6, 0.75;
		ren->RenderLines(grey, gridLinesB, kps, c );
		
	}
	FilterLines(gridLinesA, aInters );
	FilterLines(gridLinesB, bInters );
	if( showVisualiser )
	{
		cout << "num lines post filter: " << gridLinesA.size() << " " << gridLinesB.size() << endl;
		Eigen::Vector4f c;
		c << 0.9, 0.0, 0.0, 0.75;
		ren->RenderLines(grey, gridLinesA, kps, c );
		c << 0.0, 0.0, 0.9, 0.75;
		ren->RenderLines(grey, gridLinesB, kps, c );
		
		while( !ren->Step() );
		ren->SetInactive();
	}


	if( showVisualiser )
	{
		// debug here if needed
	}


    // now we decide on whether the A lines or the B lines are the rows, or the columns,
    // of our calibration grid.
    // if the grid is non-square then this is easy, we can just set it as per the known
    // geometry of the grid....
	hVec2D rowDir, colDir;

	if( gridLinesA.size() != gridLinesB.size() &&
		gridLinesA.size() == rows &&
		gridLinesB.size() == cols )
	{
		rowDir = meanADir;
		colDir = meanBDir;
		for( unsigned ac = 0; ac < aInters.size(); ++ac )
		{
			rowLines.push_back( gridLinesA[ aInters[ac].lineIdx ] );
		}
		for( unsigned bc = 0; bc < bInters.size(); ++bc )
		{
			colLines.push_back( gridLinesB[ bInters[bc].lineIdx ] );
		}
	}

	else if( gridLinesA.size() != gridLinesB.size() &&
	         gridLinesA.size() == cols &&
	         gridLinesB.size() == rows )
	{
		colDir = meanADir;
		rowDir = meanBDir;
		for( unsigned ac = 0; ac < aInters.size(); ++ac )
		{
			colLines.push_back( gridLinesA[ aInters[ac].lineIdx ] );
		}
		for( unsigned bc = 0; bc < bInters.size(); ++bc )
		{
			rowLines.push_back( gridLinesB[ bInters[bc].lineIdx ] );
		}
	}

	// It gets more difficult if the grid is square. 
	else if( gridLinesA.size() == gridLinesB.size() &&
	         rows == cols &&
	         gridLinesA.size() == rows )
	{
		// cout << "deciding row or column based on directions: " << endl;
		// cout << "mean A dir: " << meanADir.transpose() << endl;
		// cout << "mean B dir: " << meanBDir.transpose() << endl;
		if( fabs( meanADir(0) ) > fabs( meanBDir(0) ) )
		{
			rowDir = meanADir;
			colDir = meanBDir;
			// set A as rows...
			// cout << "A is rows..." << endl;
			for( unsigned ac = 0; ac < aInters.size(); ++ac )
			{
				rowLines.push_back( gridLinesA[ aInters[ac].lineIdx ] );
			}
			for( unsigned bc = 0; bc < bInters.size(); ++bc )
			{
				colLines.push_back( gridLinesB[ bInters[bc].lineIdx ] );
			}
		}
		else
		{
			colDir = meanADir;
			rowDir = meanBDir;
			// cout << "B is rows..." << endl;
			// set B as rows...
			for( unsigned ac = 0; ac < aInters.size(); ++ac )
			{
				colLines.push_back( gridLinesA[ aInters[ac].lineIdx ] );
			}
			for( unsigned bc = 0; bc < bInters.size(); ++bc )
			{
				rowLines.push_back( gridLinesB[ bInters[bc].lineIdx ] );
			}
		}
	}
	else
	{
		cout << "A, B, rows, cols: " << gridLinesA.size() << " " << gridLinesB.size() << " " << rows << " " << cols << endl;
		return false;
	}
	
	
	// now we need to check if the orientation of the board is correct.
	if( hasAlignmentDots )
	{

		// The grid we use has two extra circles at the bottom, or what we're going to call the bottom.
		// These should be detected in the ikps.
		
		// first off, for the outer row lines, or outer column lines, do they pass through a potential ipkp?
		std::vector<int> alignCheck(4,0);
		akps.clear();
		alignErrLines.clear();
		alignCheck[0] = CheckAlignKP( true, 0 );
		alignCheck[1] = CheckAlignKP( true, 1 );
		alignCheck[2] = CheckAlignKP(false, 0 );
		alignCheck[3] = CheckAlignKP(false, 1 );
		
// 		cout << "align check (must be ++00, --00, 00++, or 00--): " << endl;
// 		for( unsigned ic = 0; ic < alignCheck.size(); ++ic )
// 		{
// 			cout << alignCheck[ic] << " ";
// 		}
// 		cout << endl << endl;
		
		// now, if all has worked, we should only have 4 possible results.
		// ++00
		// --00
		// 00++
		// 00--
		//
		// We define that the alignment dots will be the bottom of the board,
		// which is to say, they must be at the +1 row.
		// So we need to do two things here:
		// 1) decide if this is a valid result (one of ++00,--00,00++,00--)
		// 2) make sure the rowLines and colLines are ordered correctly to place the 
		//    alignment dots on the bottom of the board.
		
		
		//
		// These first two results say that the alignment dots are currently on a column.
		// So we need to carefully swap our rows and columns.
		//
		if( alignCheck[0] > 0 && alignCheck[1] > 0 && alignCheck[2] == 0 && alignCheck[3] == 0 )
		{
			// the alignment dots should be on the columns, not the rows.
			auto oldCols = colLines;
			auto oldRows = rowLines;
			colLines = oldRows;
			rowLines = oldCols;
			
			alignCheck[2] = alignCheck[0];
			alignCheck[3] = alignCheck[1];
			alignCheck[0] = alignCheck[1] = 0;
		}
		else if( alignCheck[0] < 0 && alignCheck[1] < 0 && alignCheck[2] == 0 && alignCheck[3] == 0 )
		{
			// the alignment dots should be on the columns, not the rows.
			auto oldCols = colLines;
			auto oldRows = rowLines;
			colLines = oldRows;
			rowLines = oldCols;
			
			alignCheck[2] = alignCheck[0];
			alignCheck[3] = alignCheck[1];
			alignCheck[0] = alignCheck[1] = 0;
		}
	
	
		//
		// We are now in a position that, if we have a valid result, the alignment dots must
		// be on the +1 or -1 row.
		// in other words, the solution must be 00++ or 00--
		//
		if( alignCheck[0] == 0 && alignCheck[1] == 0 && alignCheck[2] > 0 && alignCheck[3] > 0 )
		{
			// the rows have the correct order - the alignment dots are beyond the last row, thus, at the bottom of the board.
			// so don't need to do anything here.
		}
		else if( alignCheck[0] == 0 && alignCheck[1] == 0 && alignCheck[2] < 0 && alignCheck[3] < 0 )
		{
			// the rows have the wrong order - the alignment dots are before the first row, thus, at the top of the board.
			// so reverse the order of the rows.
			std::reverse( rowLines.begin(), rowLines.end() );
	// 		cout << "reveresed row order..." << endl;
		}
		else
		{
			// we didn't find alignment dots.
			// for now, just fail: TODO: set an option to allow a best guess if no alignment dots.
			cout << "did not get a suitable solution for the alignment dots - either too many potential dots, insufficient dots, or unsuitable configuration of dots." << endl;
			return false;
		}
		
		
		// now set the column direction correctly...
		
		// what is the direction along the column?
		hVec2D rip0, rip1;
		float t = IntersectRays( colLines[0].p0, colLines[0].d, rowLines[0].p0, rowLines[0].d );
		rip0 = colLines[0].p0 + t * colLines[0].d;
		t = IntersectRays( colLines[0].p0, colLines[0].d, rowLines[1].p0, rowLines[1].d );
		rip1 = colLines[0].p0 + t * colLines[0].d;
		
		hVec2D cd = rip1 - rip0;
		
	// 	cout << "column direction, r0 to r1: " << cd.transpose() << endl;
		
		// we know that cd is "down"
		// what then is the current direction along the rows?
		hVec2D cip0, cip1;
		t = IntersectRays( rowLines[0].p0, rowLines[0].d, colLines[0].p0, colLines[0].d );
		cip0 = rowLines[0].p0 + t * rowLines[0].d;
		t = IntersectRays( rowLines[0].p0, rowLines[0].d, colLines[1].p0, colLines[1].d );
		cip1 = rowLines[0].p0 + t * rowLines[0].d;
		
		hVec2D rd = cip1 - cip0;
	// 	cout << "row direction, c0 to c1: " << rd.transpose() << endl;
		
		// the signed angle between these two is:
		float sa = atan2( rd(1), rd(0) ) - atan2( cd(1), cd(0) );
		
	// 	cout << "signed ang: " << sa << endl;
		
		
	// 	cout << "cd : " << cd.transpose() << endl;
	// 	cout << "rd : " << rd.transpose() << endl;
	// 	cout << "sa : " << sa << endl;
		// and that angle should be -ve
		if( sa >= 3.1416 )
		{
			sa = sa-(2*3.1416);
		}
		if( sa <= -3.1416 )
		{
			sa = (2*3.1416)+sa;
		}
	// 	cout << "signed ang: " << sa << endl;
		if( sa > 0 )
		{
			std::reverse( colLines.begin(), colLines.end() );
	// 		cout << "reversed columns" << endl;
		}
		
	}
	else
	{
		// we just have to do our best possible guess, which is not going to be robust.
		
		// if we assume the calibration board has a more or less consistent orientation,
		// we will assume that the (0,0) circle is the top left in the image.
		
		// col/row tells us if it is column or row _on the grid_, nothing
		// about what direction it is in the image (unless that was how we decided col/row above).
		hVec2D imgRowDir, imgColDir;
		bool imgIsSwapped = false;
		if( fabs( colDir(1) ) > fabs(colDir(0)) )
		{
			imgColDir = colDir;
			imgRowDir = rowDir;
		}
		else
		{
			imgIsSwapped = true;
			imgRowDir = colDir;
			imgColDir = rowDir;
		}
		
	
		if( imgColDir(1) < 0 )
		{
			if( imgIsSwapped )
			{
				std::reverse( colLines.begin(), colLines.end() );
			}
			else
			{
				std::reverse( rowLines.begin(), rowLines.end() );
			}
		}
		if( imgRowDir(0) < 0 )
		{
			if( imgIsSwapped )
			{
				std::reverse( rowLines.begin(), rowLines.end() );
			}
			else
			{
				std::reverse( colLines.begin(), colLines.end() );
			}
		}
	}
	
	if( showVisualiser )
	{
		// debug here if needed
		
		ren->SetActive();
		ren->ClearLinesAndGPs();
		Eigen::Vector4f c; c << 1.0, 0.0, 1.0, 0.75;
		ren->RenderKeyPoints( grey, kps, c, "kp-" );
		
		c << 0.5, 0.0, 0.5, 0.75;
		ren->RenderKeyPoints( grey, ikps, c, "ikp-" );
		
		c << 0.0, 0.5, 0.5, 0.75;
		ren->RenderKeyPoints( grey, akps, c, "akp-" );
		
		c << 1.0, 0.0, 0.0, 0.75;
		ren->RenderLines(grey, colLines, kps);
		ren->RenderLines(grey, rowLines, kps);
		
		cout << "col and row lines" << endl;
		while( !ren->Step() );
		ren->SetInactive();
	}
	
	
	// done!
	return true;
	
}


void CircleGridDetector::GetGridVerts(vector< GridPoint > &finalGridPoints )
{
	assert( finalGridPoints.size() == 0 );

	// intersect the row and column lines, then
	// determine which keypoint best fits each
	// hypothesised grid vertex.

	vector< GridPoint > gridPoints;
	for( unsigned rc = 0; rc < rowLines.size(); ++rc )
	{
		for( unsigned cc = 0; cc < colLines.size(); ++cc )
		{
			// intersect this row line and column line...
			GridPoint gp;
			gp.row = rc;
			gp.col = cc;

			// intersect the two rays...
			// a = r0 + tr*rr
			// b = c0 + tc*rc
			float bdx = colLines[cc].d(0);  float bdy = colLines[cc].d(1);
			float mx  = colLines[cc].p0(0); float my  = colLines[cc].p0(1);
			float ax  = rowLines[rc].p0(0); float ay  = rowLines[rc].p0(1);
			float adx = rowLines[rc].d(0);  float ady = rowLines[rc].d(1);
			float top = adx * (ay - my)  -  ady * (ax - mx);
			float bot = bdy * adx        -  bdx * ady;

			float tb = top / bot;

			gp.ph = colLines[cc].p0 + tb * colLines[cc].d;

			gridPoints.push_back( gp );
		}
	}

	// get the distance of each keypoint to each intersection point
	Eigen::MatrixXf distMat;
	distMat = Eigen::MatrixXf::Zero( gridPoints.size(), kps.size() );
	for( unsigned gpc = 0; gpc < gridPoints.size(); ++gpc )
	{
		for( unsigned kpc = 0; kpc < kps.size(); ++kpc )
		{
			hVec2D k; k << kps[kpc].pt.x, kps[kpc].pt.y, 1.0;

			hVec2D d = gridPoints[gpc].ph - k;

			distMat(gpc, kpc) = d.norm();
		}
	}


	// then greedily find the "best" pairings.
	unsigned associated = 0;
	float min = 0.0f;
	while( associated < gridPoints.size() && min < maxHypPointDist)
	{
		unsigned r,c;
		min = distMat.minCoeff(&r, &c);

		if( min < maxHypPointDist )
		{
			++associated;
			gridPoints[r].pi << kps[c].pt.x, kps[c].pt.y, 1.0;
			gridPoints[r].blobRadius = kps[c].size;
			
			// now we have to handle an edge case.
			// In some situations, particularly relatively near field calibration,
			// the grid circles can be quite large in the image, large enough that
			// when the get to the edge of the image, we only see part of the circle,
			// and thus, we get an innaccurate centre location.
			// our choices then are to use the hypothesis, which might be OK if there is minimal
			// distortion, or to just ignore this point to avoid it polluting later calibrations.
			
			// so, how do we detect problem cases?
			// first guess is when the keypoint centre is less than 1/2 of its radius from an image edge.
			// but that might not be safe in image corners where the radius of the keypoint could be
			// significantly attenuated. Better to use the average keypoint size.
			float minD = std::max( imgWidth, imgHeight );
			minD = std::min( gridPoints[r].pi(0), gridPoints[r].pi(1) ) ;
			minD = std::min( minD, imgWidth - gridPoints[r].pi(0) );
			minD = std::min( minD, imgHeight - gridPoints[r].pi(1) );
			
// 			cout << "md, kps: " << minD << ", " << keyPointSize << endl;
			
			if( minD > keyPointSize )
			{
				for( unsigned c2 = 0; c2 < kps.size(); ++c2 )
				{
					distMat(r,c2) = 999999.9f;
				}

				for( unsigned r2 = 0; r2 < gridPoints.size(); ++r2 )
				{
					distMat(r2, c) = 999999.9f;
				}

				finalGridPoints.push_back( gridPoints[r] );
			}
		}
		else if(useGridPointHypothesis)
		{
			// to be honest, the hypothesised grid position is probably highly usable where the real image feature was not located (?)
			++associated;
			gridPoints[r].pi  = gridPoints[r].ph;
			gridPoints[r].blobRadius = keyPointSize;
			
			float minD = std::max( imgWidth, imgHeight );
			minD = std::min( gridPoints[r].pi(0), gridPoints[r].pi(1) ) ;
			minD = std::min( minD, imgWidth - gridPoints[r].pi(0) );
			minD = std::min( minD, imgHeight - gridPoints[r].pi(1) );
			
			if( minD > keyPointSize )
			{
				for( unsigned c2 = 0; c2 < kps.size(); ++c2 )
				{
					distMat(r,c2) = 999999.9f;
				}

				for( unsigned r2 = 0; r2 < gridPoints.size(); ++r2 )
				{
					distMat(r2, c) = 999999.9f;
				}

				finalGridPoints.push_back( gridPoints[r] );
			}
			finalGridPoints.push_back( gridPoints[r] );
		}
	}
}



void CircleGridDetector::FilterLines(vector< kpLine > &lines, vector< lineInter > &inters )
{
	if( lines.size() == 0 )
		return;
	vector< lineInter > inters1;
	vector< kpLine > lines1;
	
// 	cout << endl << endl << "Start filtering: " << endl;
// 	cout << "inters: " << inters.size() << " : " << endl;
// 	for( unsigned ic = 0; ic < inters.size(); ++ic )
// 	{
// 		cout << "\t" << inters[ic].scalar << " " << endl;
// 	}
// 	cout << "gaps: " << endl;
// 	for( unsigned ic = 1; ic < inters.size(); ++ic )
// 	{
// 		cout << "\t" << inters[ic].scalar - inters[ic-1].scalar << " " << endl;
// 	}
	
	
	// the lines that are part of the grid should have a
	// fairly consistent spacing between them (allowing some leeway for perspective).
	// We should be able to find a set of lines that have a consistent distance between them.
	// find the largest set of lines with a consistent distance between them.
	std::vector<int> bestSet;
	for( unsigned ic1 = 0; ic1 < inters.size(); ++ic1 )
	{
// 		cout << "ic1: " << ic1 << endl;
		for( unsigned ic2 = ic1+1; ic2 < inters.size(); ++ic2 )
		{
// 			cout << "\t ic2: " << ic2 << endl;
			// gap between ic1 and ic2..
			float gap = fabs(inters[ic2].scalar - inters[ic1].scalar);
			
// 			cout << "\t\t gap: " << gap << endl;
			
			std::vector<int> set;
			set.push_back( ic1 );
			set.push_back( ic2 );
			
			// now find the next line at a consistent gap.
			int prev = ic2;
			int next = ic2+1;
			float gap2 = 0;
			while( next < inters.size() && gap2 < (gap + gapThresh*gap) )
			{
				gap2 = fabs(inters[next].scalar - inters[prev].scalar);
// 				cout << "\t\t\tp: " << prev << "   n: " << next << "  g2: " << gap2;  
				if( fabs(gap2 - gap) < gapThresh*gap )
				{
					set.push_back(next);
					prev = next;
					
// 					cout << " ! ";
					float r = set.size()/(set.size()+1);
					gap = r*gap + (1.0-r) * gap2;
				}
// 				cout << endl;
				++next;
			}
			
// 			cout << "\tset: ";
// 			for( unsigned sc = 0; sc < set.size(); ++sc )
// 			{
// 				cout << set[sc] << " ";
// 			}
// 			cout << endl;
			if( set.size() > bestSet.size() )
			{
				bestSet = set;
			}
		}
		
// 		cout << "bestset: ";
// 		for( unsigned sc = 0; sc < bestSet.size(); ++sc )
// 		{
// 			cout << bestSet[sc] << " ";
// 		}
// 		cout << endl;
	}

	
	vector< kpLine > nl;
	vector< lineInter > ni;

	nl.clear();
	ni.clear();
	for( unsigned ic = 0; ic < bestSet.size(); ++ic )
	{
		ni.push_back( inters[ bestSet[ic] ] );
		unsigned lindx = inters[bestSet[ic]].lineIdx;
		nl.push_back( lines[lindx] );
		ni[ ni.size()-1 ].lineIdx = nl.size()-1;
	}



// 
// 	std::vector<float> gaps;
// 	cout << "gaps: " << endl;
// 	for( unsigned ic = 1; ic < inters.size(); ++ic )
// 	{
// 		gaps.push_back(inters[ic].scalar - inters[ic-1].scalar);
// 		cout << gaps[ gaps.size()-1 ] << endl;
// 	}
// 	std::sort(gaps.begin(), gaps.end());
// 
// 	float medGap = gaps[ gaps.size() / 2 ];
// 	float gapThresh = 16.0f;
// 
// 	cout << "mg: " << medGap << endl << endl << endl;
// 
// 	float prevCount = 1;
// 	std::set<unsigned> accepted;
// 	while( ni.size() != prevCount )
// 	{
// 		prevCount = ni.size();
// 		// iterate over each of the lines...
// 		for( unsigned ic = 0; ic < inters.size(); ++ic )
// 		{
// 			if( accepted.find(ic) != accepted.end() )
// 				continue;
// 			
// 			// for this line, what is the gap to the next, and previous, lines?
// 			// obviously, the last/first lines wont have next/previous
// 			unsigned lindx = inters[ic].lineIdx;
// 			float gapToPrevious, gapToNext;
// 			if( ic != 0 && ic != inters.size() -1 )
// 			{
// 				gapToPrevious = inters[ic].scalar - inters[ic-1].scalar;
// 				gapToNext     = inters[ic+1].scalar - inters[ic].scalar;
// 
// 				
// 				if( fabs(gapToNext - medGap) <= gapThresh && fabs(gapToPrevious - medGap) <= gapThresh )
// 				{
// 					// we can clearly keep this line.
// 					ni.push_back( inters[ic] );
// 					nl.push_back( lines[lindx] );
// 					ni[ ni.size()-1 ].lineIdx = nl.size()-1;
// 					accepted.insert(ic);
// 				}
// 				else if( fabs(gapToNext - medGap) <= gapThresh && fabs(gapToPrevious - medGap) > gapThresh  )
// 				{
// 					// only accept if the next line has been accepted.
// 					if( accepted.find(ic+1) != accepted.end() )
// 					{
// 						ni.push_back( inters[ic] );
// 						nl.push_back( lines[lindx] );
// 						ni[ ni.size()-1 ].lineIdx = nl.size()-1;
// 						accepted.insert(ic);
// 					}
// 				}
// 				else if(fabs(gapToNext - medGap) > gapThresh && fabs(gapToPrevious - medGap) <= gapThresh )
// 				{
// 					// only accept if the previous line has been accepted.
// 					if( accepted.find(ic-1) != accepted.end() )
// 					{
// 						ni.push_back( inters[ic] );
// 						nl.push_back( lines[lindx] );
// 						ni[ ni.size()-1 ].lineIdx = nl.size()-1;
// 						accepted.insert(ic);
// 					}
// 				}
// 			}
// 			
// 			
// 			// special case for first line
// 			else if( ic == 0 )
// 			{
// 				gapToNext     = inters[ic+1].scalar - inters[ic].scalar;
// 				if( fabs(gapToNext - medGap ) <= gapThresh )
// 				{
// 					// accept if next has been accepted.
// 					if( accepted.find(ic+1) != accepted.end() )
// 					{
// 						ni.push_back( inters[ic] );
// 						nl.push_back( lines[lindx] );
// 						ni[ ni.size()-1 ].lineIdx = nl.size()-1;
// 						accepted.insert(ic);
// 					}
// 				}
// 			}
// 
// 			
// 			// special case for last line
// 			else if( ic == inters.size() - 1 )
// 			{
// 				gapToPrevious = inters[ic].scalar - inters[ic-1].scalar;
// 				if( fabs(gapToPrevious - medGap ) <= gapThresh )
// 				{
// 					// accept if previous has been accepted.
// 					if( accepted.find(ic-1) != accepted.end() )
// 					{
// 						ni.push_back( inters[ic] );
// 						nl.push_back( lines[lindx] );
// 						ni[ ni.size()-1 ].lineIdx = nl.size()-1;
// 						accepted.insert(ic);
// 					}
// 				}
// 			}
// 		}
// 	}

//	cout << "pre -> post: " << inters.size() << " -> " << ni.size() << endl;

	inters = ni;
	lines = nl;

	std::sort( inters.begin(), inters.end() );

}



int CircleGridDetector::CheckAlignKP(bool isRow, int se )
{
	kpLine *checkLine;
	float ml, Ml;
	std::vector<float> ds;
	float kpSize = 0.0f;
	if( isRow )
	{
		if( se == 0 )
		{
			checkLine = &rowLines[0];
		}
		else
		{
			checkLine = &rowLines[ rowLines.size()-1 ];
		}
		
		// intersect the checkLine with all the column lines.
		for( unsigned cc = 0; cc < colLines.size(); ++cc )
		{
			ds.push_back( IntersectRays( checkLine->p0, checkLine->d, colLines[cc].p0, colLines[cc].d ) );
			
			kpSize += kps[colLines[cc].a].size;
		}
		
		kpSize /= colLines.size();
		
	}
	else
	{
		if( se == 0 )
		{
			checkLine = &colLines[0];
		}
		else
		{
			checkLine = &colLines[ colLines.size()-1 ];
		}
		
		// intersect the checkLine with all the row lines.
		for( unsigned rc = 0; rc < rowLines.size(); ++rc )
		{
			ds.push_back( IntersectRays( checkLine->p0, checkLine->d, rowLines[rc].p0, rowLines[rc].d ) );
			
			kpSize += kps[rowLines[rc].a].size;
		}
		
		kpSize /= rowLines.size();
	}
	
	// we want to hypothesise that there is another row/col that checkLine intersected beyond the real ones.
	float ed0 = ds[0] + (ds[0]-ds[1]);
	float ed1 = ds[ds.size()-1] + (ds[ds.size()-1]-ds[ds.size()-2]);
	
	for( unsigned dc = 0; dc < ds.size(); ++dc )
	{
		cout << ds[dc] << " ";
	}
	cout << endl;
	cout << ed0 << " " << ed1 << endl;
	
	// that defines two points where we expect the alignment point to be, if it exists.
	hVec2D ep0 = checkLine->p0 + ed0 * checkLine->d;
	hVec2D ep1 = checkLine->p0 + ed1 * checkLine->d;
	
	// now, find the ipk that is closest to either ep0 or ep1.
	// also, remember whether it is ep0 or ep1 that it was closest to.
	int closestTo = 0;
	float bestd, bestsd;
	bestd = 1000000;
	unsigned bestkc = 0;
	float tmpbd, tmpsd;
	tmpbd = tmpsd = bestsd = bestd;
	for( unsigned kc = 0; kc < ikps.size(); ++kc )
	{
		hVec2D p; p << ikps[kc].pt.x, ikps[kc].pt.y, 1.0f;
		
		float dep0 = (ep0 - p).norm();
		float dep1 = (ep1 - p).norm();
		float d = std::min(dep0, dep1);
		float which = -1;
		if( dep1 < dep0 )
		{
			which = 1;
		}
		
		cout << kc << " : " << d << endl;
		
		// threshold on our worst acceptable distance between the predicted position and the potential point.
		float sd = fabs(ikps[kc].size - kpSize);
		if( d < bestd && d < alignDotDistanceThresh && sd < alignDotSizeDiffThresh )
		{
			bestkc = kc;
			closestTo = which * std::max(1.0f, d);
			bestd = d;
			bestsd = sd;
		}
		
		if( d < tmpbd )
		{
			tmpbd = d;
			tmpsd = sd;
		}
	}
	
	if( closestTo == 0 )
	{
		cout << "no alignment dot on this proposal. If alignment dots failure common, check these distances against thresholds:" << endl;
		cout << "possible best distance: " << tmpbd << "      had size difference: " << tmpsd << endl;
	}
	else
	{
		cout << "alignment potential: " << bestkc << " " << closestTo << endl << endl;
	}

	
// 	if( closestTo != 0 )
	{
		auto t = ikps[ bestkc ];
		akps.push_back( t );
		
		kpLine tl;
		if( closestTo < 0 )
		{
			tl.p0 = ep0;
			tl.d  << t.pt.x - tl.p0(0), t.pt.y - tl.p0(1), 0.0;
			tl.a = 0;
			tl.b = 0;
		}
		else
		{
			tl.p0 = ep1;
			tl.d  << t.pt.x - tl.p0(0), t.pt.y - tl.p0(1), 0.0;
			tl.a = 0;
			tl.b = 0;
		}
		alignErrLines.push_back(tl);
	}
	
	// all we really need from this is that there was an alignment dot
	// in the appropriate position, and where it was.
	return closestTo;
}








void CGDRenderer::RenderLines( cv::Mat img, const vector< CircleGridDetector::kpLine > &lines, const vector< cv::KeyPoint > &kps, Eigen::Vector4f colour )
{
	SetActive();	
	SetBGImage(img);
	
	for( unsigned lc = 0; lc < lines.size(); ++lc )
	{
		const CircleGridDetector::kpLine &l = lines[lc];
		
		std::stringstream idstr;
		idstr << "line_" << l.p0 << "_" << l.d << "_" << colour.transpose();
		vector<Eigen::Vector2f> points(2);
		points[0] << kps[l.a].pt.x, kps[l.a].pt.y;
		points[1] << kps[l.b].pt.x, kps[l.b].pt.y;
		auto n = GenerateLineNode2D( points, 2.0, idstr.str(), smartThis );
		
		n->SetBaseColour( colour );
		
		linesRoot->AddChild(n);
		
	}
}

void CGDRenderer::RenderLines( cv::Mat img, const vector< CircleGridDetector::kpLine > &lines, Eigen::Vector4f colour )
{
	SetActive();	
	SetBGImage(img);
	
	for( unsigned lc = 0; lc < lines.size(); ++lc )
	{
		const CircleGridDetector::kpLine &l = lines[lc];
		
		std::stringstream idstr;
		idstr << "line_" << l.p0 << "_" << l.d << "_" << colour.transpose();
		vector<Eigen::Vector2f> points(2);
		points[0] = l.p0.head(2);
		points[1] = (l.p0 + l.d).head(2);
		auto n = GenerateLineNode2D( points, 2.0, idstr.str(), smartThis );
		
		n->SetBaseColour( colour );
		
		linesRoot->AddChild(n);
		
	}
}

void CGDRenderer::RenderLines( cv::Mat img, const vector< CircleGridDetector::kpLine > &lines, const vector< cv::KeyPoint > &kps )
{
	SetActive();	
	SetBGImage(img);
	
	for( unsigned lc = 0; lc < lines.size(); ++lc )
	{
		const CircleGridDetector::kpLine &l = lines[lc];
		
		std::stringstream idstr;
		Eigen::Vector4f colour;
		float c = lc / (float)lines.size();
		colour << 1.0-c,c,1.0,0.7;
		
		idstr << "line_" << l.p0 << "_" << l.d << "_" << colour.transpose();
		vector<Eigen::Vector2f> points(2);
		points[0] << kps[l.a].pt.x, kps[l.a].pt.y;
		points[1] << kps[l.b].pt.x, kps[l.b].pt.y;
		auto n = GenerateLineNode2D( points, 2.0, idstr.str(), smartThis );
		n->SetBaseColour( colour );
		
		
		linesRoot->AddChild(n);
		
	}
}





void CGDRenderer::RenderGridPoints( cv::Mat img, const vector< CircleGridDetector::GridPoint > &gps, std::string tag )
{
	SetActive();	
	SetBGImage(img);
	
	for( unsigned gc = 0; gc < gps.size(); ++gc )
	{
		const CircleGridDetector::GridPoint &gp = gps[gc];
		
		std::stringstream idstr;
		Eigen::Vector4f c;
		
		idstr << tag << + "gp_" << gp.row << "_" << gp.col;
		auto n = GenerateCircleNode2D( gp.pi, 10, 2, idstr.str(), smartThis );
		c << gp.row/10.0f, gp.col/10.0f, 1.0f, 0.75;
		n->SetBaseColour(c);
		gpsRoot->AddChild(n);
		
		
		idstr << "_blobRadius" << gp.col;
		auto n2 = GenerateCircleNode2D( gp.ph, gp.blobRadius, 2, idstr.str(), smartThis );
		c << 0.5, 0.5, 0.5, 0.75;
		n2->SetBaseColour(c);
		gpsRoot->AddChild(n2);
		
//		This only serves to show that the hypothesis has no used except in missing points
// 		idstr << "_hypothesis" << gp.col;
// 		auto n2 = GenerateCircleNode2D( gp.ph, 10, 2, idstr.str(), smartThis );
// 		c << 0.5, 0.5, 0.5, 0.75;
// 		n2->SetBaseColour(c);
// 		gpsRoot->AddChild(n2);
	}
}

void CGDRenderer::RenderKeyPoints( cv::Mat img, const vector< cv::KeyPoint > &kps, Eigen::Vector4f colour, std::string tag )
{
	SetActive();	
	SetBGImage(img);
	
	for( unsigned kc = 0; kc < kps.size(); ++kc )
	{
		const cv::KeyPoint &kp = kps[kc];
		
		std::stringstream idstr;
		idstr << tag << kp.pt.x << "_" << kp.pt.y << "_" << kp.size;
		
		hVec2D p; p << kp.pt.x, kp.pt.y, 1.0f; 
		auto n = GenerateCircleNode2D( p, kp.size, 2, idstr.str(), smartThis );
		n->SetBaseColour(colour);
		
		try
		{
			gpsRoot->AddChild(n);
		}
		catch(...)
		{
		}
	}
}

void CGDRenderer::ClearLinesAndGPs()
{
	linesRoot->Clean();
	gpsRoot->Clean();
}


bool CGDRenderer::Step()
{
	Render();

	win.setActive();
	sf::Event ev;
	bool good = false;
	while( win.pollEvent(ev) )
	{
		if( ev.type == sf::Event::KeyReleased )
		{
//			if (event.key.code == sf::Keyboard::Space)
//			{
//				return true;
//			}
			good = true;
		}
	}
	return good;
}







