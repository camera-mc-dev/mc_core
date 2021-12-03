#ifndef MC_DEV_HDF5UTILS
#define MC_DEV_HDF5UTILS

#ifdef HAVE_HIGH_FIVE
#include <highfive/H5File.hpp>
#include "imgio/imagesource.h"

#include <map>

//
// Two things we want to do.
//


//
// First is a small class for writing data to hdf5 files.
//
class HDF5ImageWriter
{
public:
	HDF5ImageWriter( std::string outfn );
	~HDF5ImageWriter();
	
	
	void AddImage( cv::Mat &img, size_t imgNumber );
	
	void Flush();
	
protected:
	
	HighFive::File* outfi;


};



//
// Second is an image source for reading from hdf5 files.
//
class HDF5Source : public ImageSource
{
public:
	HDF5Source( std::string in_filepath, std::string in_calibPath );
	~HDF5Source();
	
	cv::Mat GetCurrent();
	bool Advance();
	bool Regress();
	
	unsigned GetCurrentFrameID();
	frameTime_t GetCurrentFrameTime();
	int GetNumImages();
	bool JumpToFrame(unsigned frame);
	
	void SaveCalibration()
	{
		if( calibPath.compare("none") != 0 )
			calibration.Write( calibPath );
	}
	
protected:
	
	
	HighFive::File* infi;
	
	std::vector< std::string > dsList;
	std::map<unsigned, unsigned> idx2fno, fno2idx;
	int dsIdx;
	
	void FindImage();
	void GetImage();
	cv::Mat current;
	
	int maxFrame, minFrame;
	
	
	//
	// We operate on the approach that we can have synchronised images sources,
	// and we have established the standard that frame n from camera m is the same 
	// as frame n from camera k. 
	//
	// To make this work, we need the sources to step by one frame.
	//
	// The nature of a source like this, like the frameNumberImageSource is that
	// we might have gaps between frames in the source - so stepping to the "next"
	// frame on one source might skip a frameNumber, but on another source it wont.
	//
	// To fix this, we have a monotonically increasing frame count, and if the 
	// frame doesn't exist on this source, it simply returns a black image.
	//
	int currentFrameNo;
	
	std::string calibPath;
};

#endif
#endif
