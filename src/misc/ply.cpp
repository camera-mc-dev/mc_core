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

void ReadPlyHeader( std::ifstream &infi,   bool &out_isBinary, std::vector< SElement > &out_elements )
{
	bool inHeader  = true;
	bool gotply    = false;
	bool gotformat = false;
	
	bool inElement = false;
	SElement curElement;
	out_elements.clear();
	
	std::string line;
	
	while( inHeader && std::getline(infi, line) )
	{
		auto ss = SplitLine( line, " " );
		if( ss.size() == 1 && ss[0].compare("ply") == 0 )
			gotply = true;
		else if( ss.size() == 3 && ss[0].compare("format") == 0 && ss[1].compare("binary_little_endian") == 0 && ss[2].compare("1.0") == 0 )
		{
			gotformat = true;
			out_isBinary = true;
		}
		else if( ss.size() == 3 && ss[0].compare("format") == 0 && ss[1].compare("ascii") == 0 && ss[2].compare("1.0") == 0 )
		{
			gotformat = true;
			out_isBinary = false;
		}
		else if( ss.size() == 3 && ss[0].compare("format") == 0 )
		{
			cout << "format line not understood" << endl;
			cout << line << endl;
			return;
		}
		else if( ss.size() == 3 && ss[0].compare("element") == 0 )
		{
			if( inElement )
				out_elements.push_back( curElement );
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
				out_elements.push_back( curElement );
		}
	}
	
	if( !gotply || !gotformat )
	{
		throw std::runtime_error( "did not get 'ply', nor 'format' lines" );
	}
	
	return;
	
}


