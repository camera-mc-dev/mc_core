#ifndef MC_I3DRENDERER_H
#define MC_I3DRENDERER_H


#include <chrono>
#include "renderer2/basicRenderer.h"
#include "calib/calibration.h"

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
			viewCalib = c;
			this->near = near;
			this->far  = far;
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
		
		Calibration GetViewCalib()
		{
			return viewCalib;
		}
		
	protected:
		
		virtual void FinishConstructor();
		virtual void InitialiseGraphs();
		virtual bool HandleEvents();
		virtual void HandlePivotCamera(float ft);
		virtual void HandleOrbitCamera(float ft);
		
		
		hVec2D prevMousePos;
		bool leftMousePressed, rightMousePressed;
		
		void TransformView( transMatrix3D T );
		void RotateView( hVec2D mm, float time );
		void OrbitView( hVec2D mm, float time );
		
		Calibration viewCalib;
		float near, far;
		
		enum renderMode_t {I3DR_ORBIT, I3DR_PIVOT};
		renderMode_t renMode;
		float  viewDist;
		
		std::shared_ptr<Rendering::MeshNode> viewCentNode;
		
		
		std::chrono::time_point<std::chrono::steady_clock> prevRenderTime;
	};
};


#endif
