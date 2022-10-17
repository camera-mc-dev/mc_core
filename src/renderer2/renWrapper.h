#ifndef MC_RENDERWRAPPER_H
#define MC_RENDERWRAPPER_H

#include "renderer2/basicRenderer.h"
#include "renderer2/basicHeadlessRenderer.h"


//
// Sometimes a tool might want to have the option between using a normal renderer 
// with a window, or using a windowless / headless renderer.
// 
// The two options depend on different branches of the renderer tree and I don't
// see that they would be especially amenable to re-combination. Nicer (I think)
// to have a wrapper structure around two renderers.
//
template <class T0, class T1>
class RenWrapper
{
public:
	RenWrapper( bool useHeadless, float winW, float winH, std::string title )
	{
		headless = useHeadless;
		if( headless )
		{
			Rendering::RendererFactory::Create( hren, winW, winH, title);
		}
		else
		{
			Rendering::RendererFactory::Create( ren, winW, winH, title);
		}
	}
	
	std::shared_ptr<Rendering::SceneNode> Get2dBgRoot()
	{
		if( headless )
			return hren->scenes[0].rootNode;
		return ren->scenes[0].rootNode;
	}
	
	std::shared_ptr<Rendering::SceneNode> Get3dRoot()
	{
		if( headless )
			return hren->scenes[1].rootNode;
		return ren->scenes[1].rootNode;
	}
	
	std::shared_ptr<Rendering::SceneNode> Get2dFgRoot()
	{
		if( headless )
			return hren->scenes[2].rootNode;
		return ren->scenes[2].rootNode;
	}
	
	std::shared_ptr<Rendering::CameraNode> Get2dBgCamera()
	{
		if( headless )
			return hren->scenes[0].activeCamera;
		return ren->scenes[0].activeCamera;
	}
	
	std::shared_ptr<Rendering::CameraNode> Get3dCamera()
	{
		if( headless )
			return hren->scenes[1].activeCamera;
		return ren->scenes[1].activeCamera;
	}
	
	std::shared_ptr<Rendering::CameraNode> Get2dFgCamera()
	{
		if( headless )
			return hren->scenes[2].activeCamera;
		return ren->scenes[2].activeCamera;
	}
	
	bool StepEventLoop()
	{
		if( headless )
			return hren->StepEventLoop();
		return ren->StepEventLoop();
	}
	
	std::shared_ptr<Rendering::MeshNode> SetBGImage(cv::Mat img)
	{
		if( headless )
			return hren->SetBGImage(img);
		return ren->SetBGImage(img);
	}
	
	void DeleteBGImage()
	{
		if( headless )
			return hren->DeleteBGImage();
		return ren->DeleteBGImage();
	}
	
	cv::Mat Capture()
	{
		if( headless )
			return hren->Capture();
		return ren->Capture();
	}
	
	std::shared_ptr< Rendering::AbstractRenderer > GetActive()
	{
		if( headless )
			return hren;
		else
			return ren;
	}
	
	std::shared_ptr<T0> ren;
	std::shared_ptr<T1> hren;
	bool headless;
	
};


#endif
