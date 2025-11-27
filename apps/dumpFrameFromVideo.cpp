#include "imgio/vidsrc.h"
#include "imgio/loadsave.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
using std::cout;
using std::endl;

int main(int argc, char* argv[] )
{
	if( argc < 5 )
	{
		cout << "Usage: " << endl;
		cout << "Dump the listed frames to jpeg images in the already existing output directory." << endl;
		cout << endl;
		cout << argv[0] << " <vid file> <out dir> <file tag > <frame 0> < frame 1 > ..... < frame n > " << endl;
	}
	
	std::vector<int> frames;
	std::string outDir;
	std::string vidFile;
	std::string filetag;
	
	vidFile = argv[1];
	outDir  = argv[2];
	filetag = argv[3];
	
	for(unsigned c = 4; c < argc; ++c )
	{
		frames.push_back( atoi( argv[c] ) );
	}
	
	VideoSource src( vidFile, "none" );
	
	for( unsigned fc = 0; fc < frames.size(); ++fc )
	{
		cout << fc << endl;
		std::stringstream ss;
		ss << outDir << "/" << filetag << "-" << std::setw(6) << std::setfill('0') << frames[fc] << ".jpg";
		
		src.JumpToFrame( frames[fc]);
		cv::Mat c = src.GetCurrent();
		
		MCDSaveImage( c, ss.str() );
	}
}
