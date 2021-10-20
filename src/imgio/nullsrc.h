#ifndef ME_NULL_SOURCE
#define ME_NULL_SOURCE

#include "imgio/imagesource.h"

// A null source has a calibration, but no image data.
class NullSource : public ImageSource
{
public:
	NullSource( std::string in_calPath)
	{
		frameIdx = 0;
		
		if( !calibration.Read( in_calPath ) )
		{
			throw std::runtime_error("Could not read calibration file: " + in_calPath );
		}
	}
	virtual ~NullSource() {};


	// return a suitably sized blank image.
	virtual cv::Mat GetCurrent()
	{
		cv::Mat m( calibration.width, calibration.height, CV_8UC3, cv::Scalar(0) );
		return m;
	}

	// advance the image source to the next image.
	// returns false on failure
	virtual bool Advance()
	{
		frameIdx += 1;
		return true;
	}

	// regress to the previous image if possible.
	// returns false if not possible.
	virtual bool Regress()
	{
		if( frameIdx > 0 )
		{
			frameIdx -= 1;
			return true;
		}
		return false;
	}
	
	virtual bool JumpToFrame(unsigned frame)
	{
		frameIdx = frame;
		return true;
	}

	virtual unsigned  GetCurrentFrameID()
	{
		return frameIdx;
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
		return -1;
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

	std::string calPath;
	unsigned frameIdx;

};

#endif
