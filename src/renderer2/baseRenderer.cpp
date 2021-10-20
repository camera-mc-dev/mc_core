#include "renderer2/baseRenderer.h"
#include "renderer2/renderActions.h"

#include <iostream>
using std::endl;
using std::cout;

#include <fstream>
#include <streambuf>



Rendering::BaseRenderer::BaseRenderer(unsigned in_width, unsigned in_height, std::string title) : AbstractRenderer(in_width,in_height,title)
{
	// request a modern openGL context.
	// Requesting a 4.1 context seems to work on my Mac
	sf::ContextSettings requestSettings;
	requestSettings.depthBits = 24;
	requestSettings.stencilBits = 8;
	requestSettings.antialiasingLevel = 1;
	requestSettings.majorVersion = 4;
	requestSettings.minorVersion = 1;
	win.create( sf::VideoMode(in_width, in_height), title, sf::Style::Close, requestSettings);
	
	width  = win.getSize().x;
	height = win.getSize().y;
	
#ifndef __APPLE__
	GLenum err = glewInit();
	if( err != GLEW_OK )
	{
		std::stringstream estr; 
		estr << "Error initialising glew: " << err ;
		throw std::runtime_error( estr.str() );
	}
#endif
	
	win.setActive();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	win.display();

	glPixelStorei( GL_UNPACK_ALIGNMENT, 1);

	// on OSX, needed to poll events before the window would show.
	while( !win.isOpen() )
	{
		sf::Event ev;
		while(win.pollEvent(ev))
		{

		}
	}



	cout << "window created: " << win.getSize().x << " " << win.getSize().y << endl;

	cout << "Context settings: " << endl;
	auto cs = win.getSettings();
	cout << "depth bits  : " << cs.depthBits << endl;
	cout << "stencil bits: " << cs.stencilBits << endl;
	cout << "AA level    : " << cs.antialiasingLevel << endl;
	cout << "major vrsion: " << cs.majorVersion << endl;
	cout << "minor vrsion: " << cs.minorVersion << endl;
	cout << "      ---     " << endl;
	cout << "version (GL): " << glGetString(GL_VERSION) << endl;
}


Rendering::BaseRenderer::~BaseRenderer()
{
	win.close();
}

cv::Mat Rendering::BaseRenderer::Capture()
{
	SetActive();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	cv::Mat i( height, width, CV_8UC3 );
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, i.data );
	cv::flip(i,i,0);
	return i;
}


