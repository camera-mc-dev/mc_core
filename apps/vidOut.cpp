#include "imgio/vidWriter.h"
#include "imgio/imagesource.h"

#include <iostream>
using std::cout;
using std::endl;

int main(int argc, char* argv[])
{
	if( argc != 4 && argc != 5 && argc != 7 )
	{
		cout << "Convert image directory to video: " << endl;
		cout << endl;
		cout << "This is basically a wrapper around ffmpeg." << endl;
		cout << "We use our VidWriter class which is now piping " << endl;
		cout << "images to the ffmpeg executable, rather than " << endl;
		cout << "trying to use the ceaselessly opaque API" << endl;
		cout << endl;
		cout << "Usage: " << endl;
		cout << "\t - Basic: " << endl;
		cout << "\t  " << argv[0] << " <input dir> <output vid> <fps>" << endl;
		cout << "\t  writes to an h264 mp4 with yuv420p and crf of 18" << endl;
		cout << endl;
		cout << "\t - Advanced: " << endl;
		cout << "\t  " << argv[0] << " <input dir> <output vid> <fps> <h265 | h264 | vp9 > <pixfmt> <crf>" << endl;
		cout << "\t  choice of a few high quality encoders, set the pixel format and crf yourself. " << endl;
		cout << "\t  note that each codec has a slightly different value for crf: " << endl;
		cout << "\t  - h265: 0 (high quality) -> 51 (small file): default 26." << endl;
		cout << "\t  - h264: 0 (high quality) -> 51 (small file). Default 23. 18 said to be 'visually lossles'. " << endl;
		cout << "\t  -  vp9: 4 (high quality) -> 63 (small file): Recommend 15 -> 30. " << endl;
		cout << "\t  here are a few pixfmts, see ffmpeg docs for more:" << endl;
		cout << "\t  - yuv420p: safe, works in all video players, lowers colour resolution" << endl;
		cout << "\t  - yuv444p: better colour resolution, but video players tend to not like it" << endl;
		cout << "\t  - rgb8   : try if you want, tends to be inferior for lossy compression" << endl;
		cout << endl;
		cout << "\t - Very Advanced: " << endl;
		cout << "\t  " << argv[0] << " <input dir> <output vid> <fps> \"<encoder string>\"" << endl;
		cout << "\t  set the <encoder string> and it will be passed verbatim to ffmpeg" << endl;
		cout << "\t  make sure <encoder string> is enclosed in quotes \" . \" to pass as a single argument" << endl;
		cout << endl;
		exit(0);
	}
	
	cout << argc << endl;
	
	ImageDirectory src(argv[1]);
	int fps = atoi(argv[3]);
	
	std::shared_ptr< VidWriter > vo;
	cv::Mat tmp = src.GetCurrent();
	if( argc == 4 )
	{
		vo.reset( new VidWriter( argv[2], "h265", tmp, atoi( argv[3] ), 18, "yuv420p" ) );
	}
	else if( argc == 7 )
	{
		vo.reset( new VidWriter( argv[2], argv[4], tmp, atoi( argv[3] ), atoi( argv[6] ), argv[5] ) );
	}
	else if( argc == 5 )
	{
		vo.reset( new VidWriter( argv[2], argv[4], tmp, atoi( argv[3] ) ) );
	}
	
	do
	{
		cv::Mat tmp0 = src.GetCurrent();
		cv::Mat tmp1;
		if( tmp0.type() == CV_32FC3 )
		{
			tmp0.convertTo( tmp1, CV_8UC3, 255.0f );
		}
		else if( tmp0.type() == CV_32FC1 )
		{
			tmp0.convertTo( tmp1, CV_8UC1, 255.0f );
		}
		else if( tmp0.type() == CV_8UC1 || tmp0.type() == CV_8UC3 )
		{
			tmp1 = tmp0;
		}
		else
		{
			cout << "Image source provided images in a format we can't send to video" << endl;
			exit(0);
		}
		
		vo->Write( tmp1 );
	}
	while( src.Advance() );
	
	
}
