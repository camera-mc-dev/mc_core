#ifndef MC_DEV_VIDWRITER
#define MC_DEV_VIDWRITER

//
// There is a sound argument that I should just use OpenCV's video writer,
// but it doesn't give very much control over bit rates and codecs, which I might want,
// and there's at least one comment that some part might only work on Windows.
// F that Sh.
//
#include <cv.hpp>

#ifdef HAVE_FFMPEG
extern "C"
{
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h"
	#include "libswscale/swscale.h"
	#include "libavutil/imgutils.h"
}

class VidWriter
{
public:
	//
	// Create the VidWriter to write to the specified filename, using the specified codec,
	// for an image of the format typified by typicalImage, and with the expected fps as listed.
	//
	// Bitrate is input as kbps, and 8000 is considered a good place for 1920x1080
	//
	VidWriter( std::string filename, std::string codecStr, cv::Mat typicalImage, int fps, int crf = 18 );
	
	//
	// Write the next image to the video file.
	//
	void Write( cv::Mat img );
	
	
	//
	// Close the video file
	//
	void Finish();
	
	
	~VidWriter();
	
protected:
	
	AVFormatContext *fctx;
	AVOutputFormat  *ofmt;
	
	AVCodec *codec;
	AVCodecContext *cctx;
	
	AVStream *videoStream;
	
	SwsContext *swsCtx;
	
	AVFrame *frame;
	AVFrame *bgrFrame;
	
	int yuvSize;
	uint8_t* yuvBuffer;
	
	int fnum;
	int fps;
	FILE *outfi;
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
