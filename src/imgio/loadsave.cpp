#include "imgio/loadsave.h"
#include "Magick++.h"

#include <fstream>
#include <iostream>
using std::cout;
using std::endl;

// for saving float images, we'll make our own format,
// which is simply the float buffer compressed quickly by snappy.
// Why? Because saving float images is poorly standardised, and
// often we'll be after a very fast io rather than the best of the
// best compression - and we want to be lossless!
#include <snappy.h>

bool magickIsInitted = false;

// Sometimes, it's quicker or just nicer to have a custom wrapper
// for loading / saving images and using Magick++ rather than opencv.
cv::Mat MCDLoadImage(std::string filename)
{

	if( !magickIsInitted )
    {
        Magick::InitializeMagick(NULL);
        magickIsInitted = true;
    }

	//cout << "loading: " << filename << endl;
	if( filename.find(".floatImg") != std::string::npos )
	{
		//cout << "is .floatImg" << endl;
		// this should be an uncompressed float image.
		std::ifstream infi;
		infi.open( filename, std::ios::in | std::ios::binary );

		unsigned magic,w,h,c;
		size_t s;
		infi.read( (char*)&magic, sizeof(magic) );

		if( magic != 820830001 )
		{
			goto jumpLabel01;
		}

		infi.read( (char*)&w, sizeof(w) );
		infi.read( (char*)&h, sizeof(h) );
		infi.read( (char*)&c, sizeof(c) );
		infi.read( (char*)&s, sizeof(s) );
		std::vector<char> d(s);
		infi.read( &d[0], s );
		std::string uncompressed;
		if( !snappy::Uncompress( &d[0], d.size(), &uncompressed ) )
		{
			//throw std::runtime_error("Could not uncompress image (snappy error) " );
			cout << "Could not uncompress data with snappy... perhaps this is an old file?" << endl;
			if( c == 1 )
			{
				cv::Mat img(h,w, CV_32FC1);
				memcpy(img.data, &d[0], d.size() );
				return img;
			}
			else if( c == 3 )
			{
				cv::Mat img(h,w, CV_32FC3);
				memcpy(img.data, &d[0], d.size() );
				return img;
			}
			else
			{
				throw std::runtime_error("floatImg " + filename + " had the wrong number of channels.");
			}

		}
		else
		{
			if( c == 1 )
			{
				cv::Mat img(h,w, CV_32FC1);
				memcpy(img.data, uncompressed.data(), uncompressed.size() );
				return img;
			}
			else if( c == 3 )
			{
				cv::Mat img(h,w, CV_32FC3);
				memcpy(img.data, uncompressed.data(), uncompressed.size() );
				return img;
			}
			else
			{
				throw std::runtime_error("floatImg " + filename + " had the wrong number of channels.");
			}
		}
	}
	else if( filename.find(".charImg")  != std::string::npos)
	{
		//cout << "is .charImg..." << endl;
		// this should be an snappy compress 8-bit image.
		std::ifstream infi;
		infi.open( filename, std::ios::in | std::ios::binary );

		unsigned magic,w,h,c;
		size_t s;
		infi.read( (char*)&magic, sizeof(magic) );

		if( magic != 820830002 )
		{
			cout << "wrong magic number for .charImg... trying with Magick instead..." << endl;
			goto jumpLabel01;
		}

		infi.read( (char*)&w, sizeof(w) );
		infi.read( (char*)&h, sizeof(h) );
		infi.read( (char*)&c, sizeof(c) );
		infi.read( (char*)&s, sizeof(s) );
		std::vector<char> d(s);
		infi.read( &d[0], s );
		std::string uncompressed;
		if( !snappy::Uncompress( &d[0], d.size(), &uncompressed ) )
		{
			//throw std::runtime_error("Could not uncompress image (snappy error) " );
			cout << "Could not uncompress data with snappy..." << endl;
			if( c == 1 )
			{
				cv::Mat img(h,w, CV_8UC1);
				memcpy(img.data, &d[0], d.size() );
				return img;
			}
			else if( c == 3 )
			{
				cv::Mat img(h,w, CV_8UC3);
				memcpy(img.data, &d[0], d.size() );
				return img;
			}
			else
			{
				throw std::runtime_error("charImg " + filename + " had the wrong number of channels.");
			}

		}
		else
		{
			if( c == 1 )
			{
				cv::Mat img(h,w, CV_8UC1);
				memcpy(img.data, uncompressed.data(), uncompressed.size() );
				return img;
			}
			else if( c == 3 )
			{
				cv::Mat img(h,w, CV_8UC3);
				memcpy(img.data, uncompressed.data(), uncompressed.size() );
				return img;
			}
			else
			{
				throw std::runtime_error("charImg " + filename + " had the wrong number of channels.");
			}
		}
	}
	

jumpLabel01:

	Magick::Image mimg;
	mimg.read(filename);

	// TODO: Check type and colour channels of mimg...
	if( mimg.type() == Magick::GrayscaleType)
	{
		cv::Mat cvimg( mimg.rows(), mimg.columns(), CV_8UC1 );
		mimg.write(0,0, mimg.columns(), mimg.rows(), "I", Magick::CharPixel, cvimg.data );
		return cvimg;
	}
	else if( mimg.type() == Magick::PaletteType )
	{
		if( mimg.colorMapSize() <= 256 )
		{
			cv::Mat cvimg( mimg.rows(), mimg.columns(), CV_8UC1 );
			mimg.write(0,0, mimg.columns(), mimg.rows(), "I", Magick::CharPixel, cvimg.data );
			return cvimg;
		}
		else
		{
			cv::Mat cvimg( mimg.rows(), mimg.columns(), CV_8UC3 );
			mimg.write(0,0, mimg.columns(), mimg.rows(), "BGR", Magick::CharPixel, cvimg.data );
			return cvimg;
		}
	}
	else
	{
		cv::Mat cvimg( mimg.rows(), mimg.columns(), CV_8UC3 );
		mimg.write(0,0, mimg.columns(), mimg.rows(), "BGR", Magick::CharPixel, cvimg.data );
		return cvimg;
	}


}

