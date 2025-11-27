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
#undef small
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
	
	for( float scale = 1.0; scale > 0.0; scale -= 0.05 )
	{
		cout << "scale: " << scale << endl;
		
		Calibration c = source->GetCalibration();
		c.RescaleImage( scale );
		cout << "calib size: " << c.width << " " << c.height << endl;
		cout << "K: " << endl;
		cout << c.K << endl;
		cout << " ---- " << endl << endl;
		
		cv::Mat small;
		cv::resize(img, small, cv::Size( c.width, c.height), cv::INTER_AREA );
		cout << "image size: " << small.cols << " " << small.rows << endl;
		
		std::stringstream ss;
		ss << "/tmp/rescale-" << std::setw(6) << std::setprecision(5) << std::fixed << scale << ".jpg";
		MCDSaveImage( small, ss.str() );
	}
}
