#include "misc/tokeniser.h"

bool IsDelimitor( char c, const std::string& delimitors )
{
	for( unsigned dc = 0; dc < delimitors.size(); ++dc )
	{
		if( c == delimitors[dc] ) return true;
	}
	return false;
}
#include <iostream>
using std::cout;
using std::endl;
std::vector<std::string> SplitLine(const std::string& input, const std::string& delimitors)
{
	std::vector<std::string> result;
	
	int tokenStart = 0;
	int tokenEnd   = 0;
	cout << input << endl;
	while( tokenStart < input.size() )
	{
		while( IsDelimitor( input[ tokenStart ], delimitors ) )
		{
			++tokenStart;
		}
		
		tokenEnd = tokenStart;
		while( !IsDelimitor( input[tokenEnd], delimitors ) && tokenEnd < input.size() )
		{
			++tokenEnd;
		}
		
		if( tokenStart < input.size() && tokenEnd > tokenStart)
		{
			result.push_back( std::string( input.begin()+tokenStart, input.begin()+tokenEnd ) );
			tokenStart = tokenEnd;
		}
	}
	
	return result;      
}