void MCDSaveImage(cv::Mat &img, std::string filename)
{
	if( !magickIsInitted )
	{
		Magick::InitializeMagick(NULL);
		magickIsInitted = true;
	}


// 	cout << "Saving " << filename << endl;
	if( filename.find(".floatImg") != std::string::npos)
	{
// 		cout << "is floatImg " << endl;
		// this should be an uncompressed float image.
		std::ofstream outfi;
		outfi.open( filename, std::ios::out | std::ios::binary );

		unsigned magic,w,h,c;
		magic = 820830001;
		w = img.cols;
		h = img.rows;
		if( img.type() == CV_32FC1 )
		{
			c = 1;
		}
		else if( img.type() == CV_32FC3 )
		{
			c = 3;
		}
		else
		{
			throw std::runtime_error("MCDSaveImage: Image had neither 1 nor 3 channels.");
		}
		outfi.write( (char*)&magic, sizeof(magic) );
		outfi.write( (char*)&w, sizeof(w) );
		outfi.write( (char*)&h, sizeof(h) );
		outfi.write( (char*)&c, sizeof(c) );

		//outfi.write( (char*)img.data, h*w*c*sizeof(float) );
		std::string compressed;
		size_t s = 	snappy::Compress( (char*)img.data, w*h*c*sizeof(float), &compressed );
		outfi.write( (char*) &s, sizeof(size_t) );
		outfi.write( (char*)compressed.data(), compressed.size() );

		return;
	}
	else if( filename.find(".charImg") != std::string::npos)
	{
// 		cout << "is charImg " << endl;
		// this should be an uncompressed float image.
		std::ofstream outfi;
		outfi.open( filename, std::ios::out | std::ios::binary );

		unsigned magic,w,h,c;
		magic = 820830002;
		w = img.cols;
		h = img.rows;
		if( img.type() == CV_8UC1 )
		{
			c = 1;
		}
		else if( img.type() == CV_8UC3 )
		{
			c = 3;
		}
		outfi.write( (char*)&magic, sizeof(magic) );
		outfi.write( (char*)&w, sizeof(w) );
		outfi.write( (char*)&h, sizeof(h) );
		outfi.write( (char*)&c, sizeof(c) );

		//outfi.write( (char*)img.data, h*w*c*sizeof(float) );
		std::string compressed;
		size_t s = 	snappy::Compress( (char*)img.data, w*h*c*sizeof(unsigned char), &compressed );
		outfi.write( (char*) &s, sizeof(size_t) );
		outfi.write( (char*)compressed.data(), compressed.size() );

		return;
	}

	// todo... use Magick instead of opencv. I have reasons for that... umm...
	// honest. Probably mostly to do with OpenCV normalising things etc... maybe...
	if( img.type() == CV_32FC3 || img.type() == CV_32FC1 )
	{
		img *= 255;
	}
	cv::imwrite(filename, img);
	
}

