#ifndef SCENE_GRAPH_H
#define SCENE_GRAPH_H

#include "math/mathTypes.h"
#include <vector>
#include <map>
#include <deque>
#include <memory>

#include "renderer2/opengl.h"

#include "calib/calibration.h"

namespace Rendering
{

	// forward declaration
	class BaseRenderer;
	class RenderState;
	class ShaderProgram;
	class Texture;
	class RenderList;
	class Mesh;
	class SceneNode;

	// The primary role of scene nodes is
	// to describe the structure of a rendering scene.
	// That is to say, to position the elements of that scene
	// so that they can be rendered. That includes instances of models,
	// but also cameras and lights.
	//
	// A basic scene node has no reference to any data, but merely
	// describes a position and orientation in space relative to
	// a higher scene node.
	class NodeFactory
	{
		public:
		template<class T>
		static void Create(std::shared_ptr<T> &node, std::string id)
		{
			node.reset( new T(id) );
			node->SetThis( node );
		}
	};
	
	class SceneNode
	{
		friend class NodeFactory;
		
	protected:
		SceneNode( std::string id );
		SceneNode();
	
		void SetThis( std::weak_ptr<SceneNode> in_ptr )
		{
			smartThis = in_ptr;
		}
	
	public:
		std::string GetID() { return id; }
		virtual ~SceneNode();

		// add or remove children from the scene node,
		// or find a specific child on the scene graph.
		// removing the child removes it from this
		// node's list of children. It does not remove it
		// from memory. The pointer is returned so that
		// memory can be freed as needed.
		void AddChild( std::shared_ptr<SceneNode> newChild );
		std::shared_ptr<SceneNode> FindChild(std::string id);
		std::shared_ptr<SceneNode> RemoveChild(std::string id);
		void SetParent( std::shared_ptr<SceneNode> parent );
		void RemoveFromParent()
		{
			if( parent )
				parent->RemoveChild( id );
			parent = nullptr;
		}


		// Set a transformation for this scene node.
		// The node maintains a transformation L such that
		// any point beneath this SceneNode will be transformed by L
		// p1 = L.p
		// p at the bottom several layers of scene graph will
		// be transformed by several transforms po = L1.L2.L3.p etc.
		// This absolute transformation, the product with all parent
		// transformations, can also be recovered.
		void SetTransformation(transMatrix3D L);
		void SetTransformation(hVec3D pos, hVec3D rotation);
		transMatrix3D &GetTransformation();
		transMatrix3D GetAbsoluteTransformation();

		// When we do rendering, we need to convert scene nodes into specific
		// rendering actions which get added to the render list. As such,
		// an important part of a scene node is to specify what rendering actions
		// it will perform.
		// Render is what happens when we go deeper in the scene graph.
		// Post render is when we come back out after processing childen.
		// it is used for re-setting the model matrix or other state changes.
		virtual void Render(RenderList &renderList);
		friend class RenderList;
		friend class BaseRenderer;


		unsigned GetNumChildren()
		{
			return children.size();
		}

		// removes all children, and all children's children
		void Clean()
		{
			children.clear();
			assert( children.size() == 0 );
		}




	protected:
		std::string id;

		std::shared_ptr<SceneNode> parent;
		std::map< std::string, std::shared_ptr<SceneNode> > children;

		transMatrix3D L;

		// state memory
		transMatrix3D prevModelMatrix;
		transMatrix3D prevProjMatrix;
		
		// to be able to use "this" pointer.
		std::weak_ptr< SceneNode > smartThis;

	};

	// A scene node specific for placing a camera/viewpoint
	// in a scene. Has useful tools for setting the projection
	// type and where the camera is looking.
	class CameraNode : public SceneNode
	{
		friend class NodeFactory;
	protected:
		CameraNode(std::string id) : SceneNode(id) {}
	
	public:

		// set what sort of projection the camera will perform.
		// perspective projection is set using a standard computer
		// vision K matrix, compatible with OpenCV.
		void SetOrthoProjection(float left, float right, float top, float bottom, float near, float far);
		void SetPerspectiveProjection( transMatrix2D K, float width, float height, float near, float far );
		void SetFromCalibration( const Calibration &calib, float near, float far );
		transMatrix3D GetProjection()
		{
			return projMatrix;
		}

		// position the camera at a new centre, then have it look towards
		// a target position, with the specified up direction.
		void LookAt( hVec3D centre, hVec3D up, hVec3D target );

		void SetActive()   { isActive = true; }
		void SetInactive() { isActive = false;}
		bool GetActive()   { return isActive; }

		virtual void Render(RenderList &renderList);
		friend class RenderList;
		
		float GetLeft(){ return left ;}
		float GetRight(){ return right ;}
		float GetTop(){ return top ;}
		float GetBottom(){ return bottom ;}
		float GetNear(){ return near ;}
		float GetFar(){ return far ;}
		
	protected:

		// the projection matrix that will be used.
		// can be set for orthographic projection or perspective.
		transMatrix3D projMatrix;

		bool isActive;

		// worth knowing these :)
		float left, right;
		float top,  bottom;
		float near, far;

	};


	// A scene node for rendering a textured, coloured, lit, triangle mesh.
	// Basic shaders that work with this mesh are:
	//  vertex + fragment -> shaderProg
	// 	"basicVertex" + "unlitFrag"        -> "basicShader"
	//	"basicVertex" + "dirlitFrag"       -> "basicLitShader"
	//	"basicVertex" + "colourFrag"       -> "basicColourShader"
	//  "basicVertex" + "dirlitColourFrag" -> "basicLitColourShader"
	class MeshNode : public SceneNode
	{
		friend class NodeFactory;
	protected:
		MeshNode(std::string id) : SceneNode(id)
		{
			mesh = NULL;
			texture = NULL;
			baseColour << 1,1,1,1;
		}
	
	public:


		void SetTexture( std::shared_ptr<Texture> in_texture )  { texture = in_texture;}
		void SetMesh( std::shared_ptr<Mesh> in_mesh )           { mesh = in_mesh;      }
		void SetShader( std::shared_ptr<ShaderProgram> in_prog) { shaderProg = in_prog;}
		void SetBaseColour( Eigen::Vector4f in_baseColour ) {baseColour = in_baseColour;}
		void UnsetTexture()                     { texture = NULL;      }
		void UnsetMesh()                        { mesh = NULL;         }
		void UnsetShader()                      { shaderProg = NULL;   }
		
		std::shared_ptr<Texture> GetTexture()   { return texture; }
		std::shared_ptr<Mesh>    GetMesh()      { return mesh;    }

		virtual void Render(RenderList &renderList);
		friend class RenderList;

	protected:
		std::shared_ptr<Texture>       texture;
		std::shared_ptr<Mesh>          mesh;
		std::shared_ptr<ShaderProgram> shaderProg;
		Eigen::Vector4f baseColour;			       // colour applied to all vertices. Weights texture.
	};
}

#endif
