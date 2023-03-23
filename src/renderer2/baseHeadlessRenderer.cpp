#include "renderer2/baseHeadlessRenderer.h"
#include "renderer2/renderActions.h"

#include <iostream>
using std::endl;
using std::cout;

#include <fstream>
#include <streambuf>



Rendering::BaseHeadlessRenderer::BaseHeadlessRenderer(unsigned in_width, unsigned in_height, std::string title) : AbstractRenderer(in_width,in_height,title)
{
	
#ifdef USE_EGL
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
	
	eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevs[dcToUse], 0);

	EGLint major, minor;
	eglInitialize(eglDisplay, &major, &minor);
	cout << "EGL major, minor: " << major << "," << minor << endl;
	
	
	
	// we need to do some configuring of the display.
	// as we're forced to create our own FBO anyway, some of this is probably redundant.
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
	eglChooseConfig(eglDisplay, configAttribs, &eglCfg, 1, &numConfigs);
	cout << "numConfigs: " << numConfigs << endl;

	// we should now have a headless display (I'm not sure how....) and upon that display we need a surface.


	// This configures the surface. We don't actually care how big it is, as we render to texture instead...
	// or maybe we do care?
	EGLint pbufferAttribs[] = 
	{
		EGL_WIDTH, in_width,
		EGL_HEIGHT, in_height,
		EGL_NONE
	};
	
	EGLSurface eglSurface = eglCreatePbufferSurface(eglDisplay, eglCfg, pbufferAttribs);
	if( eglSurface == EGL_NO_SURFACE )
	{
		cout << "didn't get surface." << endl;
		cout << "egl error: " << eglGetError() << endl;
		exit(0);
	}
	
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
	//eglContext = eglCreateContext(eglDisplay, eglCfg, EGL_NO_CONTEXT, ctxAttribs);
	eglContext = eglCreateContext(eglDisplay, eglCfg, EGL_NO_CONTEXT, NULL);
	if( eglContext == NULL )
	{
		cout << "Failed making context." << endl;
		exit(0);
	}
	
	eglSwapInterval( eglDisplay, 0 );
	
	eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
	
	// we should be able to ask OpenGL about which version we've got...
	GLint glMajor, glMinor;
	glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glMinor);
	cout << "gl major,minor: " << glMajor << ", " << glMinor << endl;
	cout << glGetString(GL_VERSION) << endl;
	
	if( glMajor < 4 || glMajor == 4 && glMajor < 1 )
	{
		cout << "EGL created a context with lower than desired OpenGL version. " << glMajor << "." << glMinor << " vs. 4.1" << endl;
		eglTerminate( eglDisplay );
		exit(0);
	}
	
	#ifndef __APPLE__
	GLenum err = glewInit();
	if( err = GLEW_ERROR_NO_GLX_DISPLAY )
	{
		std::stringstream estr;
		cout << "Error initialiseing glew: " << "(" << err << "): " << glewGetErrorString(err);
		cout << " - BUT! some random dude online said we can ignore this error. What's the bets they're right?" << endl;
	}
	else if( err != GLEW_OK )
	{
		std::stringstream estr;
		estr << "Error initialising glew: " << "(" << err << "): " << glewGetErrorString(err);
		throw std::runtime_error( estr.str() );
	}
	
	#endif
	
	
	
	//
	// Now we create a texture to render to.
	//
	// 1) create a frame buffer...
	framebufferName = 0;
	glGenFramebuffers(1, &framebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferName);
	
	// 2) Create the texture we're going to render into.
	width = in_width;
	height = in_height;
	
	glGenTextures(1, &renderTexture);
	glBindTexture(GL_TEXTURE_2D, renderTexture);
	
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, width, height, 0,GL_BGR, GL_UNSIGNED_BYTE, 0);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderTexture, 0);
	
	// 3) And a depth buffer.
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);
	
	
	
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw( std::runtime_error("Didn't make framebuffer...") );
	}
	
	
	
	
// 	glClearColor(0.5,0.1,0.1,1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	cout << "check error: " << glGetError() << endl;
	glFlush();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glFlush();
	
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1);
#else
	cout << "Can't create headless renderer without compiling in support for EGL" << endl;
	
#endif
	
	

}


Rendering::BaseHeadlessRenderer::~BaseHeadlessRenderer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glDeleteTextures(1, &renderTexture );
	glDeleteRenderbuffers( 1, &depthrenderbuffer );
	glDeleteFramebuffers(1, &framebufferName);
	
	#ifdef USE_EGL
	eglDestroySurface( eglDisplay, eglSurface );
	eglTerminate( eglDisplay );
	#endif
}


cv::Mat Rendering::BaseHeadlessRenderer::Capture()
{
// 	cv::Mat res( height, width, CV_8UC3, cv::Scalar(0,0,0) );
// 	SetActive();
// 	glBindTexture(GL_TEXTURE_2D, renderTexture);
// 	glGetTexImage( GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, res.data );
// 	cv::flip(res,res,0);
// 	return res;
	
	cv::Mat res( height, width, CV_8UC3, cv::Scalar(0,0,0) );
	SetActive();
	glBindTexture(GL_TEXTURE_2D, renderTexture);
	//glGetTexImage( GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, res.data );
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, res.data );
	cv::flip(res,res,0);
	return res;
}
