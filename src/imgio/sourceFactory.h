#ifndef MC_SOURCE_FACTORY_H
#define MC_SOURCE_FACTORY_H


#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "imgio/fnDirSrc.h"
#include "imgio/hdf5source.h"



struct SourceHandle
{
	// the source...
	std::shared_ptr< ImageSource > source;
	
	// a name based on the source type/location/etc.
	std::string name;
	
	// a flag to help calib class decide where to put "grids" file 
	bool isDirectorySource;
};

SourceHandle CreateSource( std::string input, std::string calibFile = "none" );

#endif

