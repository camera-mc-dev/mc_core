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

typedef Eigen::Matrix<        float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor >  floatMat;
typedef Eigen::Matrix<       double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor > doubleMat;
typedef Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor >  uint8Mat;
typedef Eigen::Matrix<         char, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor >   int8Mat;

struct SPlyData
{
	std::map< std::string,  floatMat >   fProps;
	std::map< std::string, doubleMat >   dProps;
	std::map< std::string,  uint8Mat > ui8Props;
	std::map< std::string,   int8Mat >  i8Props;
};

void ReadElementBinary( std::ifstream &infi, SElement &e, SPlyData &outData )
{
	//
	// plyData is interleaved - for each element you get all the properties together.
	// i.e. a "matrix" of shape [e,n]. Which is simple enough until you remember that all
	// of the properies can be different types, so we can't do something nice like if it was
	// [n, e], i.e., coherent matrices of the same type.
	//
	// But reading property by property from the file is achingly slow...
	//
	
	
	//
	// if we treat everything as bytes, how big is [e,n]?
	//
	unsigned totBytes = 0;
	std::vector< unsigned > propByteCounts( e.properties.size() );
	for( size_t pc = 0; pc < e.properties.size(); ++pc )
	{
		if( e.propTypes[pc].compare("float") == 0 )
		{
			totBytes += sizeof(float);
			propByteCounts[pc] = sizeof(float);
		}
		else if( e.propTypes[pc].compare("double") == 0 )
		{
			totBytes += sizeof(double);
			propByteCounts[pc] = sizeof(double);
		}
		else if( e.propTypes[pc].compare("uchar") == 0 || e.propTypes[pc].compare("uint8") == 0 )
		{
			totBytes += sizeof(unsigned char);
			propByteCounts[pc] = sizeof(unsigned char);
		}
		else if( e.propTypes[pc].compare("char") == 0 || e.propTypes[pc].compare("int8") == 0 )
		{
			totBytes += sizeof(char);
			propByteCounts[pc] = sizeof(char);
		}
		else
		{
			std::stringstream ss;
			ss <<  "unrecognised property type " << e.propTypes[pc] << " in .ply file ";
			throw std::runtime_error( ss.str() );
		}
	}
	
	//
	// Now we can read a big buffer of bytes.
	//
	std::vector< unsigned char > buf( e.number * totBytes );
	infi.read( (char*) &buf[0], e.number * totBytes );
	
	
	//
	// But we still have to reinterpret those raw bytes into individual properties.
	// We can maybe get some help from Eigen on this front if we treat the whole thing as 
	// one big matrix of uint8s...
	//
	Eigen::Map< int8Mat > bufMap( (char*)&buf[0], e.number, totBytes );
	unsigned colCount = 0;
	for( unsigned pc = 0; pc < e.properties.size(); ++pc )
	{
		// so we can take all the relevant columns for this property into its own int8 matrix.
		int8Mat M8 = bufMap.block( 0, colCount, e.number, propByteCounts[pc] );
		
		// and then, if we're lucky, we can use a map to reinterpret the matrix data 
		if( e.propTypes[pc].compare("float") == 0 )
		{
			outData.fProps[ e.properties[pc] ] = Eigen::Map< floatMat >( (float*)M8.data(), M8.rows(), 1 );
		}
		else if( e.propTypes[pc].compare("double") == 0 )
		{
			outData.dProps[ e.properties[pc] ] = Eigen::Map< doubleMat >( (double*)M8.data(), M8.rows(), 1 );
		}
		else if( e.propTypes[pc].compare("uchar") == 0 || e.propTypes[pc].compare("uint8") == 0 )
		{
			outData.ui8Props[ e.properties[pc] ] = Eigen::Map< uint8Mat >( (unsigned char*)M8.data(), M8.rows(), 1 );
		}
		else if( e.propTypes[pc].compare("char") == 0 || e.propTypes[pc].compare("int8") == 0 )
		{
			outData.i8Props[ e.properties[pc] ] = M8;
		}
		colCount += propByteCounts[pc];
	}
	
	//
	// And _maybe_ that's fine?
	//
	return;
}


