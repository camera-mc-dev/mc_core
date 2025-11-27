#include "renderer2/abstractRenderer.h"
#include "renderer2/renderActions.h"

#include <iostream>
using std::endl;
using std::cout;

#include <fstream>
#include <streambuf>


Rendering::AbstractRenderer::AbstractRenderer(unsigned width, unsigned height, std::string title) : renderList(this)
{
	// nothing happens here. Children will have to allocate an OpenGL context.
	// the two direct children should be the WindowedRenderer using SFML and 
	// the HeadlessRenderer using EGL
}

void Rendering::AbstractRenderer::FinishConstructor()
{
	// create the blank texture.
	cv::Mat blankImg( 10, 10, CV_8UC3, cv::Scalar(255,255,255) );
	
	blankTexture.reset( new Texture(smartThis) );
	blankTexture->UpMCDLoadImage( blankImg );
}


Rendering::AbstractRenderer::~AbstractRenderer()
{
}


void Rendering::AbstractRenderer::Render()
{
	SetActive();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ParseSceneGraphs();
	for( unsigned sc = 0; sc < scenes.size(); ++sc )
	{
		scenes[sc].Render();
	}
	
	SwapBuffers();
}


bool Rendering::AbstractRenderer::StepEventLoop()
{
	Render();
	return false;	// false to say that no events happened.
}

void Rendering::AbstractRenderer::RunEventLoop()
{

}



void Rendering::AbstractRenderer::ParseSceneGraphs()
{
	for( unsigned sc = 0; sc < scenes.size(); ++sc )
	{
		scenes[sc].Parse();
	}
}







std::string Rendering::AbstractRenderer::ReadTextFile(std::string filename)
{
	std::ifstream infi(filename);
	if( !infi.is_open() )
		throw std::runtime_error(std::string("AbstractRenderer: Could not open text file ") + filename);
	std::string str((std::istreambuf_iterator<char>(infi)),
	                 std::istreambuf_iterator<char>());

	return str;
}

Rendering::VertexShader::VertexShader( std::string source )
{
	GLenum err;
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "error pre vertex shader: " << err << endl;
	}

	shaderID = glCreateShader(GL_VERTEX_SHADER);
	cout << "created vertex shader: " << shaderID << endl;
	if( shaderID == 0 )
	{
		throw std::runtime_error("glCreateShader could not create vertex shader");
	}

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader create: " << err << endl;
	}


	// compile shader.
	const char* sourcep = source.c_str();
	glShaderSource( shaderID, 1, &sourcep, NULL );
	glCompileShader( shaderID );

	// Check Vertex Shader
	GLint result = GL_FALSE;
	int logLength;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
	if ( logLength > 0 )
	{
		std::string errorMessage(logLength+1, ' ');
		glGetShaderInfoLog(shaderID, logLength, NULL, &errorMessage[0]);
		cout << errorMessage << endl;
	}
	if( result != GL_TRUE )
	{
		throw std::runtime_error("Error compiling vertex shader");
	}

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader compile: " << err << endl;
	}

	assert( glIsShader(shaderID) == GL_TRUE );
}

Rendering::VertexShader::~VertexShader()
{
	glDeleteShader(shaderID);
}





Rendering::GeomShader::GeomShader( std::string source )
{
	GLenum err;
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "error pre geom shader: " << err << endl;
	}

	shaderID = glCreateShader(GL_GEOMETRY_SHADER);
	cout << "created geometry shader: " << shaderID << endl;
	if( shaderID == 0 )
	{
		throw std::runtime_error("glCreateShader could not create geometry shader");
	}

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader create: " << err << endl;
	}


	// compile shader.
	const char* sourcep = source.c_str();
	glShaderSource( shaderID, 1, &sourcep, NULL );
	glCompileShader( shaderID );

	// Check Vertex Shader
	GLint result = GL_FALSE;
	int logLength;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
	if ( logLength > 0 )
	{
		std::string errorMessage(logLength+1, ' ');
		glGetShaderInfoLog(shaderID, logLength, NULL, &errorMessage[0]);
		cout << errorMessage << endl;
	}
	if( result != GL_TRUE )
	{
		throw std::runtime_error("Error compiling geometry shader");
	}

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader compile: " << err << endl;
	}

	assert( glIsShader(shaderID) == GL_TRUE );
}

