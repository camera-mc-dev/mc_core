#include "misc/obj.h"
#include "misc/tokeniser.h"
#include "renderer2/sceneGraph.h"
#include "imgio/loadsave.h"

#include <fstream>
#include <vector>

#include <iostream>
using std::cout;
using std::endl;


#include <boost/filesystem.hpp>
std::string GetTextureFromMtl( std::string fp, std::string mfn )
{
	// fp is /path/to/file.obj
	boost::filesystem::path p( fp );
	
	// so bp is /path/to/
	auto bp = p.parent_path();
	
	// and thus.. /path/to/mfn
	auto mp = bp / mfn;
	
	std::ifstream infi( mp.string() );
	
	// super lazy...
	std::string x;
	while( infi )
	{
		infi >> x;
		if( x.find(".jpg") != std::string::npos )
			return (bp / x).string();
	}
	return "none";
}

void LoadOBJ( std::string filepath, std::shared_ptr< Rendering::Mesh > &mesh, cv::Mat &texImg )
{
	cout << "loading .obj from: " << filepath << endl;
	std::ifstream infi( filepath );
	if( !infi )
	{
		cout << "couldn't open file" << endl;
	}
	// .obj isn't typically nice enough to tell us how many
	// vertices etc.. there are in advance. So...
	std::vector< hVec3D > vertices, normals;
	std::vector< Eigen::Vector2f > texCoords;
	std::vector< std::string > faces;
	std::string textureFile = "none";
	
	std::string line;
	while( std::getline(infi, line) )
	{
		auto tokens = SplitLine( line, " " );
		
		if( tokens[0].compare("v") == 0 )
		{
			// vertex
			hVec3D v;
			v << atof( tokens[1].c_str() ), atof( tokens[2].c_str() ), atof( tokens[3].c_str() ), 1.0f;
			vertices.push_back( v );
		}
		else if( tokens[0].compare("vn") == 0 )
		{
			// vertex normal
			hVec3D vn;
			vn << atof( tokens[1].c_str() ), atof( tokens[2].c_str() ), atof( tokens[3].c_str() ), 0.0f;
			normals.push_back( vn );
		}
		else if( tokens[0].compare("vt") == 0 )
		{
			// texture coord
			Eigen::Vector2f texCoord;
			texCoord << atof( tokens[1].c_str() ), atof( tokens[2].c_str() );
			texCoords.push_back( texCoord );
		}
		else if( tokens[0].compare("f") == 0 )
		{
			// face
			faces.push_back( line ); // we'll process faces later.
		}
		else if( tokens[0].find("#") == 0 )
		{
			// comment
		}
		else if( tokens[0].compare( "mtllib" ) == 0 )
		{
			// material file
			textureFile = GetTextureFromMtl( filepath, tokens[1] );
			
		}
		else if( tokens[0].compare( "usemtl" ) == 0 )
		{
			// which material to use.
			// ignoring :)
		}
		else
		{
			cout << ".obj reader: Didn't understand line: " << endl;
			cout << line << endl;
			exit(0);
		}
	}
	
	
	cout << "Loaded " << filepath << endl;
	cout << "verts: " << vertices.size()  << endl;
	cout << "norms: " << normals.size()   << endl;
	cout << "texs : " << texCoords.size() << endl;
	cout << "faces: " << faces.size() << endl;
	
	assert( texCoords.size() >= vertices.size() ); // because I don't know what to do otherwise.
	
	//
	// The .obj file allows for a face to use vertex n multiple times with _different_ texture coordinates.
	// That's fine but my mesh structure doesn't allow for that. 
	// As such  we need to start by parsing the faces so that we figure out how many unique 
	// vertices we have.
	//
	size_t id = 0;
	std::unordered_map<std::string, size_t> key2id;
	for( unsigned c = 0; c < faces.size(); ++c )
	{
		auto tokens = SplitLine( faces[c], " " );
		
		assert( tokens.size() == 4 ); // i.e. [ "f", "vert0", "vert1", "vert2" ]
		
		if( key2id.find( tokens[1] ) == key2id.end() )
			key2id[ tokens[1] ] = id++;
		
		if( key2id.find( tokens[2] ) == key2id.end() )
			key2id[ tokens[2] ] = id++;
		
		if( key2id.find( tokens[3] ) == key2id.end() )
			key2id[ tokens[3] ] = id++;
	}
	
	
	// now we know how many unique vertices we have we can create the mesh.
	mesh.reset( new Rendering::Mesh( key2id.size(), faces.size(), 3 ) ); // force triangles for now.
	
	
	
	for( unsigned c = 0; c < faces.size(); ++c )
	{
		auto tokens = SplitLine( faces[c], " " );
		
		// we need to be more flexible in the future, but this is a start...
		auto v0tok = SplitLine( tokens[1], "/" ); assert( v0tok.size() == 3 );
		auto v1tok = SplitLine( tokens[2], "/" ); assert( v1tok.size() == 3 );
		auto v2tok = SplitLine( tokens[3], "/" ); assert( v2tok.size() == 3 );
		
		// the token is the unique key for the vertex
		auto id0 = key2id[ tokens[1] ];
		auto id1 = key2id[ tokens[2] ];
		auto id2 = key2id[ tokens[3] ];
		
		// fill things in - there may be duplication here.
		
		int v0v = atoi( v0tok[0].c_str() )-1;
		int v0t = atoi( v0tok[1].c_str() )-1;
		int v0n = atoi( v0tok[2].c_str() )-1;
		
		int v1v = atoi( v1tok[0].c_str() )-1;
		int v1t = atoi( v1tok[1].c_str() )-1;
		int v1n = atoi( v1tok[2].c_str() )-1;
		
		int v2v = atoi( v2tok[0].c_str() )-1;
		int v2t = atoi( v2tok[1].c_str() )-1;
		int v2n = atoi( v2tok[2].c_str() )-1;
		
		
		
		mesh->vertices.col(  id0 )  = vertices[ v0v ];
		mesh->normals.col(   id0 )   = normals[ v0n ];
		mesh->texCoords.col( id0 ) = texCoords[ v0t ];
		
		mesh->vertices.col(  id1 )  = vertices[ v1v ];
		mesh->normals.col(   id1 )   = normals[ v1n ];
		mesh->texCoords.col( id1 ) = texCoords[ v1t ];
		
		mesh->vertices.col(  id2 )  = vertices[ v2v ];
		mesh->normals.col(   id2 )   = normals[ v2n ];
		mesh->texCoords.col( id2 ) = texCoords[ v2t ];
		
		mesh->faces.col( c ) << id0, id1, id2;
	}
	
	//
	// load up texture
	//
	
	
	if( textureFile.compare("none") != 0 )
	{
		cv::Mat img = LoadImage( textureFile );
		cv::Mat imgf; cv::flip( img, imgf, 0 );
		texImg = imgf;
	}
	else
	{
		cv::Mat img( 32, 32, CV_8UC3, cv::Scalar(255,255,255) );
	}
}

