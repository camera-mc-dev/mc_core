#include "sourceFactory.h"
#include <boost/filesystem.hpp>
#include <map>


std::string GetBotLevelDir( std::string in )
{
	boost::filesystem::path p(in);
	return p.filename().string();
}

std::string GetBotLevelFile( std::string in )
{
	boost::filesystem::path p(in);
	return p.stem().string();
}

SourceHandle CreateSource( std::string input, std::string calibFile )
{
	//
	// input is a string, and the acceptable format of that string is:
	//   - /path/to/directory/
	//   - /path/to/video.file
	//   - <tag>:<info>
	//
	// where <tag>:<info> can be one of:
	//   - fndir:<info> : create an image directory source where image filenames indicate the actual frame number
	//
	SourceHandle retval;
	retval.isDirectorySource = false;
	
	boost::filesystem::path inpth( input );
	
	int a = input.find(":");
	if( a == std::string::npos )
	{
		if( boost::filesystem::is_directory( inpth ))
		{
			// create a directory source.
			if( calibFile.compare("none") == 0 )
				retval.source.reset( new ImageDirectory(input) );
			else
				retval.source.reset( new ImageDirectory(input, calibFile) );
			
			// return the top-level directory name as a source name.
			retval.name = GetBotLevelDir(input);
			retval.isDirectorySource = true;
		}
		else if( boost::filesystem::exists( inpth ) )
		{
			// create a video source unless its an hdf5 file
			if( inpth.extension().compare(".hdf5") == 0 )
			{
#ifdef HAVE_HIGH_FIVE
				retval.source.reset( new HDF5Source( input, calibFile ) );
#else
				throw std::runtime_error("Can't open an hdf5 image source because not compiled with high five library");
#endif
			}
			else
			{
				if( calibFile.compare( "none" ) == 0 )
				{
					// can we find a calib file?
					std::stringstream ss;
					ss << input << ".calib";
					if( boost::filesystem::exists( ss.str() ) )
						calibFile = ss.str();
				}
				retval.source.reset( new VideoSource( input, calibFile ) );
			}
			
			// return the filename without extension as a source name.
			retval.name = GetBotLevelFile( input );
		}
	}
	else
	{
		std::string tag( input.begin(), input.begin()+a);
		std::string info( input.begin()+a+1, input.end());
		
		if( tag.find(".hdf5") != std::string::npos ) // we'll assume it is an .hdf5 file.
		{
#ifdef HAVE_HIGH_FIVE
			cout << input << " " << tag << " " << info << endl;
			retval.source.reset( new HDF5Source( input, calibFile ) );
#else
			throw std::runtime_error("Can't open an hdf5 image source because not compiled with high five library");
#endif
		}
		else if( tag.compare("fndir") == 0 )
		{
			// create a directory source.
			if( calibFile.compare("none") == 0 )
				retval.source.reset( new FNImageDirectory(info) );
			else
				retval.source.reset( new FNImageDirectory(info, calibFile) );
			
			// return the top-level directory name as a source name.
			retval.name = GetBotLevelDir(input);
			retval.isDirectorySource = true;
		}
		else
		{
			cout << "unknown tag in <tag>:<info> format image source" << endl;
			cout << "input was:  " << input << endl;
			throw std::runtime_error("Bad tag");
		}
	}
	
	return retval;
	
}
