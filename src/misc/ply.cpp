#include <fstream>
#include <string>
#include <vector>
#include <iostream>
using std::cout;
using std::endl;

#include "misc/ply.h"
#include "misc/tokeniser.h"

struct SElement
{
	std::string name;
	int number;
	std::vector<std::string> properties;
	std::vector<std::string> propTypes;

	void Clear()
	{
		name = "";
		number = 0;
		properties.clear();
		propTypes.clear();
	}
};

genRowMajMatrix LoadPlyPointRGB( std::string infn )
{
	std::ifstream infi( infn, std::ios::binary );
	
	bool gotData   = false;
	bool inHeader  = true;
	bool gotply    = false;
	bool gotformat = false;
	std::string line;
	
	bool inElement = false;
	SElement curElement;
	std::vector< SElement > elements;
	
	// first line should be just "ply"
	// next line should show we are a binary mode ply.
	while( inHeader && std::getline(infi, line) )
	{
		auto ss = SplitLine( line, " " );
		if( ss.size() == 1 && ss[0].compare("ply") == 0 )
			gotply = true;
		else if( ss.size() == 3 && ss[0].compare("format") == 0 && ss[1].compare("binary_little_endian") == 0 && ss[2].compare("1.0") == 0 )
			gotformat = true;
		else if( ss.size() == 3 && ss[0].compare("format") == 0 )
		{
			cout << "format line not binary" << endl;
			cout << line << endl;
			infi.close();
			return genRowMajMatrix::Zero( 0, 0 );
		}
		else if( ss.size() == 3 && ss[0].compare("element") == 0 )
		{
			if( inElement )
				elements.push_back( curElement );
			curElement.Clear();
			curElement.name = ss[1];
			curElement.number  = std::atoi( ss[2].c_str() );
			inElement = true;
		}
		else if( ss.size() == 3 && ss[0].compare("property") == 0 && inElement )
		{
			curElement.propTypes.push_back( ss[1] );
			curElement.properties.push_back( ss[2] );
		}
		else if( ss.size() == 1 && ss[0].compare("end_header") == 0 )
		{
			inHeader = false;
			if( inElement )
				elements.push_back( curElement );
		}
	}
	
	cout << "Read .ply header:" << endl;
	cout << "elements: " << elements.size() << endl;
	for( unsigned ec = 0; ec < elements.size(); ++ec )
	{
		cout << "\t" << elements[ec].name << " " << elements[ec].number << " : np : " << elements[ec].properties.size() << endl;
	}
	
	if( elements.size() > 1 )
	{
		cout << "expected only 1 element. Failing." << endl;
		exit(0);
	}
	
	// assume this struct is always right
	struct SVert
	{
		float x, y, z;
		unsigned char r, g, b;
	};
	std::vector< SVert > data( elements[0].number );
	//infi.read( (char*) &data[0], data.size() * sizeof(SVert) );
	for( unsigned vc = 0; vc < data.size(); ++vc )
	{
		infi.read( (char*)&data[vc].x, sizeof(float ) );
		infi.read( (char*)&data[vc].y, sizeof(float ) );
		infi.read( (char*)&data[vc].z, sizeof(float ) );
		
		infi.read( (char*)&data[vc].r, sizeof(unsigned char) );
		infi.read( (char*)&data[vc].g, sizeof(unsigned char) );
		infi.read( (char*)&data[vc].b, sizeof(unsigned char) );
	}
	
	
	if( infi.fail() )
	{
		cout << "error reading data block" << endl;
		exit(0);
	}
	
	genRowMajMatrix ret = genRowMajMatrix::Zero( data.size(), 6 );
	for( unsigned dc = 0; dc < data.size(); ++dc )
	{
		ret.row(dc) <<        data[dc].x,        data[dc].y,       data[dc].z,
		               data[dc].r/255.0f, data[dc].g/255.0f, data[dc].b/255.0f;
	}
	
	
	return ret;
};


genRowMajMatrix LoadPlyTxtPointRGB( std::string infn )
{
	std::ifstream infi( infn );
	
	bool gotData   = false;
	bool inHeader  = true;
	bool gotply    = false;
	bool gotformat = false;
	std::string line;
	
	bool inElement = false;
	SElement curElement;
	std::vector< SElement > elements;
	
	// first line should be just "ply"
	// next line should show we are a binary mode ply.
	std::string cmd;
	while( inHeader && infi )
	{
		infi >> cmd;
		cout << cmd << endl;
		if( cmd.compare("ply") == 0 )
			gotply == true;
		else if( cmd.compare("format") == 0 )
		{
			std::string s;
			float v;
			infi >> s;
			infi >> v;
			assert( s.compare("ascii") == 0 );
			assert( v == 1.0f );
		}
		else if( cmd.compare("element") == 0 )
		{
			if( inElement )
				elements.push_back( curElement );
			curElement.Clear();
			
			infi >> curElement.name;
			infi >> curElement.number;
			inElement = true;
		}
		else if( cmd.compare("property") == 0 )
		{
			std::string t,p;
			infi >> t;
			infi >> p;
			curElement.propTypes.push_back( t );
			curElement.properties.push_back( p );
		}
		else if( cmd.compare("end_header") == 0 )
		{
			inHeader = false;
			elements.push_back( curElement );
		}
	}
	
	cout << "Read .ply header:" << endl;
	cout << "elements: " << elements.size() << endl;
	for( unsigned ec = 0; ec < elements.size(); ++ec )
	{
		cout << "\t" << elements[ec].name << " " << elements[ec].number << " : np : " << elements[ec].properties.size() << endl;
	}
	
	if( elements.size() > 1 )
	{
		cout << "expected only 1 element. Failing." << endl;
		exit(0);
	}
	
	int vc = 0;
	genRowMajMatrix ret = genRowMajMatrix::Zero( elements[0].number, 6 );
	while( std::getline(infi, line) )
	{
		auto ss = SplitLine( line, " " );
		if( ss.size() == 6 )
		{
			ret.row(vc) << atof( ss[0].c_str() ), atof( ss[1].c_str() ), atof( ss[2].c_str() ), atof( ss[3].c_str() )/255.0f, atof( ss[4].c_str() )/255.0f, atof( ss[5].c_str() )/255.0f;
			++vc;
		}
	}
	
	
	
	return ret;
};






