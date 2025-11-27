#ifndef MC_DEV_COMMON_CONFIG
#define MC_DEV_COMMON_CONFIG

#include "libconfig.h++"
#include <sstream>
#include <boost/filesystem.hpp>
#include <iostream>
using std::cout;
using std::endl;

#if defined(__APPLE__) || defined(  __gnu_linux__  )
#include <unistd.h>
#include <sys/types.h>
#include "pwd.h"
#endif

#include <iostream>
using std::cout;
using std::endl;

// certain things are more usefully specified once in a 
// user's common config file

class CommonConfig
{
public:
	std::string dataRoot;
	std::string shadersRoot;
	std::string coreDataRoot;
	std::string scriptsRoot;
	std::string netsRoot;
	std::string ffmpegPath;
	
	unsigned maxSingleWindowWidth;
	unsigned maxSingleWindowHeight;
	
	CommonConfig()
	{
		// we need to know the user's home directory.
		// ideally in a safe and sane cross-platform way.
		// ha ha ha ha.
		std::string userHome;
#if defined(__APPLE__) || defined( __gnu_linux__ )
		struct passwd* pwd = getpwuid(getuid());
		if (pwd)
		{
			userHome = pwd->pw_dir;
		}
		else
		{
			// try the $HOME environment variable
			userHome = getenv("HOME");
		}
#elif defined(_WIN32)
		// assume we're in a run directory, just use current dir.
		userHome = "./";
#else
		throw std::runtime_error("unknown platform, common config home dir");
#endif
		
		
		
		// does the user have a common config file?
		std::stringstream ss;
		ss << userHome << "/.mc_dev.common.cfg";
		boost::filesystem::path p(ss.str());
		try
		{
			if( !boost::filesystem::exists(p) )
			{
				// create a default config file with conservative guesses.
				libconfig::Config cfg;
				auto &cfgRoot = cfg.getRoot();
				
				cfgRoot.add("dataRoot", libconfig::Setting::TypeString);
				cfgRoot.add("shadersRoot", libconfig::Setting::TypeString);
				cfgRoot.add("coreDataRoot", libconfig::Setting::TypeString);
				cfgRoot.add("scriptsRoot", libconfig::Setting::TypeString);

				cfgRoot.add("maxSingleWindowWidth", libconfig::Setting::TypeInt );
				cfgRoot.add("maxSingleWindowHeight", libconfig::Setting::TypeInt );
				
				cfg.lookup("dataRoot")     = userHome + "/programming/mc_dev/mc_core/data/";
				cfg.lookup("shadersRoot")  = userHome + "/programming/mc_dev/mc_core/shaders/";
				cfg.lookup("coreDataRoot") = userHome + "/programming/mc_dev/mc_core/data/";
				cfg.lookup("scriptsRoot")  = userHome + "/programming/mc_dev/mc_core/python";
				cfg.lookup("netsRoot")     = userHome + "/programming/mc_dev/mc_nets/data/";
				cfg.lookup("ffmpegPath")   = userHome + "/usr/bin/ffmpeg";
				
				cfg.lookup("maxSingleWindowWidth") = 1000;
				cfg.lookup("maxSingleWindowHeight") = 800;
				
				cfg.writeFile( ss.str().c_str() );
			}
			
			libconfig::Config cfg;
			cfg.readFile( ss.str().c_str() );
			
			dataRoot     = (const char*) cfg.lookup("dataRoot");
			shadersRoot  = (const char*) cfg.lookup("shadersRoot");
			coreDataRoot = (const char*) cfg.lookup("coreDataRoot");
			scriptsRoot  = (const char*) cfg.lookup("scriptsRoot");
			netsRoot     = (const char*) cfg.lookup("netsRoot");
			ffmpegPath   = (const char*) cfg.lookup("ffmpegPath");
			
			maxSingleWindowWidth  = cfg.lookup("maxSingleWindowWidth");
			maxSingleWindowHeight = cfg.lookup("maxSingleWindowHeight");
		}
		catch( libconfig::SettingException &e)
		{
			cout << "Error reading common config file: " << ss.str() << endl;
			cout << "Setting error: " << endl;
			cout << e.what() << endl;
			cout << e.getPath() << endl;
			exit(0);
		}
		catch( libconfig::ParseException &e )
		{
			cout << "Error reading common config file: " << ss.str() << endl;
			cout << "Parse error:" << endl;
			cout << e.what() << endl;
			cout << e.getError() << endl;
			cout << e.getFile() << endl;
			cout << e.getLine() << endl;
			exit(0);
		}
		
	};
};


#endif
