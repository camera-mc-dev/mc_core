#include "renderer2/basicHeadlessRenderer.h"
#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "renderer2/sdfText.h"
#include "renderer2/showImage.h"
#include "imgio/loadsave.h"


#include <iostream>
using std::cout;
using std::endl;


#include <Eigen/Geometry>

using namespace Rendering;

#include <GL/glew.h>

int main(void)
{


	std::shared_ptr<BasicRenderer> ren;
	try
	{
		Rendering::RendererFactory::Create(ren, 1024, 512, "basic renderer test" );
	}
	catch(const std::exception& e)
	{
		cout << "caught exception: " << e.what() << endl;
		return(0);
	}
	catch(...)
	{
		cout << "caught unknown exception" << endl;
		return(0);
	}


	// 1. Check Renderer and Vendor
	const GLubyte* vendor = glGetString(GL_VENDOR);
	const GLubyte* renderer = glGetString(GL_RENDERER);
	std::cout << "--- OpenGL Info ---" << std::endl;
	std::cout << "Vendor: " << (const char*)vendor << std::endl;
	std::cout << "Renderer: " << (const char*)renderer << std::endl;
	std::cout << "-------------------" << std::endl;

	// 2. Get the raw extensions string (pre-OpenGL 3.0 method)
	const GLubyte* extensions = glGetString(GL_EXTENSIONS);

	if (extensions)
	{
		std::string extString((const char*)extensions);

		std::ofstream extfi( "extensions.txt" );
		extfi << extString << endl;

		// 3. Search for the QuadroSync-related extensions
		if (extString.find("GL_NV_swap_group") != std::string::npos)
		{
			std::cout << "Found: GL_NV_swap_group (Essential for sync groups/barriers)" << std::endl;
		}
		if (extString.find("GL_NV_present_video") != std::string::npos)
		{
			std::cout << "Found: GL_NV_present_video" << std::endl;
		}
		if (extString.find("GL_ARB_sync") != std::string::npos)
		{
			std::cout << "Found: GL_ARB_sync" << std::endl;
		}
	}
	else
	{
		std::cout << "Error: Could not retrieve OpenGL extensions string." << std::endl;
	}

}
