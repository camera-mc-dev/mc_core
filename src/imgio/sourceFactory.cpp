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

SourcePair CreateSource( std::string input, std::string calibFile )
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
	SourcePair retval;
	
	int a = input.find(":");
	if( a == std::string::npos )
	{
		if( boost::filesystem::is_directory( input ))
		{
			// create a directory source.
			if( calibFile.compare("none") == 0 )
				retval.source.reset( new ImageDirectory(input) );
			else
				retval.source.reset( new ImageDirectory(input, calibFile) );
			
			// return the top-level directory name as a source name.
			retval.name = GetBotLevelDir(input);
		}
		else if( boost::filesystem::exists( input ) )
		{
			// create a video source.
			retval.source.reset( new VideoSource( input, calibFile ) );
			
			// return the filename without extension as a source name.
			retval.name = GetBotLevelFile( input );
		}
	}
	else
	{
		std::string tag( input.begin(), input.begin()+a);
		std::string info( input.begin()+a+1, input.end());
		
		
		if( tag.compare("fndir") == 0 )
		{
			// create a directory source.
			if( calibFile.compare("none") == 0 )
				retval.source.reset( new FNImageDirectory(info) );
			else
				retval.source.reset( new FNImageDirectory(info, calibFile) );
			
			// return the top-level directory name as a source name.
			retval.name = GetBotLevelDir(input);
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
