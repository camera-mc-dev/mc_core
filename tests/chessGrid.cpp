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
	
	
	
	std::shared_ptr< Rendering::BasicPauseRenderer > ren;
	Rendering::RendererFactory::Create( ren, 1280, 720, "MSER test" );
	
	CircleGridDetector cgDet(1920, 1080, false, true, CircleGridDetector::CVCHESS_t);
<<<<<<< HEAD
	
=======
	cgDet.parallelLineLengthRatioThresh = 0.85;
	cgDet.parallelLineAngleThresh = 15.0;
>>>>>>> 297c5688c196453ad807e6d89f763aa2f1d77d84
	
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
		cgDet.FindGrid( grey, 4, 5, false, false, gridPoints );
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
