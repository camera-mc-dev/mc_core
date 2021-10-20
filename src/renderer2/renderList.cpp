#include "renderer2/renderList.h"
#include "renderer2/baseRenderer.h"

#include <iostream>
using std::cout;
using std::endl;

Rendering::RenderList::RenderList(AbstractRenderer* in_renderer)
{
	renderer = in_renderer;
}

Rendering::RenderList::~RenderList()
{

}


void Rendering::RenderList::Render(std::weak_ptr<CameraNode> activeCamera)
{
	// just to make sure!
// 	renderer->SetActive();
	
	// we should make this optional.
	glClear(GL_DEPTH_BUFFER_BIT);

	// the camera is at...
	auto ac = activeCamera.lock();
	if(ac)
	{
		transMatrix3D camPos = ac->GetAbsoluteTransformation();

		// so the last transformation to be applied is one that takes 
		// points from world space and puts them in camera space, which
		// is exactly what the camera L does.
		renderState.modelMatrix = camPos;
		renderState.projMatrix  = ac->GetProjection();

		for( unsigned ac = 0; ac < renderActions.size(); ++ac )
		{
			// renderActions[ac]->Print();
			renderActions[ac]->Perform(renderState);
		}
	}
	else
	{
		throw std::runtime_error("What happened to the active camera?");
	}
}
