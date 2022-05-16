#ifndef MC_SOURCE_FACTORY_H
#define MC_SOURCE_FACTORY_H


#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "imgio/fnDirSrc.h"
#include "imgio/hdf5source.h"



struct SourcePair
{
	std::shared_ptr< ImageSource > source;
	std::string name;
};

SourcePair CreateSource( std::string input, std::string calibFile = "none" );

#endif

