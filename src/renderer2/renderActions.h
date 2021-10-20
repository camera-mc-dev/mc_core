#ifndef RENDER_ACTIONS_H
#define RENDER_ACTIONS_H

#include "renderer2/baseRenderer.h"
#include "renderer2/renderList.h"
#include "math/mathTypes.h"

#include <vector>
#include <iostream>
using std::cout;
using std::endl;

namespace Rendering
{


	// a monolithic action for rendering textured meshes.
	// note that you can render solid coloured meshes by providing a
	// basic small white image as the texture.
	class DrawMeshAction : public RenderAction
	{
	public:
		DrawMeshAction(AbstractRenderer* in_ren,
		               std::string in_name,
		               GLuint shaderID,
		               std::shared_ptr<Mesh> mesh,
		               GLuint texID,
		               Eigen::Vector4f baseColour
		              );
		virtual ~DrawMeshAction();

		virtual void Perform( RenderState &renderState );

		virtual void Print()
		{
			cout << "draw mesh: " << name << endl;
			cout << "\tshader:  " << shader << endl;
			cout << "\tvao:     " << vao << endl;
			cout << "\tvbo:     " << vbo << endl;
			cout << "\tnbo:     " << nbo << endl;
			cout << "\ttbo:     " << tbo << endl;
			cout << "\tvcbo:    " << vcbo << endl;
			cout << "\tfbo:     " << fbo << endl;
			cout << "\tnumTris: " << numElements << endl;
		}
	private:
		GLuint shader, vao, tex;
		GLuint vbo, tbo, nbo, vcbo, fbo;
		unsigned numElements;	// triangles, line segments, rects, whatever...
		unsigned numVertsPerElement;
		Eigen::Vector4f baseColour;

	};


}


#endif
