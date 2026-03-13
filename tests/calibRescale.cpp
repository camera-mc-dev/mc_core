#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
using std::cout;
using std::endl;
using std::vector;

#include "imgio/sourceFactory.h"
#include "calib/calibration.h"

#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "commonConfig/commonConfig.h"

#include <boost/filesystem.hpp>

#include <sstream>

int main(int argc, char* argv[])
{
	if( argc != 2 )
	{
		cout << "Test calibration.RescaleImage()" << endl;
		cout << "Handles either image or video sources." << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <source > ";
		cout << endl << endl;
		exit(1);
	}
	
	std::string cfn = std::string(argv[1]) + std::string(".calib");
	std::shared_ptr< ImageSource > source;
	if( boost::filesystem::exists( cfn ) )
	{
		auto sp = CreateSource( argv[1], cfn );
		source = sp.source;
	}
	else
	{
		auto sp = CreateSource( argv[1] );
		source = sp.source;
	}
	
	
	CommonConfig ccfg;
	
	cv::Mat img = source->GetCurrent();
	
	float downscaleEndIter = 20000;
	float finalDownscale   = 1;
	float initialDownscale = 8;
	for( unsigned iterCount = 0; iterCount < 50000; ++iterCount )
	{
		float currentDownscale = finalDownscale + (initialDownscale - finalDownscale) * pow( cos( (1.57f*iterCount)/downscaleEndIter ), 2.0f ); //std::exp( -(3.0f * iterCount)/(float)downscaleEndIter );
		
		float scale = 1.0f / currentDownscale;
		if( iterCount >= downscaleEndIter )
			scale = 1.0f;
		
		
		Calibration c = source->GetCalibration();
		c.RescaleImage( scale );
		
		
		
		
		cv::Mat small;
		cv::resize(img, small, cv::Size( c.width, c.height), cv::INTER_AREA );
		
		
		std::stringstream ss;
		ss << "/tmp/rescale-" << std::setw(8) << std::setfill('0') << iterCount << "-" << std::setw(6) << std::setprecision(5) << std::fixed << currentDownscale << ".jpg";
		SaveImage( small, ss.str() );
		
		cout << iterCount << endl;
		cout << "scale: " << scale << endl;
		cout << "image size: " << small.cols << " " << small.rows << endl;
		cout << "calib size: " << c.width << " " << c.height << endl;
		cout << "K: " << endl;
		cout << c.K << endl;
		cout << " ---- " << endl << endl;
	}
}
