#include "imgio/imagesource.h"
#include "renderer2/basicRenderer.cpp"

#include "math/intersections.h"
#include "calib/circleGridTools.h"


#include <boost/filesystem.hpp>
#include "imgio/vidsrc.h"

#include <nanoflann.hpp>


int main(int argc, char* argv[] )
{
	//
	// Check...
	//
	if( argc != 2 )
	{
		cout << "supply test image file: " << endl;
		cout << argv[0] << " < image dir > " << endl;
		exit(0);
	}
	
	cout << "creating source: " << argv[1] << endl;
	ImageSource *src;
	if( boost::filesystem::is_directory( argv[1] ))
	{
		src = (ImageSource*) new ImageDirectory(argv[1]);
	}
	else
	{
		src = (ImageSource*) new VideoSource(argv[1], "none");
	}
	
	cout << "creating window" << endl;
	
	cv::Mat img = src->GetCurrent();
	
	float ar = img.rows / (float)img.cols;

	cout << img.rows << " " << img.cols << endl;

	std::shared_ptr<Rendering::BasicPauseRenderer> ren;
	try
	{
		Rendering::RendererFactory::Create(ren, 1000, ar*1000, " circles test ");
	}
	catch(const std::exception& e)
	{
		cout << "caught exception: " << e.what() << endl;
		return(0);
	}
	catch(...)
	{
		cout << "caught unknown exception" << endl;
		return(0);
	}

	
	ren->Get2dBgCamera()->SetOrthoProjection(0, img.cols, 0, img.rows, -100, 100 );
	
	cout << "starting loop" << endl;
	
	bool paused, advance;
	paused = true;
	advance = false;
	do
	{
		
		img = src->GetCurrent();
		cv::Mat grey;
		cv::cvtColor( img, grey, cv::COLOR_BGR2GRAY );
		
		cout << endl;
		cout << "ic: " << src->GetCurrentFrameID() << endl;
		
		std::vector< cv::KeyPoint > kps;
		GridCircleFinder( grey, 0.1, 0.3, 0.5, kps );
		
		for( unsigned pc = 0; pc < kps.size(); ++pc )
		{	
			cv::circle( img, kps[pc].pt, kps[pc].size, cv::Scalar(0,255,0), 2 );
		}
		
		ren->SetBGImage( img );
		
		
		ren->Step(paused, advance);
		while( paused && !advance )
			ren->Step(paused, advance);
		advance = false;
		
	}
	while( src->Advance() );
}
