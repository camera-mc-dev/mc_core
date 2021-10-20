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

#include "imgio/loadsave.h"

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
	void PreFetchThread()
	{
		while( !threadQuit )
		{
			// wait for trigger
			std::unique_lock<std::mutex> lock( nextImage_mutex );
			while( !getNext )
			{
				nextImage_cv.wait( lock );
			}
			
			// read image 
			if( frameIdx+1  < imageList.size() )
			{
// 				std::cout << "\n*\n*\n*\n*\nprefetch load \n*\n*\n*\n*" << std::endl;
				nextImage = LoadImage( imageList[frameIdx+1] );
			}
			
			// release mutex and allow anyone waiting on us to continue.
			getNext = false;
			lock.unlock();
			nextImage_cv.notify_one();
			
		}
		
	}
	cv::Mat nextImage;
	std::condition_variable nextImage_cv;
	std::mutex nextImage_mutex;
	bool getNext;
	std::thread preFetchThread;
	bool threadQuit;
	
public:
	ImageDirectory( std::string in_path )
	{
		this->path = in_path;

		// find the images in the directory.
		boost::filesystem::path p(path);

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

		std::sort( imageList.begin(), imageList.end() );

		// find the calibration file in the directory and read it.
		// if there's no file, this will return false, but we'll still
		// have a null calibration.
		calibration.Read( path + "/calibFile");


		// advance to the first frame.
		frameIdx = 0;
		ReadImage();
		
		// start the pre-fetch thread.
		// If we're doing heavy processing, we don't want to have to also wait to
		// read the next image from disk if we don't have to.
		getNext = true;
		threadQuit = false;
		preFetchThread = std::thread( &ImageDirectory::PreFetchThread, this );
		
	}
	
	ImageDirectory( std::string in_path, std::string in_calibPath )
	{
		this->path = in_path;

		// find the images in the directory.
		boost::filesystem::path p(path);

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

		std::sort( imageList.begin(), imageList.end() );

		// find the calibration file in the directory and read it.
		// if there's no file, this will return false, but we'll still
		// have a null calibration.
		calibration.Read( in_calibPath );


		// advance to the first frame.
		frameIdx = 0;
		ReadImage();
		
		// start the pre-fetch thread.
		// If we're doing heavy processing, we don't want to have to also wait to
		// read the next image from disk if we don't have to.
		getNext = true;
		threadQuit = false;
		preFetchThread = std::thread( &ImageDirectory::PreFetchThread, this );
		
	}
	
	~ImageDirectory()
	{
		std::unique_lock<std::mutex> lock( nextImage_mutex );
		threadQuit = true;
		lock.unlock();
		nextImage_cv.notify_one();
		
		preFetchThread.join();
	}

	cv::Mat GetCurrent()
	{
		// return the current image.
		return current;
	}

	bool Advance()
	{
		if( frameIdx == imageList.size()-1 )
			return false;
		
		//
		// The prefetch should have the next image ready for us.
		//
		
		
		// try to lock nextImage mutex
		std::unique_lock<std::mutex> lock( nextImage_mutex );
		current = nextImage.clone();
		++frameIdx;
		
		// let the prefetch work on getting the next image.
		getNext = true;
		lock.unlock();
		nextImage_cv.notify_one();
		
		return true;
	}

	bool Regress()
	{
		if( frameIdx == 0 )
			return false;
		
		// have some care of the pre-fetch...
		std::unique_lock<std::mutex> lock( nextImage_mutex );
		nextImage = current.clone();
		
		// decrement the frame index
		--frameIdx;
		
		lock.unlock();

		// load the image and update timestamps etc.
		return ReadImage();
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
		// current = cv::imread( imageList[frameIdx] );
		current = LoadImage( imageList[frameIdx] );
		return true;
		//TODO: Error checks!
	}

	int GetNumImages()
	{
		return imageList.size();
	}

	// WARNING!!!
	// replaces the calibration file in the source directory!
	void SaveCalibration()
	{
		calibration.Write( path + "/calibFile" );
	}

	virtual bool JumpToFrame(unsigned frame)
	{
		std::unique_lock<std::mutex> lock( nextImage_mutex );
		
		frameIdx = frame;
		if( ReadImage() )
		{
			// let the prefetch work on getting the next image.
			getNext = true;
			lock.unlock();
			nextImage_cv.notify_one();
			
			return true;
		}
		return false;
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
