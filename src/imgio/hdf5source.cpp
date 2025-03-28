#ifdef HAVE_HIGH_FIVE

#include "imgio/hdf5source.h"
#include "misc/tokeniser.h"


#include <iostream>
#include <iomanip>
using std::cout;
using std::endl;

HDF5ImageWriter::HDF5ImageWriter( std::string outfn )
{
	try
	{
		outfi = new HighFive::File( outfn, HighFive::File::ReadWrite | HighFive::File::Create );
	}
	catch (HighFive::Exception& err) 
	{
		// catch and print any HDF5 error
		std::cout << err.what() << std::endl;
		exit(0);
	}
}

HDF5ImageWriter::~HDF5ImageWriter()
{
	outfi->flush();
	delete outfi;
}

void HDF5ImageWriter::AddImage( cv::Mat &img, size_t imgNumber )
{
	std::stringstream ss;
	ss << std::setw(12) << std::setfill('0') << imgNumber << "_" << img.rows << "_" << img.cols << "_";
	std::vector<size_t> dims;
	std::shared_ptr< HighFive::DataSet > dsi;
	switch( img.type() )
	{
		case CV_8UC1:
			dims = {(size_t)img.rows * (size_t)img.cols * 1 };
			ss << "1_b";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<unsigned char>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
		case CV_8UC3:
			dims = {(size_t)img.rows * (size_t)img.cols * 3 };
			ss << "3_b";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<unsigned char>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
		case CV_32FC1:
			dims = {(size_t)img.rows * (size_t)img.cols * 1 };
			ss << "1_f";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<float>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
		case CV_32FC3:
			dims = { (size_t)img.rows * (size_t)img.cols * 3 };
			ss << "3_f";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<float>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
	}
	cout << ss.str() << endl;
	dsi->write( img.data );
}


void HDF5ImageWriter::Flush()
{
	outfi->flush();
}









HDF5Source::HDF5Source( std::string in_filepath, std::string in_calibPath )
{
	//
	// Input filepath can be of the format:
	// /path/to/file.hdf5
	// <firstframe>:path/to/file.hdf5
	//
	int a = in_filepath.find(":");
	
	std::string filepath;
	if( a == std::string::npos )
	{
		filepath = in_filepath;
		currentFrameNo = 0;
	}
	else
	{
		filepath = std::string( in_filepath.begin(), in_filepath.begin()+a );
		std::string numStr( in_filepath.begin()+a+1, in_filepath.end() );
		currentFrameNo = std::atoi( numStr.c_str() );
	}
	
	
	cout << "opening: " << filepath << endl;
	infi = new HighFive::File( filepath, HighFive::File::ReadOnly );
	
	// get list of dataset names from file
	dsList = infi->listObjectNames();
	
	// map frame index to frame number from dataset name
	for( unsigned lc = 0; lc < dsList.size(); ++lc )
	{
		auto a = dsList[ lc ].find("_");
		std::string numStr = std::string( dsList[ lc ].begin(), dsList[ lc ].begin() + a );
		unsigned fno = std::atoi( numStr.c_str() );
		
//		cout << lc << " " << dsList[lc] << " " << fno << endl;
		
		idx2fno[ lc ] = fno;
		fno2idx[ fno ] = lc;
	}
	
	
	FindImage();
	
	calibPath = in_calibPath;
}


HDF5Source::~HDF5Source()
{
	delete infi;
}

cv::Mat HDF5Source::GetCurrent()
{
	return current;
}

bool HDF5Source::Advance()
{
	if( currentFrameNo < GetNumImages() - 1 )
	{
		++currentFrameNo;
		FindImage();
		return true;
	}
	else return false;
}


bool HDF5Source::Regress()
{
	if( currentFrameNo > 0 )
	{
		--currentFrameNo;
		FindImage();
		return true;
	}
	else return false;
}

unsigned HDF5Source::GetCurrentFrameID()
{
	return currentFrameNo;
}

frameTime_t HDF5Source::GetCurrentFrameTime()
{
	return 0;
}


int HDF5Source::GetNumImages()
{
	// what we really want to do is return our largest frame number
	// so not this: return dsList.size();
	//cout << "hdf5 fnos: " <<  fno2idx.begin()->first << " " << fno2idx.rbegin()->first << endl;
	return fno2idx.rbegin()->first;
}


bool HDF5Source::JumpToFrame(unsigned frame)
{
	currentFrameNo = frame;
	FindImage();
}

void HDF5Source::FindImage()
{
//	cout << "Finding frame number: " << currentFrameNo << endl;
	auto i = fno2idx.find( currentFrameNo );
	if( i == fno2idx.end() )
	{
//		cout << "\t" << "no such frame no" << endl;
		// make a blank image the same size as the first image we can get.
		dsIdx = fno2idx.begin()->second;
		GetImage();
		
		current = cv::Mat( current.rows, current.cols, current.type(), cv::Scalar(0) );
	}
	else
	{
//		cout << "\t" << currentFrameNo << " had idx : " << i->second << endl;
		dsIdx = i->second;
		GetImage();
	}
}

void HDF5Source::GetImage()
{
	HighFive::DataSet dsi = infi->getDataSet( dsList[ dsIdx ] );
	
	auto ss = SplitLine( dsList[ dsIdx ], "_");
	for( unsigned c = 0; c < ss.size(); ++c )
	{
//		cout << c << " " << ss[c] << endl;
	}
	// we should have [ frameNumber, rows, cols, channels, type ]
	if( ss[3].compare("1") == 0 && ss[4].compare("b") == 0 )
	{
		current = cv::Mat( std::atoi( ss[1].c_str()), std::atoi(ss[2].c_str()), CV_8UC1 );
	}
	else if( ss[3].compare("3") == 0 && ss[4].compare("b") == 0 )
	{
		current = cv::Mat( std::atoi( ss[1].c_str()), std::atoi(ss[2].c_str()), CV_8UC3 );
	}
	else if( ss[3].compare("1") == 0 && ss[4].compare("f") == 0 )
	{
		current = cv::Mat( std::atoi( ss[1].c_str()), std::atoi(ss[2].c_str()), CV_32FC1 );
	}
	else if( ss[3].compare("3") == 0 && ss[4].compare("f") == 0 )
	{
		current = cv::Mat( std::atoi( ss[1].c_str()), std::atoi(ss[2].c_str()), CV_32FC3 );
	}
	else
	{
		cout << "didn't understand string: " << dsList[ dsIdx ] << endl;
		throw std::runtime_error( "Error with hdf5 source" );
	}
	
	
	dsi.read( current.data );
}

#endif