void ReadElementASCII( std::ifstream &infi, SElement &e, SPlyData &outData )
{
	//
	// With ASCII data we're just going to slowly cycle through the file.
	// _Maybe_ we could be clever and read all of into one big string, then 
	// parse that big string, but... likely as not, an ASCII file will be 
	// a relatively small model.
	//
	for( size_t pc = 0; pc < e.properties.size(); ++pc )
	{
		if( e.propTypes[pc].compare("float") == 0 )
		{
			outData.fProps[ e.properties[pc] ] == floatMat::Zero( e.number, 1 );
		}
		else if( e.propTypes[pc].compare("double") == 0 )
		{
			outData.dProps[ e.properties[pc] ] == doubleMat::Zero( e.number, 1 );
		}
		else if( e.propTypes[pc].compare("uchar") == 0 || e.propTypes[pc].compare("uint8") == 0 )
		{
			outData.ui8Props[ e.properties[pc] ] == uint8Mat::Zero( e.number, 1 );
		}
		else if( e.propTypes[pc].compare("char") == 0 || e.propTypes[pc].compare("int8") == 0 )
		{
			outData.i8Props[ e.properties[pc] ] == int8Mat::Zero( e.number, 1 );
		}
		else
		{
			std::stringstream ss;
			ss <<  "unrecognised property type " << e.propTypes[pc] << " in .ply file ";
			throw std::runtime_error( ss.str() );
		}
	}
	
	
	std::string line;
	std::vector<std::string> tokens;
	for( unsigned c = 0; c < e.number; ++c )
	{
		std::getline(infi, line);
		tokens = SplitLine(line," \t");
		assert( tokens.size() == e.properties.size() );
		
		for( size_t pc = 0; pc < e.properties.size(); ++pc )
		{
			if( e.propTypes[pc].compare("float") == 0 )
			{
				outData.fProps[ e.properties[pc] ]( c, pc ) = atof( tokens[pc].c_str() );
			}
			else if( e.propTypes[pc].compare("double") == 0 )
			{
				outData.dProps[ e.properties[pc] ]( c, pc ) = atof( tokens[pc].c_str() );
			}
			else if( e.propTypes[pc].compare("uchar") == 0 || e.propTypes[pc].compare("uint8") == 0 )
			{
				outData.ui8Props[ e.properties[pc] ]( c, pc ) = atoi( tokens[pc].c_str() );
			}
			else if( e.propTypes[pc].compare("char") == 0 || e.propTypes[pc].compare("int8") == 0 )
			{
				outData.i8Props[ e.properties[pc] ]( c, pc ) = atoi( tokens[pc].c_str() );
			}
		}
	}
}