Rendering::GeomShader::~GeomShader()
{
	glDeleteShader(shaderID);
}






Rendering::FragmentShader::FragmentShader( std::string source )
{
	shaderID = glCreateShader(GL_FRAGMENT_SHADER);
	if( shaderID == 0 )
	{
		throw std::runtime_error("glCreateShader could not create fragment shader");
	}
	cout << "created frag shader: " << shaderID << endl;

	// compile shader.
	const char* sourcep = source.c_str();
	glShaderSource( shaderID, 1, &sourcep, NULL );
	glCompileShader( shaderID );

	// Check Vertex Shader
	GLint result = GL_FALSE;
	int logLength;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
	if ( logLength > 0 )
	{
		std::string errorMessage(logLength+1, ' ');
		glGetShaderInfoLog(shaderID, logLength, NULL, &errorMessage[0]);
		cout << errorMessage << endl;
	}
	if( result != GL_TRUE )
	{
		throw std::runtime_error("Error compiling fragment shader");
	}

	assert( glIsShader(shaderID) == GL_TRUE );
}

Rendering::FragmentShader::~FragmentShader()
{
	glDeleteShader(shaderID);
}

Rendering::ShaderProgram::ShaderProgram( std::shared_ptr<VertexShader> vs, std::shared_ptr<FragmentShader> fs )
{
	GLenum err;
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "error pre shader linking: " << err << endl;
	}

	progID = glCreateProgram();
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: create " << err << endl;
	}

	cout << "Creating shader program with ID: " << progID << endl;

	vsID = vs->GetID();
	fsID = fs->GetID();

	assert( glIsShader(vsID) == GL_TRUE);
	assert( glIsShader(fsID) == GL_TRUE);

	glAttachShader(progID, vsID );
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: attach vs " << err << endl;
	}

	glAttachShader(progID, fsID );
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: attach fs " << err << endl;
	}

	glLinkProgram(progID);

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: link " << err << endl;
	}

	GLint result = GL_FALSE;
	int logLength;
	glGetProgramiv(progID, GL_LINK_STATUS, &result);
	glGetProgramiv(progID, GL_INFO_LOG_LENGTH, &logLength);
	if ( logLength > 0 && result != GL_TRUE )
	{
		std::string errorMessage(logLength+1, ' ');
		glGetProgramInfoLog(progID, logLength, NULL, &errorMessage[0]);
		cout << errorMessage << endl;
	}
	if( result != GL_TRUE )
	{
		throw std::runtime_error("Error linking shader");
	}

	// don't need these attached anymore?
	glDetachShader( progID, vsID );
	glDetachShader( progID, fsID );

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "error post shader linking: " << err << endl;
	}

}


Rendering::ShaderProgram::ShaderProgram( std::shared_ptr<VertexShader> vs, std::shared_ptr<GeomShader> gs, std::shared_ptr<FragmentShader> fs )
{
	GLenum err;
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "error pre shader linking: " << err << endl;
	}

	progID = glCreateProgram();
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: create " << err << endl;
	}

	cout << "Creating shader program with ID: " << progID << endl;

	vsID = vs->GetID();
	gsID = gs->GetID();
	fsID = fs->GetID();
	

	assert( glIsShader(vsID) == GL_TRUE);
	assert( glIsShader(fsID) == GL_TRUE);

	glAttachShader(progID, vsID );
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: attach vs " << err << endl;
	}
	
	glAttachShader(progID, gsID );
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: attach gs " << err << endl;
	}

	glAttachShader(progID, fsID );
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: attach fs " << err << endl;
	}

	glLinkProgram(progID);

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "shader: link " << err << endl;
	}

	GLint result = GL_FALSE;
	int logLength;
	glGetProgramiv(progID, GL_LINK_STATUS, &result);
	glGetProgramiv(progID, GL_INFO_LOG_LENGTH, &logLength);
	if ( logLength > 0 && result != GL_TRUE )
	{
		std::string errorMessage(logLength+1, ' ');
		glGetProgramInfoLog(progID, logLength, NULL, &errorMessage[0]);
		cout << errorMessage << endl;
	}
	if( result != GL_TRUE )
	{
		throw std::runtime_error("Error linking shader");
	}

	// don't need these attached anymore?
	glDetachShader( progID, vsID );
	glDetachShader( progID, gsID );
	glDetachShader( progID, fsID );

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "error post shader linking: " << err << endl;
	}

}