void SaveCFImage( cfMatrix &img, std::string filename)
{
	cout << "Saving " << filename << endl;
	if( filename.find(".cplxImg") != std::string::npos)
	{
		cout << "is cplxImg " << endl;
		// this should be an uncompressed float image.
		std::ofstream outfi;
		outfi.open( filename, std::ios::out | std::ios::binary );

		unsigned magic,w,h,c;
		magic = 820830003;
		w = img.cols();
		h = img.rows();
		
		outfi.write( (char*)&magic, sizeof(magic) );
		outfi.write( (char*)&w, sizeof(w) );
		outfi.write( (char*)&h, sizeof(h) );
		

		//outfi.write( (char*)img.data, h*w*c*sizeof(float) );
		std::string compressed;
		size_t s = 	snappy::Compress( (char*)img.data(), w*h*2*sizeof(float), &compressed );
		outfi.write( (char*) &s, sizeof(size_t) );
		outfi.write( (char*)compressed.data(), compressed.size() );

		return;
	}
	else
	{
		throw std::runtime_error("Please use extension .cplxImg for saving cfMatrix images.");
	}
}

cfMatrix LoadCFImage(std::string filename)
{
	if( filename.find(".cplxImg") != std::string::npos )
	{
		std::ifstream infi;
		infi.open( filename, std::ios::in | std::ios::binary );

		unsigned magic,w,h,c;
		size_t s;
		infi.read( (char*)&magic, sizeof(magic) );

		if( magic != 820830003 )
		{
			throw std::runtime_error("Magic number in file does not match .cmplxImg.");
		}

		infi.read( (char*)&w, sizeof(w) );
		infi.read( (char*)&h, sizeof(h) );
		infi.read( (char*)&s, sizeof(s) );
		std::vector<char> d(s);
		infi.read( &d[0], s );
		std::string uncompressed;
		if( !snappy::Uncompress( &d[0], d.size(), &uncompressed ) )
		{
			throw std::runtime_error(std::string("Could not uncompress image (snappy error): ") +  filename );
			cout << "Could not uncompress data with snappy... " << endl;
		}
		else
		{
			cfMatrix res = cfMatrix::Zero( h, w );
			memcpy( res.data(), uncompressed.data(), uncompressed.size() );
			return res;
		}
	}
	else
	{
		throw std::runtime_error("Please use extension .cplxImg for loading cfMatrix images.");
	}
}




void SaveFBuffer( int rows, int cols, int channels, std::vector<float> &inBuff, std::string fn )
{
	assert( inBuff.size() == rows * cols * channels );
	
	std::ofstream outfi;
	outfi.open( fn, std::ios::out | std::ios::binary );

	unsigned magic;
	magic = 820830004;
	
	outfi.write( (char*)&magic, sizeof(magic) );
	outfi.write( (char*)&cols, sizeof(cols) );
	outfi.write( (char*)&rows, sizeof(rows) );
	outfi.write( (char*)&channels, sizeof(channels) );
	
	std::string compressed;
	size_t s = snappy::Compress( (char*) &inBuff[0], rows*cols*channels*sizeof(float), &compressed );
	
	outfi.write( (char*) &s, sizeof(size_t) );
	outfi.write( (char*)compressed.data(), compressed.size() );

}


void LoadFBuffer( std::string fn, std::vector<float> &outBuff, int &rows, int &cols, int &channels )
{
	std::ifstream infi;
	infi.open( fn, std::ios::in | std::ios::binary );
	
	unsigned magic;
	size_t s;
	
	infi.read( (char*)&magic, sizeof(magic) );
	infi.read( (char*)&cols, sizeof(cols) );
	infi.read( (char*)&rows, sizeof(rows) );
	infi.read( (char*)&channels, sizeof(channels) );
	infi.read( (char*)&s, sizeof(s) );
	assert( magic == 820830004);
	
	if( outBuff.size() != rows * cols * channels )
		outBuff.resize( rows * cols * channels );
	
	std::string uncompressed;
	std::vector<char> d(s);
	infi.read( &d[0], s );
	if( !snappy::Uncompress( &d[0], d.size(), &uncompressed ) )
	{
		throw std::runtime_error(std::string("Could not uncompress image (snappy error): ") +  fn );
		cout << "Could not uncompress data with snappy... " << endl;
	}
	
	memcpy( &outBuff[0], uncompressed.data(), uncompressed.size() );
}
