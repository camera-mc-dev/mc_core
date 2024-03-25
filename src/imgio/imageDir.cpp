#include "imgio/imagesource.h"

// the most basic image source is a directory of images.
void ImageDirectory::PreFetchThread()
{
	while( !threadQuit )
	{
		// wait for trigger
		std::unique_lock<std::mutex> lock( nextImage_mutex );
		while( !getNext && !threadQuit )
		{
			if( threadQuit )
				continue;
			nextImage_cv.wait( lock );
		}
		
		// read image 
		if( frameIdx+1  < imageList.size() )
		{
			//std::cout << "\n*\n*\n*\n*\nprefetch load \n*\n*\n*\n*" << std::endl;
			nextImage = LoadImage( imageList[frameIdx+1] );
		}
		
		// release mutex and allow anyone waiting on us to continue.
		getNext = false;
		lock.unlock();
		nextImage_cv.notify_one();
		
	}
}

ImageDirectory::ImageDirectory( std::string in_path )
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

ImageDirectory::ImageDirectory( std::string in_path, std::string in_calibPath )
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
	
ImageDirectory::~ImageDirectory()
{
	std::unique_lock<std::mutex> lock( nextImage_mutex );
	threadQuit = true;
	lock.unlock();
	nextImage_cv.notify_one();
	
	preFetchThread.join();
}

cv::Mat ImageDirectory::GetCurrent()
{
	// return the current image.
	assert( current.rows > 0 && current.cols > 0 );
	return current;
}

bool ImageDirectory::Advance()
{
	if( frameIdx == imageList.size()-1 )
		return false;
	
	//
	// The prefetch should have the next image ready for us.
	//
	
	
	// try to lock nextImage mutex
	bool got = false;
	while( !got )
	{
		std::unique_lock<std::mutex> lock( nextImage_mutex );
		current = nextImage.clone();
		if( current.rows == 0 || current.cols == 0 )
		{
			// std::cout << "ImageDirectory: Got empty image from nextImage?" << std::endl;
			lock.unlock();
		}
		else
		{
			got = true;
			++frameIdx;
			
			// let the prefetch work on getting the next image.
			getNext = true;
			lock.unlock();
			nextImage_cv.notify_one();
			
		}
	}
	
	
	
	return true;
}

bool ImageDirectory::Regress()
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

unsigned ImageDirectory::GetCurrentFrameID()
{
	return frameIdx;
}

frameTime_t ImageDirectory::GetCurrentFrameTime()
{
	return frameTime;
}

bool ImageDirectory::ReadImage( )
{
	// current = cv::imread( imageList[frameIdx] );
	current = LoadImage( imageList[frameIdx] );
	return true;
	//TODO: Error checks!
}

int ImageDirectory::GetNumImages()
{
	return imageList.size();
}

// WARNING!!!
// replaces the calibration file in the source directory!
void ImageDirectory::SaveCalibration()
{
	calibration.Write( path + "/calibFile" );
}

bool ImageDirectory::JumpToFrame(unsigned frame)
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


// sometimes we might want something weird like this.
void ImageDirectory::SortImageList()
{
	std::unique_lock<std::mutex> lock( nextImage_mutex );
	frameIdx = 0;
	std::sort( imageList.begin(), imageList.end() );
	
	// let the prefetch work on getting the next image.
	getNext = true;
	lock.unlock();
	nextImage_cv.notify_one();
}

void ImageDirectory::ShuffleImageList()
{
	std::unique_lock<std::mutex> lock( nextImage_mutex );
	frameIdx = 0;
	
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(imageList.begin(), imageList.end(), g);
	
	// let the prefetch work on getting the next image.
	getNext = true;
	lock.unlock();
	nextImage_cv.notify_one();
}



bool ImageDirectory::IsImage(string s)
{
	unsigned end = s.size();
	if( s.find(".jpg") == end-4     || s.find(".JPG") == end-4  ||  
	    s.find(".png") == end-4     || s.find(".PNG") == end-4  ||
	    s.find(".bmp") == end-4     || s.find(".BMP") == end-4  ||
	    s.find(".tiff") == end-5    ||
	    s.find(".tif") == end-4     ||
	    s.find(".charImg") == end-8 ||
	    s.find(".floatImg") == end-9
	  )
	{
		return true;
	}
	return false;
}
