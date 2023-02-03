#ifndef BASIC_RENDERER
#define BASIC_RENDERER

#include "renderer2/baseRenderer.h"
#include "renderer2/renWrapper.h"


// the basic renderer sets up three scene graphs:
//
// 0) a background 2D-scene graph with an orthographic projection such that
//    we look along positive z with a top-left image origin, and coordinates
//    match pixels.
// 1) a 3D scene graph with a perspective projection.
// 2) a foreground overlap 2D scene graph otherwise the same as graph 0.
//
// the cameras for all graphs can of course be changed.
//
namespace Rendering
{
	// forward declarations
	class SDFText;

	class BasicRenderer : public BaseRenderer
	{
		friend class Rendering::RendererFactory;
		template<class T0, class T1 > friend class RenWrapper;
		
	protected:
		BasicRenderer(unsigned width, unsigned height, std::string title);
					
	public:
		
		virtual ~BasicRenderer();

		std::shared_ptr<SceneNode> Get2dBgRoot()
		{
			return scenes[0].rootNode;
		}

		std::shared_ptr<SceneNode> Get3dRoot()
		{
			return scenes[1].rootNode;
		}

		std::shared_ptr<SceneNode> Get2dFgRoot()
		{
			return scenes[2].rootNode;
		}

		std::shared_ptr<CameraNode> Get2dBgCamera()
		{
			return scenes[0].activeCamera;
		}

		std::shared_ptr<CameraNode> Get3dCamera()
		{
			return scenes[1].activeCamera;
		}

		std::shared_ptr<CameraNode> Get2dFgCamera()
		{
			return scenes[2].activeCamera;
		}

		virtual bool StepEventLoop();
		virtual void RunEventLoop();
		virtual void RenderToTexture( cv::Mat &res );

		std::shared_ptr<MeshNode> SetBGImage(cv::Mat img);
		void DeleteBGImage();

	protected:

		virtual void InitialiseGraphs();

		// convenience for putting up a simple background image.
		std::shared_ptr<Mesh>     bgImgCard;
		std::shared_ptr<Texture>  bgImgTex;
		std::shared_ptr<MeshNode> bgImgNode;


	};
	
	
	class BasicPauseRenderer : public BasicRenderer
	{
	public:
		friend class Rendering::RendererFactory;		

		bool Step(bool &paused, bool &advance)
		{
			win.setActive();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			Render();

			sf::Event ev;
			while( win.pollEvent(ev) )
			{
				if( ev.type == sf::Event::Closed )
					return true;
				
				if( ev.type == sf::Event::KeyReleased )
				{
					if (ev.key.code == sf::Keyboard::Space )
					{
						paused = !paused;
					}
					if (paused && ev.key.code == sf::Keyboard::Up )
					{
						advance = true;
					}
				}
				
			}
			return false;
		}
	protected:
		BasicPauseRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title)
		{

		}
	};
}

#endif
