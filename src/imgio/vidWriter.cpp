#include "imgio/vidWriter.h"
#include <boost/algorithm/string.hpp>

#ifdef HAVE_FFMPEG

#include <iostream>
using std::cout;
using std::endl;

VidWriter::VidWriter( std::string filename, std::string codecStr, cv::Mat typicalImage, int in_fps, int in_crf)
{
	av_register_all();
	avcodec_register_all();
	int err;
	
	fps = in_fps;
	
	//
	// Guess the output format.
	//
	ofmt = av_guess_format(NULL, filename.c_str(), NULL);
	if (ofmt == NULL)
	{
		throw std::runtime_error("can't guess output format" );
	}
	
	//
	// But change the codec to the one requested.
	//
	//
	//
	// Try to find the desired codec.
	//
	boost::algorithm::to_lower(codecStr);
	if( codecStr.compare("h264") == 0 )
	{
		ofmt->video_codec = AV_CODEC_ID_H264;
	}
	else if( codecStr.compare("h265") == 0 )
	{
		ofmt->video_codec = AV_CODEC_ID_H265;
	}
	else if( codecStr.compare("vp9") == 0 )
	{
		ofmt->video_codec = AV_CODEC_ID_VP9;
	}
	else
	{
		throw std::runtime_error("Unknown codec for vidWriter: " + codecStr );
	}
	
	
	//
	// Create an output context. 
	//
	std::stringstream ss;
	ss << filename << ".tmp." << codecStr;
	std::string tmpFile = ss.str();
	err = avformat_alloc_output_context2(&fctx, ofmt, NULL, filename.c_str());
	if( err < 0 )
	{
		// probably need to do some cleanup...
		throw std::runtime_error("Failed to allocate output context");
	}
	
	
	//
	// Find the encoder.
	//
	if( ofmt->video_codec == AV_CODEC_ID_H265 )
	{
		codec = avcodec_find_encoder_by_name("hevc");
		
	}
	
	if( !codec )
		codec = avcodec_find_encoder(ofmt->video_codec);
	if(!codec)
	{
		throw std::runtime_error("codec not found: " + codecStr );
	}

    cout << "Created codec: " << endl;
    cout << "\tshort name : " << codec->name << endl;
    cout << "\tlong name  : " << codec->long_name << endl;
	
	
	//
	// Create the output stream.
	//
	videoStream = avformat_new_stream(fctx, codec);
	if (!videoStream)
	{
		throw std::runtime_error("Failed to create video stream");
	}
	
	
	//
	// Create codec context.
	//
	cctx = avcodec_alloc_context3(codec);
	
	
	
	//
	// Do some configuration.
	//
	videoStream->codecpar->codec_id = ofmt->video_codec;
	videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	videoStream->codecpar->width = typicalImage.cols;
	videoStream->codecpar->height = typicalImage.rows;
	videoStream->codecpar->format = AV_PIX_FMT_YUV444P;
// 	videoStream->time_base = av_make_q( 1, fps );			// turns out to be set by codec or muxer or something.
	
	avcodec_parameters_to_context(cctx, videoStream->codecpar);
	cctx->time_base = av_make_q( 1, fps );
	cctx->max_b_frames = 4;     // saw these numbers recommended somewhere.
	cctx->gop_size = 10 * fps;  // ""    ""    ""         ""        ""
	cctx->sample_rate = fps;
	AVDictionary *codecOptions( 0 );
	std::stringstream crfss; crfss << in_crf;
	if (videoStream->codecpar->codec_id == AV_CODEC_ID_H264)
	{
		av_dict_set(&codecOptions, "crf", crfss.str().c_str(), 0); // default is 18
		av_dict_set(&codecOptions, "preset", "medium", 0);
	}
	if (videoStream->codecpar->codec_id == AV_CODEC_ID_H265)
	{
		av_dict_set(&codecOptions, "crf", crfss.str().c_str(), 0); // default is 18
		av_dict_set(&codecOptions, "preset", "medium", 0);
	}
	if (videoStream->codecpar->codec_id == AV_CODEC_ID_VP9)
	{
		// according to the ffmpeg guide on VP9, VP9 likes a two pass encoding.
		// this however should set us up for a constant quality single pass.
		// actually, forget about that, it is far far far too slow.
		av_dict_set(&codecOptions, crfss.str().c_str(), "30", 0); // recommended is 31 for 1080p
		av_dict_set(&codecOptions, "deadline", "good", 0);
		av_dict_set(&codecOptions, "row-mt", "1", 0);
		videoStream->codecpar->bit_rate = 0;
	}
// 	if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
// 	{
// 		cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
// 	}
	avcodec_parameters_from_context(videoStream->codecpar, cctx);
	
// 	av_opt_set_int( cctx, "r", fps, 0 );
	
	//
	// Open the codec?
	//
	err = avcodec_open2(cctx, codec, &codecOptions);
	if( err < 0)
	{
		throw std::runtime_error("Failed to open codec.");
	}
	
	
	//
	// Open the output file.
	//
	err = avio_open(&fctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
	if( err < 0 )
	{
		throw std::runtime_error("Failed to open output file.");
	}
	
	
	//
	// Write the header.
	//
	err = avformat_write_header(fctx, NULL);
	if( err < 0 )
	{
		throw std::runtime_error("Failed to write header.");
	}
	
	
	av_dump_format(fctx, 0, filename.c_str(), 1);
	
	
	frame = av_frame_alloc();
	frame->height = cctx->height;
	frame->width  = cctx->width;
	frame->format = cctx->pix_fmt;
	err = av_frame_get_buffer(frame, 24);
	if( err < 0 )
	{
		throw std::runtime_error("Failed to allocate picture.");
	}
	
	fnum = 0;
	
	
	swsCtx = sws_getContext(
	                          cctx->width,
	                          cctx->height,
	                          AV_PIX_FMT_BGR24,
	                          cctx->width, cctx->height,
	                          AV_PIX_FMT_YUV444P,
	                          SWS_BICUBIC,
	                          0, 0, 0
	                       );
	
	
	
	//
	// Allocate my two frames.
	//
	bgrFrame = av_frame_alloc();
	frame    = av_frame_alloc();
	
	bgrFrame->width = cctx->width;
	bgrFrame->height = cctx->height;
	bgrFrame->format = AV_PIX_FMT_BGR24;
	
	frame->width = cctx->width;
	frame->height = cctx->height;
	frame->format = AV_PIX_FMT_YUV444P;
	
	av_frame_make_writable(bgrFrame);
	av_frame_make_writable(frame);
	
	yuvSize = av_image_get_buffer_size(AV_PIX_FMT_YUV444P, cctx->width, cctx->height, 1);
	yuvBuffer = (uint8_t *)av_malloc(yuvSize*sizeof(uint8_t));
	avpicture_fill((AVPicture *)frame, yuvBuffer, AV_PIX_FMT_YUV444P,  cctx->width, cctx->height);
	
	cout << "cctx tb        : " << cctx->time_base.num << " " << cctx->time_base.den << endl;
	cout << "stream tb      : " << videoStream->time_base.num << " " << videoStream->time_base.den << endl;
	cout << "stream codec tb: " << videoStream->codec->time_base.num << " " << videoStream->codec->time_base.den << endl;
}


void VidWriter::Write( cv::Mat img )
{
	int err;
	
	// need to convert to YUV first. Probably best to use ffmpeg for that 
	// given reports I've had about OpenCVs conversion.
	int inLinesize[1] = { 3 * cctx->width };
	
	avpicture_fill( (AVPicture *)bgrFrame, img.data, AV_PIX_FMT_BGR24, img.cols, img.rows);
	
	sws_scale(
	            swsCtx,
	            bgrFrame->data,
	            inLinesize,
	            0,
	            cctx->height,
	            frame->data,
	            frame->linesize
	         );
	
	
	// we've set the codec context timebase to 1/fps
	// stream context seems to be fixed at 1/12800
	//
	frame->pts = fnum * (videoStream->time_base.den / cctx->time_base.den);
	
// 	cout << bgrFrame->format << " " << bgrFrame->width << " " << bgrFrame->height << endl;
// 	cout << frame->format << " " << frame->width << " " << frame->height << endl;
	
	err = avcodec_send_frame(cctx, frame);
	if ( err < 0)
	{
		throw std::runtime_error("Failed to send frame.");
	}
	
	
	AVPacket *pkt = av_packet_alloc(); // also inits to defaults
	if (pkt == NULL)
	{
		throw std::runtime_error("Failed to allocate packet.");
	}
	
	bool done = false;
	while(!done)
	{
		err = avcodec_receive_packet(cctx, pkt);
		if (err == 0) 
		{
			//av_packet_rescale_ts(pkt, cctx->time_base, videoStream->time_base);
    		pkt->stream_index = videoStream->index;
			// pkt->flags |= AV_PKT_FLAG_KEY;
			av_interleaved_write_frame(fctx, pkt);
			av_packet_unref(pkt);
		}
		else if( err == AVERROR(EAGAIN) || err == AVERROR_EOF )
		{
			done = true;
		}
		else if( err < 0 )
		{
			throw std::runtime_error("Error during frame write");
		}
	}
	
	++fnum;
}

void VidWriter::Finish()
{
	int err; 
	
	AVPacket *pkt = av_packet_alloc(); // also inits to defaults
	if (pkt == NULL)
	{
		throw std::runtime_error("Failed to allocate packet.");
	}
	
	
	
	bool done = false;
	avcodec_send_frame(cctx, NULL);
	while(!done)
	{
		err = avcodec_receive_packet(cctx, pkt);
		if (err == 0) 
		{
			av_interleaved_write_frame(fctx, pkt);
			av_packet_unref(pkt);
		}
		else
		{
			done = true;
		}
	}
	
	
	av_write_trailer(fctx);
	err = avio_close(fctx->pb);
	if (err < 0)
	{
		throw( std::runtime_error("Failed to close file") );
	}
	
	av_frame_free(&frame);
	avcodec_free_context(&cctx);
	avformat_free_context(fctx);
	sws_freeContext(swsCtx);
}



VidWriter::~VidWriter()
{
}
#endif