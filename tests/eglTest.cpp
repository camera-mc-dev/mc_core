#ifdef USE_EGL

#include "imgio/loadsave.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <fstream>
#include <iostream>
using std::cout;
using std::endl;



// to debug, we're just using a tutorial for a very basic triangle render,
// but we still need shader loading, so we've stolen the tutorial stuff to avoid
// ripping into our renderer code.
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path);

int main( int argc, char *argv[] )
{

	
	
	//
	// Can we find out what EGL devices there are?
	//	
	EGLDeviceEXT eglDevs[10];
	EGLint numDevices;

	// The functions we need are all extensions to EGL, so we need to find them in the API

	// function to query available devices
	PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");
	if( eglQueryDevicesEXT == NULL )
	{
		cout << "Can't get eglQueryDevices extension" << endl;
		exit(0);
	}

	// functions to query information about the devices.
	PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC) eglGetProcAddress("eglQueryDeviceStringEXT");
	PFNEGLQUERYDEVICEATTRIBEXTPROC eglQueryDeviceAttribEXT = (PFNEGLQUERYDEVICEATTRIBEXTPROC) eglGetProcAddress("eglQueryDeviceAttribEXT");

	// so now we ask about up to 10 devices.
	eglQueryDevicesEXT(10, eglDevs, &numDevices);
	cout << "Num devices: " << numDevices << endl;

	// There doesn't seem to be a way of finding a recognisable name for the devices,
	// meaning that we can't easily pick - say - the first Titan, or a 1080, etc...
	// so we'll just take the first device that has drm
	// note that we may be able to export EGL_DEVICE_ID before we start a program to select
	// a specific device, and then maybe not use these extensions?
	int dcToUse = -1;
	for( unsigned dc = 0; dc < numDevices; ++dc )
	{
		// Maybe later versions of EGL can do this - there is a webpage about this being a valid extension.
		// std::string vendor( eglQueryDeviceStringEXT( eglDevs[dc], EGL_VENDOR ) );
		// std::string rendererName( eglQueryDeviceStringEXT( eglDevs[dc], EGL_RENDERER_EXT ) );
		// std::string devName = vendor + " " + rendererName;
		std::string devName( "N/A" );
		std::string devExtensions( eglQueryDeviceStringEXT( eglDevs[dc], EGL_EXTENSIONS ) );
		cout << dc << " : " << devName << " : " << devExtensions << endl;
		
		if( devExtensions.find("drm") != std::string::npos && dcToUse < 0)
		{
			dcToUse = dc;
		}
	}

	// So we've got a device to use. It may not be a specific GPU, but I guess we at least know it has direct rendering.

	//
	// The next thing we do is turn the device into a "display" which doesn't actually include any physical display,
	// nor a need for X, etc.. etc..
	//
	
	// we need this extension function now.
	PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
	
	EGLDisplay eglDpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevs[dcToUse], 0);

	EGLint major, minor;
	eglInitialize(eglDpy, &major, &minor);
	cout << "EGL major, minor: " << major << "," << minor << endl;

	
	// we need to do some configuring of the display.
	EGLint configAttribs[] = 
	{
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE
	};

	EGLint numConfigs;
	EGLConfig eglCfg;
	eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);
	cout << "numConfigs: " << numConfigs << endl;

	// we should now have a headless display (I'm not sure how....) and upon that display we need a surface.


	// This configures the surface
	int renderWidth, renderHeight;
	renderWidth = 800;
	renderHeight = 600;
	EGLint pbufferAttribs[] = 
	{
		EGL_WIDTH, renderWidth,
		EGL_HEIGHT, renderHeight,
		EGL_NONE
	};

	EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);

	eglBindAPI(EGL_OPENGL_API);

	// and then we make the context...
	// I think this is the way to ask for a specific OpenGL version
	// but it fails when I use this.
	// Having said that, Tractor is giving me a 4.6 context anyway
	// and that's what I want, so, hey!
	EGLint ctxAttribs[] =
	{
		EGL_CONTEXT_MAJOR_VERSION, 4,
		EGL_CONTEXT_MINOR_VERSION, 1
	};
	EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);
	if( eglCtx == NULL )
	{
		cout << "Failed making context." << endl;
		exit(0);
	}

	EGLint v;
	eglQueryContext( eglDpy, eglCtx, EGL_CONTEXT_CLIENT_VERSION, &v);
	cout << "v: " << v << endl;

	// and make it current.
	eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);


	// we should be able to ask OpenGL about which version we've got...
	GLint glMajor, glMinor;
	glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glMinor);
	cout << "gl major,minor: " << glMajor << ", " << glMinor << endl;
	cout << glGetString(GL_VERSION) << endl;
	
	
	#ifndef __APPLE__
	GLenum err = glewInit();
	if( err != GLEW_OK )
	{
		std::stringstream estr; 
		estr << "Error initialising glew: " << err ;
		throw std::runtime_error( estr.str() );
	}
	#endif
	
	//
	// We can do whatever OpenGL rendering we want in here now.
	//
	glClearColor(0.8,0.8,0.8,1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	
	GLuint programID = LoadShaders( "eglTest-vshader.glsl", "eglTest-fshader.glsl" );
	glUseProgram(programID);
	
	
	
	GLuint vaid;
	glGenVertexArrays(1, &vaid);
	glBindVertexArray(vaid);
	
	GLfloat verts[] = { 
	                     -1.0f, -1.0f, 0.0f,
	                      1.0f, -1.0f, 0.0f,
	                      0.0f,  1.0f, 0.0f
	                  };
	
	GLuint vbuffer;
	glGenBuffers(1, &vbuffer);
	glBindBuffer( GL_ARRAY_BUFFER, vbuffer );
	glBufferData( GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW );
	
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbuffer);
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, NULL );
	
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisableVertexAttribArray(0);
	
	
	glFlush();
	
	
	cv::Mat res(600,800, CV_8UC3, cv::Scalar(0));
//	glGetTexImage( GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, res.data );
	glReadPixels(0, 0, 800, 600, GL_BGR, GL_UNSIGNED_BYTE, res.data );
	SaveImage(res, "tst.jpg");
	
	
	
	
	
	
	
	
	
	

	// and termintate when done.
	eglTerminate( eglDpy );

	return 0;
}



// to debug, we're just using a tutorial for a very basic triangle render,
// but we still need shader loading, so we've stolen the tutorial stuff to avoid
// ripping into our renderer code.
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path)
{
	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open()){
		std::stringstream sstr;
		sstr << VertexShaderStream.rdbuf();
		VertexShaderCode = sstr.str();
		VertexShaderStream.close();
	}else{
		printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", vertex_file_path);
		getchar();
		return 0;
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::stringstream sstr;
		sstr << FragmentShaderStream.rdbuf();
		FragmentShaderCode = sstr.str();
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
		glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
		printf("%s\n", &VertexShaderErrorMessage[0]);
	}

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
		glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
		printf("%s\n", &FragmentShaderErrorMessage[0]);
	}

	// Link the program
	printf("Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> ProgramErrorMessage(InfoLogLength+1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}
	
	glDetachShader(ProgramID, VertexShaderID);
	glDetachShader(ProgramID, FragmentShaderID);
	
	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

#else

#include <iostream>
using std::cout;
using std::endl;
int main(void)
{
	cout << "Not compiled with EGL" << endl;
}

#endif
