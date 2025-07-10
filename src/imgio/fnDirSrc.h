#ifndef MC_FNIMGSRC_H
#define MC_FNIMGSRC_H

#include "imagesource.h"
#include <map>

#include <iostream>
using std::cout;
using std::endl;

class FNImageDirectory : public ImageSource
{
protected:
	
	std::string info;
	std::string prefix;
	std::string path;
	
	std::map< int, std::string > imgMap;
	
	int maxFrame;
	int minFrame;
	int firstFrame;
	
	
public:
	FNImageDirectory( std::string in_info )
	{
		cout << "creating fnDirSrc from:" << in_info << endl;
		this->info = in_info;
		SplitInfo();
		GetImgMap();
		frameIdx = 0;
		calibration.Read( path + "/calibFile");
		JumpToFrame(0); // should we jump to minFrame?
	}
	
	FNImageDirectory( std::string in_info, std::string in_calibPath )
	{
		cout << "creating fnDirSrc from:" << in_info << endl;
		this->info = in_info;
		SplitInfo();
		GetImgMap();
		frameIdx = 0;
		calibration.Read( in_calibPath );
		JumpToFrame(0); // should we jump to minFrame?
	}
	
	~FNImageDirectory()
	{
		
	}
	
	
	void SplitInfo()
	{
		//
		// The info string can have one of the following formats:
		//   - <path>
		//   - <path>:<tag>
		//
		// In the second case, the <tag> can have the format:
		//   - <first frame>
		//   - <first frame>:<prefix>
		//
		// As you provide a directory of images with their frame numbers in
		// the filename, it might be the case that those frame numbers don't start at 0.
		// 
		// The source always starts at frame 0, so when you use the source, you might 
		// find that you have to go through 10k frames before you get to the first real image.
		// (if that's how your images are numbered). So, you can instead specify where the 
		// first frame is.
		// 
		// This means that when you use the source and ask from frame 0, you get frame n instead.
		// when you ask for frame 1, you get n+1 instead.
		// when you ask for frame x, you get n+x. Got it?
		//
		int n = 0;
		for( unsigned c = 0; c < info.length(); ++c )
		{
			if( info[c] == ':' )
				++n;
		}
		if( n == 0 )
		{
			firstFrame = 0;
			path = info;
		}
		else if( n == 1 )
		{
			// path:firstFrame
			int a = info.find(";");
			path = std::string( info.begin(), info.begin()+a );
			firstFrame = atoi( std::string( info.begin()+a+1, info.end() ).c_str() );
			prefix = "";
		}
		else if( n == 2 )
		{
			// path:firstFrame:prefix
			int a = info.find(":");
			int b = info.rfind(":");
			path  = std::string( info.begin(), info.begin()+a );
			firstFrame = atoi( std::string( info.begin()+a+1, info.begin()+b ).c_str() );
			prefix = std::string( info.begin() + b + 1, info.end() );
		}
		else
		{
			throw std::runtime_error("fnDirSrc needs path:firstframe or path:firstframe:prefix");
		}
	}
	
	void GetImgMap()
	{
		cout << "\t getting image map: " << endl;
		// find the images in the directory.
		boost::filesystem::path p(path);
		std::vector< std::string > imageList;
		if( boost::filesystem::exists(p) && boost::filesystem::is_directory(p))
		{
			boost::filesystem::directory_iterator di(p), endi;
			for( ; di != endi; ++di )
			{
				std::string s = di->path().string();
				
				if( IsImage(s) )
				{
					imageList.push_back(s);
				}
			}
		}
		else
		{
			throw std::runtime_error("Could not find image source directory.");
		}
		cout << "\t" << imageList.size() << " images " << endl;
		//
		// Now we need to process that image list so that we can find the frame number for each image.
		//
		maxFrame = 0;
		minFrame = 9999999999;
		for( unsigned lc = 0; lc < imageList.size(); ++lc )
		{
			int ic = ParseFilename( imageList[lc] );
			maxFrame = std::max( ic, maxFrame );
			minFrame = std::min( ic, minFrame );
			
			imgMap[ic] = imageList[lc];
		}
		
		minFrame = std::max( minFrame, firstFrame );
		cout << path << " : " << firstFrame << " " << minFrame << endl;
	}
	
	int ParseFilename( std::string fn )
	{
		//
		// For now, we assume that the filename is <prefix><number>.ext
		//
		// In the future, we should allow for other formats, e.g. : <timecode>-<frame>.ext
		// or i-felt-i-needed-a-prefix-<number>-and-a-postfix.ext
		//
		boost::filesystem::path p(fn);
		std::string stemStr = p.stem().string();
		std::string numStr  = std::string( stemStr.begin() + prefix.length(), stemStr.end() );
		//cout << fn << " " << stemStr << " " << numStr << endl; exit(0);
		return atoi(numStr.c_str());
	}
	
	
	cv::Mat GetCurrent()
	{
		// return the current image.
		return current;
	}
	
	bool Advance()
	{
		if( frameIdx <= maxFrame )
		{
			++frameIdx;
			ReadImage();
			return true;
		}
		else
		{
			return false;
		}
	}
	
	bool Regress()
	{
		if( frameIdx > 0 )
		{
			--frameIdx;
			return true;
		}
		else
		{
			return false;
		}
	}
	
	virtual bool JumpToFrame(unsigned frame)
	{
		if( frame >= 0 && frame <= maxFrame )
		{
			frameIdx = frame;
			ReadImage();
			return true;
		}
		else
		{
			return false;
		}
	}
	
	
	int GetNumImages()
	{
		return maxFrame - minFrame;
	}
	
	unsigned GetCurrentFrameID()
	{
		return frameIdx;
	}
	
	frameTime_t GetCurrentFrameTime()
	{
		return frameTime;
	}
	
	bool ReadImage( )
	{
		int realFrameNo = frameIdx;
		if( firstFrame > -1 )
		{
			realFrameNo = frameIdx + firstFrame;
		}
		auto i = imgMap.find( realFrameNo );
		if( i != imgMap.end() )
		{
			current = LoadImage( i->second );
		}
		else
		{
			cout << "source didn't have " << frameIdx << " (" << realFrameNo << ")" << endl;
			cout << "so using: " << imgMap.begin()->first << endl;
			// we need an image of a sensible size... load the first image and blank it.
			// really we could remember what size image and just make a blank image, but
			// will it matter?
			current = LoadImage( imgMap.begin()->second );
			current = cv::Mat( current.rows, current.cols, current.type(), cv::Scalar(0) );
		}
		cout << "frameIdx: " << frameIdx << " (" << realFrameNo << ")" << " : " << current.rows << " " << current.cols << endl;
		
		return true;
	}
	
	
	// WARNING!!!
	// replaces the calibration file in the source directory!
	void SaveCalibration()
	{
		calibration.Write( path + "/calibFile" );
	}



private:

	bool IsImage(string s)
	{
		unsigned end = s.size();
		if( s.find(".jpg") == end-4  ||
		    s.find(".jpeg") == end-5 ||
			s.find(".png") == end-4  ||
			s.find(".bmp") == end-4  ||
			s.find(".tiff") == end-5 ||
			s.find(".tif") == end-4 ||
		    s.find(".charImg") == end-8 ||
			s.find(".floatImg") == end-9
			)
		{
			return true;
		}
		return false;
	}
	
	unsigned frameIdx;
	frameTime_t frameTime;
	
	// the current image.
	cv::Mat current;
};
#endif
