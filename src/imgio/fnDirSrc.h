#include "imagesource.h"
#include <map>

#include <iostream>
using std::cout;
using std::endl;

class FNImageDirectory : public ImageSource
{
protected:
	
	std::string info;
	std::string format;
	std::string path;
	
	std::map< int, std::string > imgMap;
	
	int maxFrame;
	int minFrame;
	
	
public:
	FNImageDirectory( std::string in_info )
	{
		this->info = in_info;
		SplitInfo();
		GetImgMap();
		frameIdx = 0;
		calibration.Read( path + "/calibFile");
		JumpToFrame(0); // should we jump to minFrame?
	}
	
	FNImageDirectory( std::string in_info, std::string in_calibPath )
	{
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
		//   - <tag>:<path>
		//
		// In the second case, the <tag> can be used to specify the format of the image filenames. In the first case, the format
		// is assumed to be <frameNumber>.ext
		//
		
		int a = info.find(":");
		if( a == std::string::npos )
		{
			path = info;
		}
		else
		{
			cout << "The FNImageDirectory source does not currently support specifying the format of the image filename." << endl;
			cout << "You must for now use <frameNumber>.ext as the format. Sorry!" << endl;
			throw std::runtime_error("Pending feature not done yet!");
		}
		
	}
	
	void GetImgMap()
	{
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
	}
	
	int ParseFilename( std::string fn )
	{
		//
		// For now, we assume that the filename is <number>.ext
		//
		// In the future, we should allow for other formats, e.g. : <timecode>-<frame>.ext
		// or i-felt-i-needed-a-prefix-<number>-and-a-postfix.ext
		//
		boost::filesystem::path p(fn);
		std::string numStr = p.stem().string();
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
		auto i = imgMap.find( frameIdx );
		if( i != imgMap.end() )
		{
			current = LoadImage( i->second );
		}
		else
		{
			cout << "source didn't have " << frameIdx << endl;
			cout << "so using: " << imgMap.begin()->first << endl;
			// we need an image of a sensible size... load the first image and blank it.
			// really we could remember what size image and just make a blank image, but
			// will it matter?
			current = LoadImage( imgMap.begin()->second );
			current = cv::Mat( current.rows, current.cols, current.type(), cv::Scalar(0) );
		}
		cout << "frameIdx: " << frameIdx << " : " << current.rows << " " << current.cols << endl;
		
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
