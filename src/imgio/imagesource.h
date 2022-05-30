#ifndef ME_IMAGE_SOURCE
#define ME_IMAGE_SOURCE

#include "calib/calibration.h"
#include "misc/types.h"

#include <cv.hpp>
#include <opencv2/highgui/highgui.hpp> // only keep this line until we get up to openCV 3.0!

#include <string>
using std::string;

#include <stdexcept>
#include <boost/filesystem.hpp>

#include <thread>
#include <condition_variable>
#include <random>

#include "imgio/loadsave.h"

#include <random>

// and ImageSource is the basic root class for something that provides images.
// It could be a camera, or it could be a directory of images, or any number
// of other things :)
class ImageSource
{
public:
	ImageSource(){};
	virtual ~ImageSource(){};

	// get the current image as a cv::Mat
	virtual cv::Mat GetCurrent()=0;

	// advance the image source to the next image.
	// returns false on failure
	virtual bool Advance()=0;

	// regress to the previous image if possible.
	// returns false if not possible.
	virtual bool Regress()=0;

	virtual unsigned  GetCurrentFrameID()=0;
	virtual frameTime_t GetCurrentFrameTime()=0;
	
	virtual bool JumpToFrame(unsigned frame)=0;

	// if the source is finite, return how many images
	// are in the source. If it is not finite (e.g. a camera)
	// return a negative value.
	virtual int GetNumImages()=0;

	Calibration& GetCalibration()
	{
		return calibration;
	}

	void SetCalibration( cv::Mat &K, std::vector<float> &k, transMatrix3D &L)
	{
		transMatrix2D tK;
		tK << K.at<double>(0,0), K.at<double>(0,1), K.at<double>(0,2),
			  K.at<double>(1,0), K.at<double>(1,1), K.at<double>(1,2),
			  K.at<double>(2,0), K.at<double>(2,1), K.at<double>(2,2);

		SetCalibration(tK, k, L);
	}

	void SetCalibration( transMatrix2D &K, std::vector<float> &k, transMatrix3D &L)
	{
		calibration.width = GetCurrent().cols;
		calibration.height = GetCurrent().rows;
		calibration.K = K;
		calibration.distParams = k;
		calibration.L = L;
	}


	virtual void SaveCalibration() = 0;

	// all image sources should have an associated calibration,
	// even if that calibration is a null calibration.
	Calibration calibration;
};


// the most basic image source is a directory of images.
class ImageDirectory : public ImageSource
{
private:
	void PreFetchThread();
	
	cv::Mat nextImage;
	std::condition_variable nextImage_cv;
	std::mutex nextImage_mutex;
	bool getNext;
	std::thread preFetchThread;
	bool threadQuit;
	
public:
	ImageDirectory( std::string in_path );
	
	ImageDirectory( std::string in_path, std::string in_calibPath );
	
	~ImageDirectory();
	
	cv::Mat GetCurrent();
	
	bool Advance();
	
	bool Regress();
	
	unsigned GetCurrentFrameID();
	
	frameTime_t GetCurrentFrameTime();
	
	bool ReadImage( );
	
	int GetNumImages();
	
	// WARNING!!!
	// replaces the calibration file in the source directory!
	void SaveCalibration();

	virtual bool JumpToFrame(unsigned frame);
	
	
	// sometimes we might want something weird like this.
	void SortImageList();
	
	void ShuffleImageList();

private:

	bool IsImage(string s);
	
	std::vector<string> imageList;
	unsigned frameIdx;
	frameTime_t frameTime;

	// the current image.
	cv::Mat current;

	std::string path;
};


// it is occassionally useful to have a source that we can put an image and a calibration into.
class PassSource : public ImageSource
{
	PassSource( cv::Mat &img, Calibration &calib )
	{
		current = img;
		calibration = calib;
	}
	
	void SetCurrent( cv::Mat &img )
	{
		current = img;
	}
	
	cv::Mat GetCurrent()
	{
		return current;
	}
	
	bool Advance()
	{
		// a bit unclear what best to return here, as this source does not advance!
		// maybe just throw an exception?
		return true;
	}
	
	bool Regress()
	{
		return true;
	}
	
	unsigned GetCurrentFrameID()
	{
		return 0;
	}

	frameTime_t GetCurrentFrameTime()
	{
		return 0;
	}

	int GetNumImages()
	{
		return 1;
	}
	
private:
	
	cv::Mat current;
	
	
};




#endif
