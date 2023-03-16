#include "vidsrc.h"

#include <iostream>
using std::cout;
using std::endl;


VideoSource::VideoSource(std::string in_vidPath, std::string in_calPath)
{
	cout << "Creating video source: " << in_vidPath << endl;
	cout << "using calib file: " << in_calPath << endl;
	// try to open the video with opencv...
	auto x = in_vidPath.find(":");
	if( x == std::string::npos )
	{
		// on-disk file source.
		if( !cvvc.open( in_vidPath ) )
		{
			throw std::runtime_error("Could not open video source: " + in_vidPath );
		}
	}
	else
	{
		// string is of the format:
		// usb:<num>
		// ???:??? // for the future!
		if( in_vidPath.find("usb") == 0 )
		{
			std::string s = in_vidPath.substr(4, std::string::npos);
			std::stringstream ss;
			ss << s;
			unsigned id;
			ss >> id;
			
			if( !cvvc.open( id ) )
			{
				throw std::runtime_error("Could not open video source: " + in_vidPath );
			}
		}
	}
		

	// now try to get the calibration...
	if( in_calPath.compare("none") != 0)
	{
		if( !calibration.Read( in_calPath ) )
		{
			throw std::runtime_error("Could not read calibration file: " + in_calPath );
		}
		calPath = in_calPath;
	}
	else
	{
		calPath = in_vidPath + ".calib";
	}

	if( !cvvc.read(current) )
	{
		throw std::runtime_error("Could not read first frame" );
	}

	currentReady = true;

	vidPath = in_vidPath;

	frameIdx = 0;
	numberOfFirstFrame = cvvc.get(cv::CAP_PROP_POS_FRAMES);
	cout << "video has first frame with index: " << numberOfFirstFrame << endl;
	cout << "but don't worry, we compensate so that we're 0 indexed!" << endl;
// 	JumpToFrame(0);	// otherwise we seem to be out of step
}

VideoSource::~VideoSource()
{
	if( cvvc.isOpened() )
		cvvc.release();
}

bool VideoSource::Advance()
{
	
	++frameIdx;
	cvvc.grab();
	
	
	try
	{
		return cvvc.retrieve(current);
	}
	catch(...)
	{
		return false;
	}
	
	
	
}

bool VideoSource::Regress()
{
	if( frameIdx > 0 )
		--frameIdx;
	cvvc.set(cv::CAP_PROP_POS_FRAMES, frameIdx);
	
	try
	{
		return cvvc.retrieve(current);
	}
	catch(...)
	{
		return false;
	}
}

bool VideoSource::JumpToFrame(unsigned frame)
{
	// It seems like the capture frames might be indexed from 1. Very annoying to find this out quite so many years later!
	cvvc.set(cv::CAP_PROP_POS_FRAMES, frame + numberOfFirstFrame);
	frameIdx = frame;
	
	
	try
	{
		return cvvc.retrieve(current);
	}
	catch(...)
	{
		return false;
	}
}

cv::Mat VideoSource::GetCurrent()
{
	return current;
}


