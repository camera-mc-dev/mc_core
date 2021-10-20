#include "renderer2/texture.h"

#include <iostream>
using std::endl;
using std::cout;


Rendering::Texture::Texture(std::weak_ptr<AbstractRenderer> in_renderer)
{
	// texture has to be associated to a specific renderer.
	renderer = in_renderer;
	auto ren = renderer.lock();
	ren->SetActive();
	
	magFilter = GL_LINEAR;
	minFilter = GL_LINEAR;

	// create textureID for texture,
	// and set basic texture parameters.
	glGenTextures( 1, &texID );

	glBindTexture(GL_TEXTURE_2D, texID);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	width = 0;
	height = 0;


}

Rendering::Texture::~Texture()
{
	// get rid of the texture from the GPU?
	auto ren = renderer.lock();
	if( !ren )
		return;	// the renderer is already gone, so no need to do GL clean up.
	
	ren->SetActive();
	glDeleteTextures(1, &texID);
}

void Rendering::Texture::SetFilters( GLuint in_magFilter, GLuint in_minFilter )
{
	magFilter = in_magFilter;
	minFilter = in_minFilter;

	auto ren = renderer.lock();
	ren->SetActive();
	
	glBindTexture(GL_TEXTURE_2D, texID);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter );
}

void Rendering::Texture::UploadImage(cv::Mat img)
{
	GLenum err;

	auto ren = renderer.lock();
	if( !ren )
	{
		throw std::runtime_error("renderer associated to texture no longer valid!");
	}
	ren->SetActive();
	
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "tex pre-upload error (a): " << err << endl;
	}

	width = img.cols;
	height = img.rows;

	if ( glIsTexture( texID ) == GL_FALSE )
	{
		// texture has somehow gone awry...
		cout << "existing texture ID no longer a texture. odd... texID: " << texID << endl;
		glBindTexture(GL_TEXTURE_2D, 0);	// bind no texture.
		glGenTextures(1, &texID );
		
		SetFilters(magFilter, minFilter);
		
		cout << "new ID: " << texID << endl;
		assert( glIsTexture( texID ) == GL_TRUE );
	}
	
	// upload the image as a texture.
	glBindTexture(GL_TEXTURE_2D, texID);


	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "tex pre-upload error (b): " << err << "    - texID: " << texID << endl;
		
		// not sure how this can happen, but we can at least try to compensate... (?)
		ren->SetActive();
		glDeleteTextures(1, &texID);
		glGenTextures(1, &texID);
		cout << " \t new texID: " << texID << endl;
		glBindTexture(GL_TEXTURE_2D, texID);		
		if( err != GL_NO_ERROR )
		{
			throw std::runtime_error("Something has gone wrong uploading a texture.");
		}
	}

	if( img.type() == CV_8UC3 )
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, img.data);
	}
	else if( img.type() == CV_8UC1 )
	{
		// GL_LUMINANCE is deprecated and there's no direct replacement,
		// so the easiest solution is to convert grey to rgb, as horrible a solution as that is.
		cv::Mat img2;
		cv::cvtColor(img, img2, cv::COLOR_GRAY2BGR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img2.cols, img2.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, img2.data);
	}
	else if( img.type() == CV_8UC4 )
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, img.data);
	}
	else if( img.type() == CV_32FC1 )
	{
		cv::Mat img2;
		cv::cvtColor(img, img2, cv::COLOR_GRAY2BGR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img2.cols, img2.rows, 0, GL_BGR, GL_FLOAT, img2.data);
	}
	else if( img.type() == CV_32FC3 )
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0, GL_BGR, GL_FLOAT, img.data);
	}
	else if( img.type() == CV_32FC4 )
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.cols, img.rows, 0, GL_BGRA, GL_FLOAT, img.data);
	}
	

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "tex upload error: " << err << endl;
	}

}
