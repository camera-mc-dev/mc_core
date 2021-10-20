#include "imgio/vidWriter.h"
#include "imgio/imagesource.h"

#include <iostream>
using std::cout;
using std::endl;

int main(int argc, char* argv[])
{
	if( argc != 4 )
	{
		cout << "Convert image directory to h265 .mp4 video: " << endl;
		cout << "Usage: " << endl;
		cout << argv[0] << " <input dir> <output vid> <fps>" << endl;
		exit(0);
	}
	
	ImageDirectory src(argv[1]);
	int fps = atoi(argv[3]);
	
	
	cv::Mat tmp = src.GetCurrent();
	VidWriter vo(argv[2], "h265", tmp, fps, fps*1024);
	
	do
	{
		
		cv::Mat tmp = src.GetCurrent();
		vo.Write( tmp );
		
	}while( src.Advance() );
	
	vo.Finish();
	
}