std::map<std::string, genRowMajMatrix> GetCloudData( SPlyData &plyData )
{
	std::map<std::string, genRowMajMatrix> out;
	
	
	//
	// if we're doing gaussian splatting, we know that the spherical harmonic parameters
	// will be in the "float" properties, and we need to know how many there are.
	//
	int numfr = 0;
	for( auto i = plyData.fProps.begin(); i != plyData.fProps.end(); ++i )
	{
		if( i->first.find("f_rest") == 0 )
		{
			int a = i->first.rfind("_");
			std::string ns( i->first.begin()+a+1, i->first.end() );
			numfr = std::max( numfr, atoi( ns.c_str() ) + 1 );
		}
	}
	if( numfr > 0 )
	{
		out["gs-frest"] = genRowMajMatrix::Zero( plyData.fProps["f_dc_0"].rows() , numfr );
		cout << "frest: " << out["gs-frest"].rows() << " " << out["gs-frest"].cols() << endl;
	}
	
	
	bool gotxyz  = false;
	bool gotnorm = false;
	bool gotrgb  = false;
	bool gotsc   = false;
	bool gotrot  = false;
	bool gotfdc  = false;
	for( auto i = plyData.fProps.begin(); i != plyData.fProps.end(); ++i )
	{
		
		//
		// x,y,z
		//
		if( i->first.compare("x") == 0 )
		{
			if( !gotxyz ){  out["xyz"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotxyz = true; }
			out["xyz"].col(0) = i->second.col(0);
			
// 			cout << "x: " << i->second.col(0).head(10).transpose() << endl;
		}
		else if( i->first.compare("y") == 0 )
		{
			if( !gotxyz ){  out["xyz"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotxyz = true; }
			out["xyz"].col(1) = i->second.col(0);
			
// 			cout << "y: " << i->second.col(0).head(10).transpose() << endl;
		}
		else if( i->first.compare("z") == 0 )
		{
			if( !gotxyz ){  out["xyz"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotxyz = true; }
			out["xyz"].col(2) = i->second.col(0);
			
// 			cout << "z: " << i->second.col(0).head(10).transpose() << endl;
			
// 			cout << out["xyz"].block( 0, 0, 10, 3 ) << endl;
// 			exit(0);
		}
		
		
		//
		// normal (nx,ny,nz)
		//
		if( i->first.compare("nx") == 0 )
		{
			if( !gotnorm ){  out["norm"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotnorm = true; }
			out["norm"].col(0) = i->second.col(0);
		}
		else if( i->first.compare("ny") == 0 )
		{
			if( !gotnorm ){  out["norm"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotnorm = true; }
			out["norm"].col(1) = i->second.col(0);
		}
		else if( i->first.compare("nz") == 0 )
		{
			if( !gotnorm ){  out["norm"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotnorm = true; }
			out["norm"].col(2) = i->second.col(0);
		}
		
		
		
		//
		// rgb (red,green,blue)
		//
		if( i->first.compare("red") == 0 )
		{
			if( !gotrgb ){  out["rgb"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotrgb = true; }
			out["rgb"].col(0) = i->second.col(0);
		}
		else if( i->first.compare("green") == 0 )
		{
			if( !gotrgb ){  out["rgb"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotrgb = true; }
			out["rgb"].col(1) = i->second.col(0);
		}
		else if( i->first.compare("blue") == 0 )
		{
			if( !gotrgb ){  out["rgb"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotrgb = true; }
			out["rgb"].col(2) = i->second.col(0);
		}
		
		
		
		
		
		
		
		
		//
		// The next are gaussian splat specific.
		//
		
		
		//
		// scale (sx,sy,sz)  or (scale_0, scale_1, scale_2)
		//
		if( i->first.compare("sx") == 0   ||   i->first.compare("scale_0") == 0)
		{
			if( !gotsc ){  out["gs-scale"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotsc = true; }
			out["gs-scale"].col(0) = i->second.col(0);
		}
		else if( i->first.compare("sy") == 0   ||   i->first.compare("scale_1") == 0)
		{
			if( !gotsc ){  out["gs-scale"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotsc = true; }
			out["gs-scale"].col(1) = i->second.col(0);
		}
		else if( i->first.compare("sz") == 0   ||   i->first.compare("scale_2") == 0)
		{
			if( !gotsc ){  out["gs-scale"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotsc = true; }
			out["gs-scale"].col(2) = i->second.col(0);
		}
		
		
		
		//
		// dc spherical component
		//
		else if( i->first.compare("f_dc_0") == 0  )
		{
			if( !gotfdc ){  out["gs-fdc"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotfdc = true; }
			out["gs-fdc"].col(0) = i->second;
		}
		else if( i->first.compare("f_dc_1") == 0  )
		{
			if( !gotfdc ){  out["gs-fdc"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotfdc = true; }
			out["gs-fdc"].col(1) = i->second;
		}
		else if( i->first.compare("f_dc_2") == 0  )
		{
			if( !gotfdc ){  out["gs-fdc"] = genRowMajMatrix::Zero( i->second.rows(), 3 ); gotfdc = true; }
			out["gs-fdc"].col(2) = i->second;
		}
		
		
		//
		// remaining spherical components
		//
		else if( i->first.find("f_rest") == 0 )
		{
			int a = i->first.rfind("_");
			std::string ns( i->first.begin()+a+1, i->first.end() );
			int k = atoi( ns.c_str() );
			out["gs-frest"].col(k) = i->second;
		}
		
		
		
		//
		// orientation (quaternion) of splat gaussian orientations.
		//
		else if( i->first.compare("rot_0") == 0  )
		{
			if( !gotrot ){  out["gs-qwxyz"] = genRowMajMatrix::Zero( i->second.rows(), 4 ); gotrot = true; }
			out["gs-qwxyz"].col(0) = i->second;
		}
		else if( i->first.compare("rot_1") == 0  )
		{
			if( !gotrot ){  out["gs-qwxyz"] = genRowMajMatrix::Zero( i->second.rows(), 4 ); gotrot = true; }
			out["gs-qwxyz"].col(1) = i->second;
		}
		else if( i->first.compare("rot_2") == 0  )
		{
			if( !gotrot ){  out["gs-qwxyz"] = genRowMajMatrix::Zero( i->second.rows(), 4 ); gotrot = true; }
			out["gs-qwxyz"].col(2) = i->second;
		}
		else if( i->first.compare("rot_3") == 0  )
		{
			if( !gotrot ){  out["gs-qwxyz"] = genRowMajMatrix::Zero( i->second.rows(), 4 ); gotrot = true; }
			out["gs-qwxyz"].col(3) = i->second;
		}
		
		
		
		//
		// splat gaussian opacity.
		//
		else if( i->first.compare("opacity") == 0  )
		{
			out["gs-opacity"] = i->second;
		}
		
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
	
	//
	// in this specific case, we only want 1 element : "vertex"
	//
	assert( elements.size() == 1 && elements[0].name.compare("vertex") == 0 );
	
	SPlyData outData;
	if( isBinary )
	{
		ReadElementBinary( infi, elements[0], outData );
	}
	else
	{
		ReadElementASCII( infi, elements[0], outData );
	}
	
	
	//
	// My "legacy" implementation means I now need to do some rearranging into convenience matrices.
	//
	return GetCloudData( outData );
	
	
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
