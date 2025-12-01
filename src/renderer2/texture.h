#ifndef TEXTURE_H
#define TEXTURE_H

#include <opencv2/opencv.hpp>

#include "renderer2/abstractRenderer.h"

namespace Rendering
{
	class AbstractRenderer;

	class Texture
	{
	public:
		std::string id;

		Texture(std::weak_ptr<AbstractRenderer> renderer);
		~Texture();

		void UploadImage( cv::Mat img );

		GLuint GetID()
		{
			return texID;
		}
		
		void SetFilters( GLuint in_magFilter, GLuint in_minFilter );
		
		
	protected:

		GLuint texID;
		unsigned width, height;
		
		GLuint magFilter;
		GLuint minFilter;
		
		

		std::weak_ptr<AbstractRenderer> renderer;

	};
}
#endif
