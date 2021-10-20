#include "misc/tokeniser.h"

std::vector<std::string> SplitLine(const std::string& input, const std::string& delimitors)
{
	std::vector<std::string> result;
	
	size_t pos = std::string::npos;
	for( char c : delimitors )
	{
		pos = std::min(pos, input.find( c ) );
	}
	
	size_t startIndex = 0;
	while(pos != std::string::npos)
	{
		std::string temp(input.begin()+startIndex, input.begin()+pos);
		if( temp.size() > 0 )
			result.push_back(temp);
		startIndex = pos + 1;
		pos = std::string::npos;
		for( char c : delimitors )
		{
			pos = std::min(pos, input.find( c, startIndex ) );
		}
	}
	if(startIndex != input.size())
		result.push_back(std::string(input.begin()+startIndex, input.end()));
	return result;      
}
