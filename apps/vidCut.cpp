#include "imgio/vidWriter.h"
#include "imgio/sourceFactory.h"

#include "libconfig.h++"

#include <iostream>
using std::cout;
using std::endl;


struct SCut
{
	SCut( std::string n, int s, int e )
	{
		name  = n;
		start = s;
		end   = e;
	};
	std::string name;
	int start;
	int end;
};

int main(int argc, char* argv[])
{
	if( argc != 2 && argc != 3 && argc != 6 )
	{
		cout << "Cut video into sequences based on frame numbers or timecode. " << endl;
		cout << endl;
		cout << "This is basically a wrapper around ffmpeg." << endl;
		cout << "We use our VidWriter class which is now piping " << endl;
		cout << "images to the ffmpeg executable, rather than " << endl;
		cout << "trying to use the ceaselessly opaque API" << endl;
		cout << endl;
		cout << "Usage: " << endl;
		cout << "\t - Basic: " << endl;
		cout << "\t  " << argv[0] << " <cutFile> <fps> " << endl;
		cout << "\t  writes to an h264 mp4 with yuv420p and crf of 18" << endl;
		cout << endl;
		cout << "\t - Advanced: " << endl;
		cout << "\t  " << argv[0] << " <cutFile> <fps> <h265 | h264 | vp9 > <pixfmt> <crf>" << endl;
		cout << "\t  choice of a few high quality encoders, set the pixel format and crf yourself. " << endl;
		cout << "\t  note that each codec has a slightly different value for crf: " << endl;
		cout << "\t  - h265: 0 (high quality) -> 51 (small file): default 26." << endl;
		cout << "\t  - h264: 0 (high quality) -> 51 (small file). Default 23. 18 said to be 'visually lossles'. " << endl;
		cout << "\t  -  vp9: 4 (high quality) -> 63 (small file): Recommend 15 -> 30. " << endl;
		cout << "\t  here are a few pixfmts, see ffmpeg docs for more:" << endl;
		cout << "\t  - yuv420p: safe, works in all video players, lowers colour resolution" << endl;
		cout << "\t  - yuv444p: better colour resolution, but video players tend to not like it" << endl;
		cout << "\t  - rgb8   : try if you want, tends to be inferior for lossy compression" << endl;
		cout << endl << endl;
		cout << "\t - cutFile help:" << endl;
		cout << "\t  " << argv[0] << " help " << endl;
		exit(0);
	}
	else if( argc == 2 )
	{
		cout << "Cut file is config file: " << endl << endl;
		cout << "dataRoot = \"/path/to/dataRoot/\";                             # dataroot as normal" << endl;
		cout << "testRoot = \"path/to/testRoot/\";                              # path to test root <dataRoot>/<testRoot>/" << endl;
		cout << "vidFiles = (\"raw/00.mp4\", \"raw/01.mp4\", \"raw/02.mp4\");       # all video files beneath testRoot" << endl;
		cout << "refVid = \"raw/00.mp4\";                                       # which video do the cutFrames refer to?" << endl;
		cout << "cutFrames = ( (\"test0\", 12, 150), (\"test1\", 210, 280) );     # cut frames, with name of cut, and start, end frames" << endl;
		cout << "offsets = (0, 17, 19);                                       # sync offsets for each video file" << endl;
		cout << endl << endl;
	}
	
	//
	// Parse args
	//
	std::string cfgFile = argv[1];
	int fps = atoi( argv[2] );
	std::string ocdc, ofmt;
	int ocrf;
	if( argc == 3 )
	{
		ocdc = "h264";
		ofmt = "yuv420p";
		ocrf = 18;
	}
	else
	{
		ocdc = argv[3];
		ofmt = argv[4];
		ocrf = atoi( argv[5] );
	}
	
	//
	// Parse config file
	//
	// parse config file
	libconfig::Config cfg;
	
	std::string dataRoot;
	std::string testRoot;
	std::vector< std::string > vidFiles;
	std::vector< int >         offsets;
	std::string refcid;
	std::vector< SCut >        cuts;
	try
	{
		cfg.readFile( cfgFile.c_str() );
		
		if( cfg.exists("dataRoot") )
			dataRoot = (const char*)cfg.lookup("dataRoot");
		testRoot = (const char*)cfg.lookup("testRoot");
		
		libconfig::Setting &vfs = cfg.lookup("vidFiles");
		for( unsigned c = 0; c < vfs.getLength(); ++c )
			vidFiles.push_back( vfs[c] );
		
		libconfig::Setting &offs = cfg.lookup("offsets");
		for( unsigned c = 0; c < offs.getLength(); ++c )
			offsets.push_back( offs[c] );
		
		assert( offsets.size() == vidFiles.size() );
		
		refcid = (const char*)cfg.lookup("refVid");
		
		libconfig::Setting &cps = cfg.lookup("cutFrames");
		for( unsigned c = 0; c < cps.getLength(); ++c )
		{
			cuts.push_back( SCut( cps[c][0], cps[c][1], cps[c][2] ) );
		}
		
		
	}
	catch( libconfig::SettingException &e)
	{
		cout << "Setting error: " << endl;
		cout << e.what() << endl;
		cout << e.getPath() << endl;
		exit(0);
	}
	catch( libconfig::ParseException &e )
	{
		cout << "Parse error:" << endl;
		cout << e.what() << endl;
		cout << e.getError() << endl;
		cout << e.getFile() << endl;
		cout << e.getLine() << endl;
		exit(1);
	}
	
	for( unsigned vc = 0; vc < vidFiles.size(); ++vc )
	{
		cout << vidFiles[vc] << endl;
		std::stringstream vss;
		vss << dataRoot << testRoot << vidFiles[vc];
		auto sp = CreateSource( vidFiles[vc] );
		auto src = sp.source;
		
		for( unsigned curCut = 0; curCut < cuts.size(); ++curCut )
		{
			cout << "\t" << cuts[curCut].name << endl;
			cout << "\t advancing to first frame of cut..." << cuts[ curCut ].start + offsets[vc] << "( " << cuts[ curCut ].start << " + " << offsets[vc] << ")" << endl;
			while( src->GetCurrentFrameID() < (cuts[ curCut ].start + offsets[vc]) )
			{
				src->Advance();
			}
			int a = vidFiles[vc].rfind("/");
			std::stringstream oss;
			oss << dataRoot << testRoot << cuts[ curCut ].name;
			
			std::string od = oss.str();
			if( !boost::filesystem::exists(od) )
				boost::filesystem::create_directories(od);
			
			oss << "/" << std::string( vidFiles[vc].begin() + a + 1, vidFiles[vc].end() );
			std::string of = oss.str();
			
			std::shared_ptr< VidWriter > vo;
			vo.reset( new VidWriter( of, ocdc, src->GetCurrent(), fps, ocrf, ofmt ) );
			
			cout << "\t === cutting: " << src->GetCurrentFrameID() << " " << cuts[ curCut ].start + offsets[vc] << " : " << (cuts[ curCut ].end + offsets[vc]) << endl;
			while( src->GetCurrentFrameID() < (cuts[ curCut ].end + offsets[vc]) )
			{
				cv::Mat img = src->GetCurrent();
				vo->Write( img );
				if( !src->Advance() )
				{
					cout << "run out of frames processing " << vidFiles[vc] << " at frame " << src->GetCurrentFrameID() << endl;
					break;
				}
			}
			
// 			break;
		}
	}
	
	
}