Rendering::ShaderProgram::~ShaderProgram()
{
	glDeleteProgram( progID );
}













void Rendering::AbstractRenderer::LoadVertexShader(std::string filename, std::string shaderName)
{
	SetActive();

	// check it doesn't already exists
	if( vertexShaders.count(shaderName) != 0 )
	{
		throw std::runtime_error(std::string("Shader with name already exists: ") + shaderName );
	}

	std::string source = ReadTextFile(filename);
	vertexShaders[shaderName] = std::shared_ptr< VertexShader >( new VertexShader(source) );
}

void Rendering::AbstractRenderer::LoadGeometryShader(std::string filename, std::string shaderName)
{
	SetActive();

	// check it doesn't already exists
	if( geomShaders.count(shaderName) != 0 )
	{
		throw std::runtime_error(std::string("Shader with name already exists: ") + shaderName );
	}

	std::string source = ReadTextFile(filename);
	geomShaders[shaderName] = std::shared_ptr< GeomShader >( new GeomShader(source) );
}

void Rendering::AbstractRenderer::LoadFragmentShader(std::string filename, std::string shaderName)
{
	SetActive();

	// check it doesn't already exists
	if( fragmentShaders.count(shaderName) != 0 )
	{
		throw std::runtime_error(std::string("Shader with name already exists: ") + shaderName );
	}

	std::string source = ReadTextFile(filename);

	fragmentShaders[shaderName] = std::shared_ptr< FragmentShader >( new FragmentShader(source) );
}


void Rendering::AbstractRenderer::CreateShaderProgram( std::string vertexShaderName, std::string fragShaderName, std::string progName )
{
	if( shaderProgs.count(progName) != 0 )
	{
		throw std::runtime_error(std::string("Shader program with name already exists: ") + progName );
	}

	auto vi = vertexShaders.find(vertexShaderName);
	auto fi = fragmentShaders.find( fragShaderName );
	if( vi == vertexShaders.end() )
	{
		throw std::runtime_error( std::string("Can't create shader program, shader with name does not exist: " + vertexShaderName ));
	}
	if( fi == fragmentShaders.end() )
	{
		throw std::runtime_error( std::string("Can't create shader program, shader with name does not exist: " + fragShaderName ));
	}

	cout << "shader IDs for create shader prog: " << endl;
	cout << vi->second->GetID() << " " << fi->second->GetID() << endl;

	shaderProgs[ progName ] = std::shared_ptr< ShaderProgram >( new ShaderProgram( vi->second, fi->second ) );
}

void Rendering::AbstractRenderer::CreateShaderProgram( std::string vertexShaderName, std::string geomShaderName, std::string fragShaderName, std::string progName )
{
	if( shaderProgs.count(progName) != 0 )
	{
		throw std::runtime_error(std::string("Shader program with name already exists: ") + progName );
	}

	auto vi = vertexShaders.find(vertexShaderName);
	auto gi = geomShaders.find(geomShaderName);
	auto fi = fragmentShaders.find( fragShaderName );
	if( vi == vertexShaders.end() )
	{
		throw std::runtime_error( std::string("Can't create shader program, shader with name does not exist: " + vertexShaderName ));
	}
	if( gi == geomShaders.end() )
	{
		throw std::runtime_error( std::string("Can't create shader program, shader with name does not exist: " + geomShaderName ));
	}
	if( fi == fragmentShaders.end() )
	{
		throw std::runtime_error( std::string("Can't create shader program, shader with name does not exist: " + fragShaderName ));
	}

	cout << "shader IDs for create shader prog: " << endl;
	cout << vi->second->GetID() << " " << gi->second->GetID() << " " << fi->second->GetID() << endl;

	shaderProgs[ progName ] = std::shared_ptr< ShaderProgram >( new ShaderProgram( vi->second, gi->second, fi->second ) );
}
