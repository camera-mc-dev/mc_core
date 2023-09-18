#include <iostream>
using std::cout;
using std::endl;

#include "imgio/vidsrc.h"
#include "renderer2/basicRenderer.h"

#include <vector>
#include <opencv2/opencv.hpp>
using std::vector;

#include <opencv2/xfeatures2d.hpp>
#include <opencv2/ximgproc.hpp>
#include <opencv2/ximgproc/slic.hpp>

#include "calib/circleGridTools.h"

int main(int argc, char* argv[])
{
	if( argc != 2 && argc != 3 )
	{
		cout << "test mser grid detector: " << endl;
		cout << argv[0] << " <source> [jump to frame] " << endl;
		cout << endl;
		exit(0);
	}
	// is it a directory?
	ImageSource *src;
	if( boost::filesystem::is_directory( argv[1] ))
	{
		src = (ImageSource*) new ImageDirectory( argv[1] );
	}
	else
	{
		src = (ImageSource*) new VideoSource( argv[1], "none");
	}
	
	if( argc == 3 )
		src->JumpToFrame( atoi(argv[2]) );
	
	int MSER_delta = 2;
	int MSER_minArea = 3*2;
	int MSER_maxArea = 20*20;
	float MSER_maxVariation = 0.4;
	cv::Ptr< cv::MSER > mser = cv::MSER::create(MSER_delta, MSER_minArea, MSER_maxArea, MSER_maxVariation);
	cv::Ptr< cv::ORB > orb = cv::ORB::create();
	
	int SIFT_numFeats = 0;
	int SIFT_nLayers  = 3;
	double SIFT_contThresh = 0.01; // increase to reduce features (default 0.04)
	double SIFT_edgeThresh = 10;   // decrease to reduce features (default 10)
	double SIFT_sigma      = 1.6;
	cv::Ptr<cv::SIFT> sift = cv::SIFT::create(SIFT_numFeats, SIFT_nLayers, SIFT_contThresh, SIFT_edgeThresh, SIFT_sigma);
	
	
	double SURF_thresh = 5000;
	cv::Ptr<cv::xfeatures2d::SURF> surf = cv::xfeatures2d::SURF::create(SURF_thresh);
	
	std::shared_ptr< Rendering::BasicPauseRenderer > ren;
	Rendering::RendererFactory::Create( ren, 1280, 720, "MSER test" );
	
	CircleGridDetector cgDet(1920, 1080, false, true, CircleGridDetector::MSER_t);
	cgDet.MSER_delta   = 5;
	cgDet.MSER_maxArea = 50*50;
	cgDet.MSER_minArea = 5;
	cgDet.MSER_maxVariation = 0.1;
	cgDet.parallelLineLengthRatioThresh = 0.85;
	cgDet.parallelLineAngleThresh = 15.0;
	
	bool done = false;
	bool paused = false;
	bool advance = false;
	cv::Mat img,grey, grey2;
	while( !done )
	{
		cout << src->GetCurrentFrameID() << endl;
		
		img = src->GetCurrent();
		cv::cvtColor(  img,  grey, cv::COLOR_BGR2GRAY );
		cv::cvtColor( grey, grey2, cv::COLOR_GRAY2BGR );
		
		
		std::vector< CircleGridDetector::GridPoint > gridPoints;
		cgDet.FindGrid( grey, 9, 10, false, false, gridPoints );
// 		cgDet.FindGrid( grey, 5, 9, false, false, gridPoints );
		cout << "gridPoints: " << gridPoints.size() << endl;
		for( unsigned gpc = 0; gpc < gridPoints.size(); ++gpc )
		{
			float b = 255.0f * (float)gridPoints[gpc].row / 9.0f;
			float r = 255.0f * (float)gridPoints[gpc].col / 10.0f;
			
			cv::circle( 
			            grey2, 
			            cv::Point( gridPoints[gpc].pi(0), gridPoints[gpc].pi(1) ), 
			            gridPoints[gpc].blobRadius,
			            cv::Scalar(b,255,r),
			            3
			          );
		}
		
		vector< cv::KeyPoint > akps;
		cgDet.GetAlignmentPoints( akps );
		for( unsigned kc = 0; kc < akps.size(); ++kc )
		{
			cv::circle( 
			            grey2, 
			            cv::Point( akps[kc].pt.x, akps[kc].pt.y ), 
			            akps[kc].size,
			            cv::Scalar(0,0,255),
			            3
			          );
		}
		
		
		
		ren->Get2dBgCamera()->SetOrthoProjection(0, img.cols, 0, img.rows, -100, 100 );
		ren->SetBGImage( grey2 );
		
		ren->Step(paused, advance);
// 		while( paused && !advance )
// 		{
// 			ren->Step(paused, advance);
// 		}
// 		advance = false;
		
		done = !src->Advance();
	}
	
}
