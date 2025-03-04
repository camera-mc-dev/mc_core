#ifndef MC_I3DRENDERER_H
#define MC_I3DRENDERER_H

#include "renderer2/basicRenderer.h"

namespace Rendering
{
	// forward declarations
	class SDFText;
	
	
	//
	// This is an interactive 3D renderer allowing mouse and keyboard movement of the camera.
	// There are 4 scenes:
	//     0) 2D background scene.
	//     1) 3D main scene.       \_ shared 
	//     2) 3D overlay scene.    /  camera
	//     3) 2D overlay scene.
	//
	
	class I3DRenderer : public BaseRenderer
	{
		friend class Rendering::RendererFactory;
		template<class T0, class T1 > friend class RenWrapper;
		
	protected:
		I3DRenderer(unsigned width, unsigned height, std::string title);
					
	public:
		
		virtual ~I3DRenderer();
		
		std::shared_ptr<SceneNode> Get2dBgRoot()
		{
			return scenes[0].rootNode;
		}
		
		std::shared_ptr<SceneNode> Get3dRoot()
		{
			return scenes[1].rootNode;
		}
		
		std::shared_ptr<SceneNode> Get3dOverlayRoot()
		{
			return scenes[2].rootNode;
		}
		
		std::shared_ptr<SceneNode> Get2dOverlayRoot()
		{
			return scenes[3].rootNode;
		}
		
		void Set3DCamera( Calibration c, float near, float far )
		{
			scenes[1].activeCamera->SetFromCalibration( c, near, far );
			scenes[2].activeCamera->SetFromCalibration( c, near, far );
		}
		
		void Set2DCamera( float left, float right, float top, float bottom, float near, float far )
		{
			scenes[0].activeCamera->SetOrthoProjection( left, right, top, bottom, near, far );
			scenes[3].activeCamera->SetOrthoProjection( left, right, top, bottom, near, far );
		}
		
		
		virtual bool StepEventLoop();
		virtual void RunEventLoop();
		virtual void RenderToTexture( cv::Mat &res )
		{
			throw std::runtime_error( "i3DRenderer can't really render to texture - there's no point!" );
		}
		
		
		
	protected:
		
		virtual void InitialiseGraphs();
		
		
	};
};


#endif
