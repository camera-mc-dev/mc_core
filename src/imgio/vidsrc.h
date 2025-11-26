#ifndef ME_VIDEO_SOURCE
#define ME_VIDEO_SOURCE

#include "imgio/imagesource.h"

// the most basic image source is a directory of images.
class VideoSource : public ImageSource
{
public:
	VideoSource( std::string in_vidPath, std::string in_calPath);
	virtual ~VideoSource();


// get the current image as a cv::Mat
	virtual cv::Mat GetCurrent();

	// advance the image source to the next image.
	// returns false on failure
	virtual bool Advance();

	// regress to the previous image if possible.
	// returns false if not possible.
	virtual bool Regress();
	
	virtual bool JumpToFrame(unsigned frame);

	virtual unsigned  GetCurrentFrameID()
	{
		// OpenCV says the CAP_PROP_POS_FRAMES is
		// the number of the next frame, hence -1
		return cvvc.get(cv::CAP_PROP_POS_FRAMES);
	}
	virtual frameTime_t GetCurrentFrameTime()
	{
		// ?
		return 0;
	}

	// if the source is finite, return how many images
	// are in the source. If it is not finite (e.g. a camera)
	// return a negative value.
	virtual int GetNumImages()
	{
		return cvvc.get(cv::CAP_PROP_FRAME_COUNT);	// negative signals we don't know
	}

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


	virtual void SaveCalibration()
	{
		calibration.Write(calPath);
	}



private:
	cv::VideoCapture cvvc;

	bool currentReady;
	cv::Mat current;

	std::string calPath;
	std::string vidPath;

	unsigned frameIdx;
	int numberOfFirstFrame;
	bool failAdvance;

};

#endif