void LoadOBJ( std::string filepath, std::shared_ptr< Rendering::Mesh > &mesh )
{
	cout << "loading .obj from: " << filepath << endl;
	std::ifstream infi( filepath );
	if( !infi )
	{
		cout << "couldn't open file" << endl;
	}
	// .obj isn't typically nice enough to tell us how many
	// vertices etc.. there are in advance. So...
	std::vector< hVec3D > vertices, normals;
	std::vector< Eigen::Vector2f > texCoords;
	std::vector< std::string > faces;
	std::string textureFile = "none";
	
	std::string line;
	while( std::getline(infi, line) )
	{
		auto tokens = SplitLine( line, " " );
		
		if( tokens[0].compare("v") == 0 )
		{
			// vertex
			hVec3D v;
			v << atof( tokens[1].c_str() ), atof( tokens[2].c_str() ), atof( tokens[3].c_str() ), 1.0f;
			vertices.push_back( v );
		}
		else if( tokens[0].compare("vn") == 0 )
		{
			// vertex normal
			hVec3D vn;
			vn << atof( tokens[1].c_str() ), atof( tokens[2].c_str() ), atof( tokens[3].c_str() ), 0.0f;
			normals.push_back( vn );
		}
		else if( tokens[0].compare("vt") == 0 )
		{
			// texture coord
			Eigen::Vector2f texCoord;
			texCoord << atof( tokens[1].c_str() ), atof( tokens[2].c_str() );
			texCoords.push_back( texCoord );
		}
		else if( tokens[0].compare("f") == 0 )
		{
			// face
			faces.push_back( line ); // we'll process faces later.
		}
		else if( tokens[0].find("#") == 0 )
		{
			// comment
		}
		else if( tokens[0].compare( "mtllib" ) == 0 )
		{
			// material file
			textureFile = GetTextureFromMtl( filepath, tokens[1] );
			
		}
		else if( tokens[0].compare( "usemtl" ) == 0 )
		{
			// which material to use.
			// ignoring :)
		}
		else
		{
			cout << ".obj reader: Didn't understand line: " << endl;
			cout << line << endl;
			exit(0);
		}
	}
	
	
	cout << "Loaded " << filepath << endl;
	cout << "verts: " << vertices.size()  << endl;
	cout << "norms: " << normals.size()   << endl;
	cout << "texs : " << texCoords.size() << endl;
	cout << "faces: " << faces.size() << endl;
	
	assert( texCoords.size() >= vertices.size() ); // because I don't know what to do otherwise.
	
	//
	// The .obj file allows for a face to use vertex n multiple times with _different_ texture coordinates.
	// That's fine but my mesh structure doesn't allow for that. 
	// As such  we need to start by parsing the faces so that we figure out how many unique 
	// vertices we have.
	//
	size_t id = 0;
	std::unordered_map<std::string, size_t> key2id;
	for( unsigned c = 0; c < faces.size(); ++c )
	{
		auto tokens = SplitLine( faces[c], " " );
		
		assert( tokens.size() == 4 ); // i.e. [ "f", "vert0", "vert1", "vert2" ]
		
		if( key2id.find( tokens[1] ) == key2id.end() )
			key2id[ tokens[1] ] = id++;
		
		if( key2id.find( tokens[2] ) == key2id.end() )
			key2id[ tokens[2] ] = id++;
		
		if( key2id.find( tokens[3] ) == key2id.end() )
			key2id[ tokens[3] ] = id++;
	}
	
	
	// now we know how many unique vertices we have we can create the mesh.
	mesh.reset( new Rendering::Mesh( key2id.size(), faces.size(), 3 ) ); // force triangles for now.
	
	
	
	for( unsigned c = 0; c < faces.size(); ++c )
	{
		auto tokens = SplitLine( faces[c], " " );
		
		// we need to be more flexible in the future, but this is a start...
		auto v0tok = SplitLine( tokens[1], "/" ); assert( v0tok.size() == 3 );
		auto v1tok = SplitLine( tokens[2], "/" ); assert( v1tok.size() == 3 );
		auto v2tok = SplitLine( tokens[3], "/" ); assert( v2tok.size() == 3 );
		
		// the token is the unique key for the vertex
		auto id0 = key2id[ tokens[1] ];
		auto id1 = key2id[ tokens[2] ];
		auto id2 = key2id[ tokens[3] ];
		
		// fill things in - there may be duplication here.
		
		int v0v = atoi( v0tok[0].c_str() )-1;
		int v0t = atoi( v0tok[1].c_str() )-1;
		int v0n = atoi( v0tok[2].c_str() )-1;
		
		int v1v = atoi( v1tok[0].c_str() )-1;
		int v1t = atoi( v1tok[1].c_str() )-1;
		int v1n = atoi( v1tok[2].c_str() )-1;
		
		int v2v = atoi( v2tok[0].c_str() )-1;
		int v2t = atoi( v2tok[1].c_str() )-1;
		int v2n = atoi( v2tok[2].c_str() )-1;
		
		
		
		mesh->vertices.col(  id0 )  = vertices[ v0v ];
		mesh->normals.col(   id0 )   = normals[ v0n ];
		mesh->texCoords.col( id0 ) = texCoords[ v0t ];
		
		mesh->vertices.col(  id1 )  = vertices[ v1v ];
		mesh->normals.col(   id1 )   = normals[ v1n ];
		mesh->texCoords.col( id1 ) = texCoords[ v1t ];
		
		mesh->vertices.col(  id2 )  = vertices[ v2v ];
		mesh->normals.col(   id2 )   = normals[ v2n ];
		mesh->texCoords.col( id2 ) = texCoords[ v2t ];
		
		mesh->faces.col( c ) << id0, id1, id2;
	}
}



