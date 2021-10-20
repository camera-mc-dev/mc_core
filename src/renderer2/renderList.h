#ifndef RENDER_LIST_H
#define RENDER_LIST_H

#include "math/mathTypes.h"
#include "renderer2/sceneGraph.h"




#include <stack>
#include <iostream>
using std::endl;
using std::cout;

#include <fstream>

namespace Rendering
{
	class AbstractRenderer;
	
	
	// the scene graph is a way of placing renderable elements
	// in a fairly user friendly way - position lights,
	// position models, etc.
	//
	// The scene graph then gets parsed to produce the render list,
	// which is stepped through in order to actually perform the
	// rendering.
	//
	// The render list consists of a sequence of render actions,
	// where each render-action is basically an OpenGL call,
	// (more or less).
	//
	// Render actions affect aspects of the render
	// state. We need to keep track of some of that
	// state.
	class RenderState
	{
	public:
		transMatrix3D modelMatrix;
		transMatrix3D projMatrix;
	};



	class RenderAction
	{
	public:
		RenderAction(AbstractRenderer* in_ren)
		{
			renderer = in_ren;
		}
		virtual ~RenderAction()
		{	}

		virtual void Perform(RenderState &renderState) = 0;

		virtual void Print()
		{
			cout << "abstract render action " << endl;
		}

		transMatrix3D L;	// where in the world is this applied? (if it needs a location)

	protected:
		AbstractRenderer* renderer;
		std::string name;

	};





	class RenderList
	{
	public:
		RenderList( AbstractRenderer* renderer );
		~RenderList();

		void Render(std::weak_ptr<CameraNode> activeCamera);

		std::vector< std::shared_ptr<RenderAction> > renderActions;

		AbstractRenderer* GetRenderer()
		{
			return renderer;
		}

	protected:
		AbstractRenderer* renderer;

		RenderState renderState;
		friend class SceneNode;

	};
}
#endif
