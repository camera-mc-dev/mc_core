#ifndef RENDERER2_H_ME
#define RENDERER2_H_ME


#include "renderer2/abstractRenderer.h"
#include "math/mathTypes.h"
#include "renderer2/sceneGraph.h"

#include <string>
#include <vector>
#include <stack>

#include "renderer2/mesh.h"
#include "renderer2/texture.h"
#include "renderer2/renderList.h"

#include "commonConfig/commonConfig.h"

namespace Rendering
{



	
	class BaseRenderer : public AbstractRenderer
	{
		friend class RendererFactory;
		template<class T0, class T1 > friend class RenWrapper;
		
		// The constructor should be private or protected so that we are forced 
		// to use the factory...
	protected:
		// the constructor creates the renderer with a window of the specified
		// size, and with the specified title.
		BaseRenderer(unsigned width, unsigned height, std::string title);
		
		
		
	public:
		virtual ~BaseRenderer();
		
		
		// make sure this renderer has the active window.
		virtual void SetActive()
		{
			win.setActive();
		}
		
		virtual void SetInactive()
		{
			win.setActive(false);
		}
		
		virtual void SwapBuffers()
		{
			win.display();
		}
		
		void SetWindowPosition(unsigned x, unsigned y)
		{
			win.setPosition( sf::Vector2i(x,y) );
		}
		
		
		virtual cv::Mat Capture();
		
		
	protected:
		
		// sfml stuff - may yet change to another windowing framework.
		sf::Window win;




	};
} // namespace

#endif
