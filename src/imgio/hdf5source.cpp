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
			dims = {img.rows * img.cols * 1 };
			ss << "1_b";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<unsigned char>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
		case CV_8UC3:
			dims = {img.rows * img.cols * 3 };
			ss << "3_b";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<unsigned char>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
		case CV_32FC1:
			dims = {img.rows * img.cols * 1 };
			ss << "1_f";
			dsi.reset(new HighFive::DataSet(outfi->createDataSet<float>( ss.str(), HighFive::DataSpace( dims ) ) ) );
			break;
		case CV_32FC3:
			dims = {img.rows * img.cols * 3 };
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
	cout << "opening: " << in_filepath << endl;
	infi = new HighFive::File( in_filepath, HighFive::File::ReadOnly );
	
	// get list of dataset names from file
	dsList = infi->listObjectNames();
	
	// map frame index to frame number from dataset name
	for( unsigned lc = 0; lc < dsList.size(); ++lc )
	{
		auto a = dsList[ dsIdx ].find("_");
		std::string numStr = std::string( dsList[ dsIdx ].begin(), dsList[ dsIdx ].begin() + a );
		unsigned fno = std::atoi( numStr.c_str() );
		
		idx2fno[ lc ] = fno;
		fno2idx[ fno ] = lc;
	}
	
	// set to first ds.
	dsIdx = 0;
	
	GetImage();
	
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
	if( dsIdx < dsList.size() - 1 )
	{
		++dsIdx;
		GetImage();
		return true;
	}
	else return false;
}


bool HDF5Source::Regress()
{
	if( dsIdx > 0 )
	{
		--dsIdx;
		GetImage();
		return true;
	}
	else return false;
}

unsigned HDF5Source::GetCurrentFrameID()
{
	return idx2fno[ dsIdx ];
}

frameTime_t HDF5Source::GetCurrentFrameTime()
{
	return 0;
}


int HDF5Source::GetNumImages()
{
	return dsList.size();
}


bool HDF5Source::JumpToFrame(unsigned frame)
{
	// note that this might not jump to a frame with the frame number "frame"
	auto fi = fno2idx.find( frame );
	if( fi == fno2idx.end() )
		return false;
	else
		dsIdx = fi->second;
	GetImage();
	return true;
}

void HDF5Source::GetImage()
{
	HighFive::DataSet dsi = infi->getDataSet( dsList[ dsIdx ] );
	
	auto ss = SplitLine( dsList[ dsIdx ], "_");
	for( unsigned c = 0; c < ss.size(); ++c )
	{
		cout << c << " " << ss[c] << endl;
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

