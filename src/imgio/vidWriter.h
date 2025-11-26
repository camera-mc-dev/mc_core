#ifndef MC_DEV_VIDWRITER
#define MC_DEV_VIDWRITER

//
// We've previously used our own fight with the ffmpeg API to write a video - but that's
// too much like hard work.
// So, instead of that, we're going to try and _pipe_ the data to the ffmpeg command which 
// we assume is somewhere on the system.
//


#ifdef HAVE_FFMPEG
extern "C"
{
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h"
	#include "libswscale/swscale.h"
	#include "libavutil/imgutils.h"
}

#include <opencv2/opencv.hpp>
#include <cstdio>


class VidWriter
{
public:
	//
	// Create the VidWriter to write to the specified filename, using the specified codec,
	// for an image of the format typified by typicalImage, and with the expected fps as listed.
	//
	// Bitrate is input as kbps, and 8000 is considered a good place for 1920x1080
	//
	VidWriter( std::string filename, std::string   codecStr, cv::Mat typicalImage, int fps, int crf,  std::string pixfmt);
	VidWriter( std::string filename, std::string encoderStr, cv::Mat typicalImage, int fps );
	
	//
	// Write the next image to the video file.
	//
	void Write( cv::Mat img );
	
	
	//
	// Close the video file
	// (depracated)
	void Finish();
	
	
	~VidWriter();
	
protected:
	
	size_t frameBytes;
	int fnum;
	int fps;
	FILE *outPipe;
};

#else
class VidWriter
{
public:
	//
	// Create the VidWriter to write to the specified filename, using the specified codec,
	// for an image of the format typified by typicalImage, and with the expected fps as listed.
	//
	// Bitrate is input as kbps, and 8000 is considered a good place for 1920x1080
	//
	VidWriter(std::string filename, std::string codecStr, cv::Mat typicalImage, int fps, int crf = 18)
	{
		throw std::runtime_error("Not compiled with ffmpeg, so no vid writer.");
	}

	//
	// Write the next image to the video file.
	//
	void Write(cv::Mat img)
	{
		throw std::runtime_error("Not compiled with ffmpeg, so no vid writer.");
	}


	//
	// Close the video file
	//
	void Finish()
	{
		throw std::runtime_error("Not compiled with ffmpeg, so no vid writer.");
	}


	~VidWriter()
	{

	}

protected:


};
#endif


#endif
