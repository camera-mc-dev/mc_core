#include "imgio/loadsave.h"

#include <iostream>
using std::cout;
using std::endl;

int main( int argc, char* argv[] )
{
	if( argc != 3 )
	{
		cout << "Convert the format of an image, particularly .floatImg to other formats: " << endl;
		cout << argv[0] << " <input image> <output image>" << endl;
		return 1;
	}

	cv::Mat img, out;
	img = MCDLoadImage( argv[1] );

	if( std::string(argv[2]).find(".floatImg") == std::string::npos )
	{

		if( img.type() == CV_32FC3 )
		{
			img *= 255.0f;
			img.convertTo( img, CV_8UC3 );
		}
		if( img.type() == CV_32FC1 )
		{
			img *= 255.0f;
			img.convertTo( img, CV_8UC1 );
		}


	}
	SaveImage( img, argv[2] );
	return 0;
}