std::map<std::string, genRowMajMatrix> ReadPlyData( std::ifstream &infi, std::vector< SElement > &elements, bool isBinary )
{
	std::map<std::string, genRowMajMatrix> out;
	SElement &e = elements[0];   // we've already checked that there's only 1 element and that it is "vertex"
	
	bool gotxyz  = false;
	bool gotnorm = false;
	bool gotrgb  = false;
	bool gotfdc  = false;
	int  numfr   = 0;
	bool gotop   = false;
	bool gotsc   = false;
	bool gotrot  = false;
	for( size_t pc = 0; pc < e.properties.size(); ++pc )
	{
		if(( e.properties[pc].compare("x") == 0  ||
		     e.properties[pc].compare("y") == 0  ||
		     e.properties[pc].compare("z") == 0 ) && !gotxyz   )
		{
			out["xyz"] = genRowMajMatrix::Zero( e.number, 3 );
			gotxyz = true;
		}
		
		else if(( e.properties[pc].compare("nx") == 0  ||
			      e.properties[pc].compare("ny") == 0  ||
			      e.properties[pc].compare("nz") == 0 ) && !gotnorm   )
		{
			out["norm"] = genRowMajMatrix::Zero( e.number, 3 );
			gotnorm = true;
		}
		
		else if(( e.properties[pc].compare("red")   == 0  ||
			      e.properties[pc].compare("green") == 0  ||
			      e.properties[pc].compare("blue")  == 0 ) && !gotrgb   )
		{
			out["rgb"] = genRowMajMatrix::Zero( e.number, 3 );
			gotnorm = true;
		}
		
		
		
		
		else if(( e.properties[pc].compare("sx") == 0  ||
			      e.properties[pc].compare("sy") == 0  ||
			      e.properties[pc].compare("sz") == 0 ) && !gotsc   )
		{
			out["gs-scale"] = genRowMajMatrix::Zero( e.number, 3 );
			gotsc = true;
		}
		
		else if(( e.properties[pc].compare("scale_0") == 0  ||
			      e.properties[pc].compare("scale_1") == 0  ||
			      e.properties[pc].compare("scale_2") == 0 ) && !gotsc   )
		{
			out["gs-scale"] = genRowMajMatrix::Zero( e.number, 3 );
			gotsc = true;
		}
		
		else if( e.properties[pc].find("f_dc") == 0  && !gotfdc )
		{
			gotfdc = true;
			out["gs-fdc"] = genRowMajMatrix::Zero( e.number, 3 );
		}
		else if( e.properties[pc].find("f_rest") == 0 )
		{
			int a = e.properties[pc].rfind("_");
			std::string ns( e.properties[pc].begin()+a+1, e.properties[pc].end() );
			numfr = std::max( numfr, atoi( ns.c_str() ) + 1 );
		}
		
		else if( e.properties[pc].find("rot") == 0  && !gotrot )
		{
			gotrot = true;
			out["gs-qwxyz"] = genRowMajMatrix::Zero( e.number, 4 );
		}
		else if( e.properties[pc].find("opacity") == 0  && !gotop )
		{
			gotrot = true;
			out["gs-opacity"] = genRowMajMatrix::Zero( e.number, 1 );
		}
	}
	
	if( numfr > 0 )
		out["gs-frest"] = genRowMajMatrix::Zero( e.number, numfr );
	
	cout << endl << "-out check-" << endl;
	for( auto i = out.begin(); i != out.end(); ++i )
		cout << i->first << " " << i->second.rows() << " " << i->second.cols() <<  endl;
	cout << "--" << endl;
	
	std::string line;
	std::vector<std::string> tokens;
	for( unsigned c = 0; c < e.number; ++c )
	{
		if( !isBinary )
		{
			std::getline(infi, line);
			tokens = SplitLine(line," \t");
			assert( tokens.size() == e.properties.size() );
		}
		
		for( size_t pc = 0; pc < e.properties.size(); ++pc )
		{
			float v;
			if( e.propTypes[pc].compare("float") == 0 )
				if( isBinary )
						infi.read( (char*)&v, sizeof( float ) );
				else
					v = atof( tokens[pc].c_str() );
			else if( e.propTypes[pc].compare("uchar") == 0 )
			{
				if( isBinary )
				{
					unsigned char ucv;
					infi.read( (char*)&ucv, sizeof(unsigned char) );
					v = (float)ucv / 255.0f;
				}
				else
				{
					v = atof( tokens[pc].c_str() ) / 255.0f;
				}
			}
			else
			{
				cout << e.properties[pc] << " " << e.propTypes[pc] << endl;
				throw std::runtime_error( "not ready for other property types" );
			}
			
			
			// now put that value in the right place...
			// more ugly :(
			if( e.properties[ pc ].compare("x") == 0 )
				out["xyz"](c,0) = v;
			else if( e.properties[ pc ].compare("y") == 0 )
				out["xyz"](c,1) = v;
			else if( e.properties[ pc ].compare("z") == 0 )
				out["xyz"](c,2) = v;
			
			
			else if( e.properties[ pc ].compare("nx") == 0 )
				out["norm"](c,0) = v;
			else if( e.properties[ pc ].compare("ny") == 0 )
				out["norm"](c,1) = v;
			else if( e.properties[ pc ].compare("nz") == 0 )
				out["norm"](c,2) = v;
			
			
			else if( e.properties[ pc ].compare("red") == 0 )
				out["rgb"](c,0) = v;
			else if( e.properties[ pc ].compare("green") == 0 )
				out["rgb"](c,1) = v;
			else if( e.properties[ pc ].compare("blue") == 0 )
				out["rgb"](c,2) = v;
			
			
			
			else if( e.properties[ pc ].compare("sx") == 0 )
				out["gs-scale"](c,0) = v;
			else if( e.properties[ pc ].compare("sy") == 0 )
				out["gs-scale"](c,1) = v;
			else if( e.properties[ pc ].compare("sz") == 0 )
				out["gs-scale"](c,2) = v;
			
			else if( e.properties[ pc ].compare("scale_0") == 0 )
				out["gs-scale"](c,0) = v;
			else if( e.properties[ pc ].compare("scale_1") == 0 )
				out["gs-scale"](c,1) = v;
			else if( e.properties[ pc ].compare("scale_2") == 0 )
				out["gs-scale"](c,2) = v;
			
			
			
			else if( e.properties[ pc ].compare("f_dc_0") == 0 )
				out["gs-fdc"](c,0) = v;
			else if( e.properties[ pc ].compare("f_dc_1") == 0 )
				out["gs-fdc"](c,1) = v;
			else if( e.properties[ pc ].compare("f_dc_2") == 0 )
				out["gs-fdc"](c,2) = v;
			
			
			
			
			else if( e.properties[ pc ].compare("rot_0") == 0 )
				out["gs-qwxyz"](c,0) = v;
			else if( e.properties[ pc ].compare("rot_1") == 0 )
				out["gs-qwxyz"](c,1) = v;
			else if( e.properties[ pc ].compare("rot_2") == 0 )
				out["gs-qwxyz"](c,2) = v;
			else if( e.properties[ pc ].compare("rot_3") == 0 )
				out["gs-qwxyz"](c,3) = v;
			
			
			else if( e.properties[ pc ].compare("opacity") == 0 )
				out["gs-opacity"](c,0) = v;
			
			
			else if( e.properties[ pc ].find("f_rest") == 0 )
			{
				int a = e.properties[pc].rfind("_");
				std::string ns( e.properties[pc].begin()+a+1, e.properties[pc].end() );
				int k = atoi( ns.c_str() );
				out["gs-frest"](c,k) = 0;
			}
		}
	}
	
	
	
	if( infi.fail() )
	{
		cout << "error reading data block" << endl;
		exit(0);
	}
	
	
	return out;
}





std::map<std::string, genRowMajMatrix> LoadPlyPointCloud( std::string infn )
{
	std::ifstream infi( infn, std::ios::binary );
	
	bool gotData   = false;
	bool isBinary  = false;
	std::vector<SElement> elements;
	ReadPlyHeader( infi, isBinary, elements );
	
	
	if( elements.size() > 1 )
	{
		cout << "expected only 1 element: 'vertex' " << endl;
		cout << "got all these instead: " << endl;
		for( unsigned ec = 0; ec < elements.size(); ++ec )
		{
			cout << "\t" << elements[ec].name << " " << elements[ec].number << " : np : " << elements[ec].properties.size() << endl;
		}
		std::map<std::string, genRowMajMatrix> out;
		return out;
	}
	
	if( elements[0].name.compare( "vertex" ) != 0 )
	{
		cout << "expected only 1 element: 'vertex' " << endl;
		cout << "got all these instead: " << endl;
		for( unsigned ec = 0; ec < elements.size(); ++ec )
		{
			cout << "\t" << elements[ec].name << " " << elements[ec].number << " : np : " << elements[ec].properties.size() << endl;
		}
		std::map<std::string, genRowMajMatrix> out;
		return out;
	}
	
	
	if( isBinary )
	{
		return ReadPlyData( infi, elements, isBinary );
	}
}


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