//
// More robust loader for more generic ply files (binary)
//
genRowMajMatrix LoadPlyPointNormalRGB( std::string infn )
{
	std::ifstream infi( infn, std::ios::binary );
	
	bool gotData   = false;
	bool inHeader  = true;
	bool gotply    = false;
	bool gotformat = false;
	std::string line;
	
	bool inElement = false;
	SElement curElement;
	std::vector< SElement > elements;
	
	// first line should be just "ply"
	// next line should show we are a binary mode ply.
	while( inHeader && std::getline(infi, line) )
	{
		auto ss = SplitLine( line, " " );
		if( ss.size() == 1 && ss[0].compare("ply") == 0 )
			gotply = true;
		else if( ss.size() == 3 && ss[0].compare("format") == 0 && ss[1].compare("binary_little_endian") == 0 && ss[2].compare("1.0") == 0 )
			gotformat = true;
		else if( ss.size() == 3 && ss[0].compare("format") == 0 )
		{
			cout << "format line not binary" << endl;
			cout << line << endl;
			infi.close();
			return genRowMajMatrix::Zero( 0, 0 );
		}
		else if( ss.size() == 3 && ss[0].compare("element") == 0 )
		{
			if( inElement )
				elements.push_back( curElement );
			curElement.Clear();
			curElement.name = ss[1];
			curElement.number  = std::atoi( ss[2].c_str() );
			inElement = true;
		}
		else if( ss.size() == 3 && ss[0].compare("property") == 0 && inElement )
		{
			curElement.propTypes.push_back( ss[1] );
			curElement.properties.push_back( ss[2] );
		}
		else if( ss.size() == 1 && ss[0].compare("end_header") == 0 )
		{
			inHeader = false;
			if( inElement )
				elements.push_back( curElement );
		}
	}
	
	cout << "Read .ply header:" << endl;
	cout << "elements: " << elements.size() << endl;
	for( unsigned ec = 0; ec < elements.size(); ++ec )
	{
		cout << "\t" << elements[ec].name << " " << elements[ec].number << " : np : " << elements[ec].properties.size() << endl;
	}
	
	int ec = 0;
	while( elements[ec].name != "vertex" )
		++ec;
	
	if( ec >= elements.size() )
	{
		cout << "no vertex element, not loading point cloud file" << endl;
		exit(0);
	}
	
	
	// return a matrix x,y,z,    nx,ny,nz,    r,g,b,a
	genRowMajMatrix ret = genRowMajMatrix::Zero( elements[ec].number, 3 + 4 + 3 );
	
	
	float           vf;
	unsigned char vuch; 
	int column;
	for( unsigned vc = 0; vc < elements[ec].number; ++vc )
	{
		bool gotAlpha = false;
		for( unsigned pc = 0; pc < elements[ec].properties.size(); ++pc )
		{
			if     ( elements[ec].properties[pc].compare( "x" ) == 0 )
				column = 0;
			else if( elements[ec].properties[pc].compare( "y" ) == 0 )
				column = 1;
			else if( elements[ec].properties[pc].compare( "z" ) == 0 )
				column = 2;
			else if( elements[ec].properties[pc].compare( "nx" ) == 0 )
				column = 3;
			else if( elements[ec].properties[pc].compare( "ny" ) == 0 )
				column = 4;
			else if( elements[ec].properties[pc].compare( "nz" ) == 0 )
				column = 5;
			else if( elements[ec].properties[pc].compare( "red" ) == 0 )
				column = 6;
			else if( elements[ec].properties[pc].compare( "green" ) == 0 )
				column = 7;
			else if( elements[ec].properties[pc].compare( "blue" ) == 0 )
				column = 8;
			else if( elements[ec].properties[pc].compare( "alpha" ) == 0 )
			{
				gotAlpha = true;
				column = 9;
			}
			
			if( elements[ec].propTypes[pc].compare( "float" ) == 0 )
				infi.read( (char*)  &vf, sizeof(float ) );
			else if( elements[ec].propTypes[pc].compare( "uchar" ) == 0 )
			{
				infi.read( (char*)&vuch, sizeof(unsigned char ) );
				vf = (float)vuch;
				vf /= 255.0f;
			}
			else
				throw( std::runtime_error( "ply reader only handling float and uchar - you have something else" ) );
			
			ret( vc, column ) = vf;
		}
		
		if( !gotAlpha )
			ret( vc, 9 ) = 1.0f;
	}
	
	
	if( infi.fail() )
	{
		cout << "error reading data block" << endl;
		exit(0);
	}
	
	
	
	return ret;
}
