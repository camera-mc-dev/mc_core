#include "imgio/vidWriter.h"
#include <boost/algorithm/string.hpp>
#include "commonConfig/commonConfig.h"


#include <sstream>
#include <iostream>
using std::cout;
using std::endl;

VidWriter::VidWriter( std::string filename, std::string codecStr, cv::Mat typicalImage, int in_fps, int in_crf, std::string pixfmt)
{
	// build up our ffmpeg command into a stringstream
	std::stringstream ss;
	
	// start with the basics - the actual ffmpeg executable.
	CommonConfig ccfg;
	ss << ccfg.ffmpegPath << " "; 
	
	// we'll just go with overwriting existing files rather than have to handle a failed ffmpeg open.
	ss << "-y ";
	
	// now we specify the codec for the input
	ss << "-f rawvideo -vcodec rawvideo ";
	
	// now the input shape and format.
	ss << "-s " << typicalImage.cols << "x" << typicalImage.rows << " ";
	
	if( typicalImage.type() == CV_8UC3 )
	{
		frameBytes = typicalImage.rows * typicalImage.cols * 3;
		ss << "-pix_fmt bgr24 ";
	}
	else if( typicalImage.type() == CV_8UC1 )
	{
		frameBytes = typicalImage.rows * typicalImage.cols * 1;
		ss << "-pix_fmt gray8 ";
	}
	else
	{
		throw std::runtime_error("Vid writer expects typical image to be CV_8UC1 or CV_8UC3");
	}
	
	// input framerate.
	fps = in_fps;
	ss << "-r " << fps;
	
	// and that the input comes from stdin
	ss << " -i - ";
	
	
	// now we specify the output codec.
	// we'll allow a few common shortcuts.
	std::string cstring;
	ss << " -c:v " ;
	boost::algorithm::to_lower(codecStr);
	if( codecStr.compare("h264") == 0 )
	{
		cstring = "libx264";
	}
	else if( codecStr.compare("h265") == 0 )
	{
		cstring = "libx265";
	}
	else if( codecStr.compare("vp9") == 0 )
	{
		cstring = "libvpx-vp9";
	}
	else
	{
		cstring = codecStr;
	}
	
	ss << cstring << " ";
	
	// pixel format. For BioCV data we used yuv444p and got better compression that yuv420p, 
	// but many media players don't like that so the default is now the less good yuv420p.
	ss << " -pix_fmt " << pixfmt << " ";
	
	// the crf quality - only really valid for x264, x265 and vpn
	ss << "-crf " << in_crf << " ";
	
	// finally, the output file
	ss << filename;
	
	cout << ss.str() << endl;
	
	
	// open the pipe...
	if( !(outPipe = popen(ss.str().c_str(), "w")) )
	{
		cout << "popen error" << endl;
		exit(1);
	}
	
	

}

VidWriter::VidWriter( std::string filename, std::string encoderStr, cv::Mat typicalImage, int in_fps )
{
		// build up our ffmpeg command into a stringstream
	std::stringstream ss;
	
	// start with the basics - the actual ffmpeg executable.
	CommonConfig ccfg;
	ss << ccfg.ffmpegPath << " ";
	
	// we'll just go with overwriting existing files rather than have to handle a failed ffmpeg open.
	ss << "-y ";
	
	// now we specify the codec for the input
	ss << "-f rawvideo -vcodec rawvideo ";
	
	// now the input shape and format.
	ss << "-s " << typicalImage.cols << "x" << typicalImage.rows << " ";
	
	if( typicalImage.type() == CV_8UC3 )
	{
		frameBytes = typicalImage.rows * typicalImage.cols * 3;
		ss << "-pix_fmt bgr24 ";
	}
	else if( typicalImage.type() == CV_8UC1 )
	{
		frameBytes = typicalImage.rows * typicalImage.cols * 1;
		ss << "-pix_fmt gray8 ";
	}
	else
	{
		throw std::runtime_error("Vid writer expects typical image to be CV_8UC1 or CV_8UC3");
	}
	
	// input framerate.
	fps = in_fps;
	ss << "-r " << fps;
	
	// and that the input comes from stdin
	ss << " -i - ";
	
	
	// the encoding options are passed verbatim from the user's string.
	ss << encoderStr << " ";
	
	// finally, the output file
	ss << filename;
	
	cout << ss.str() << endl;
	
	
	// open the pipe...
	if( !(outPipe = popen(ss.str().c_str(), "w")) )
	{
		cout << "popen error" << endl;
		exit(1);
	}
}


void VidWriter::Write( cv::Mat img )
{
	fwrite( img.data, 1, frameBytes, outPipe );
	++fnum;
}

void VidWriter::Finish()
{

}



VidWriter::~VidWriter()
{
	fflush(outPipe);
	fclose(outPipe);
// 	delete outPipe;
}
