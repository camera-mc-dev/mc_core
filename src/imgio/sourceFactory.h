#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "imgio/fnDirSrc.h"


struct SourcePair
{
	std::shared_ptr< ImageSource > source;
	std::string name;
};

SourcePair CreateSource( std::string input, std::string calibFile = "none" );
