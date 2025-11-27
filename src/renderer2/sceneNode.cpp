#include "renderer2/sceneGraph.h"
#include "renderer2/renderList.h"
#include "renderer2/renderActions.h"
#include "math/matrixGenerators.h"

// ----------------------------------------------------------------------------------------------------- //
//
//      Basic scene node: performs transformations and little else.
//
// ----------------------------------------------------------------------------------------------------- //


Rendering::SceneNode::SceneNode(std::string in_id)
{
	id = in_id;

	L = transMatrix3D::Identity();

	parent = NULL;
	
	// cout << "cre: " << id << endl;
}

Rendering::SceneNode::~SceneNode()
{
	// cout << "del: " << id << endl;
	
	// although we might initially think that this is corrent to do, it is not now that we're in smart pointer land.
	// what happens is the parent cleans up, which calls this destructor, which tries to remove from parent, which calls this destructor... mess ensues!
	// RemoveFromParent();
	Clean();
}


void Rendering::SceneNode::AddChild( std::shared_ptr<SceneNode> newChild )
{
	if( newChild->parent )
	{
        // if the newChild already has a parent node, we want newChild to
        // be removed from that parent.
        // n should, in this case, be the same as newChild...
		std::shared_ptr<SceneNode> n = newChild->parent->RemoveChild( newChild->id );
        assert(n == newChild);
	}
	newChild->parent = smartThis.lock();

	auto c = children.find( newChild->id );
	if( c == children.end() )
	{
		children[ newChild->id ] = newChild;
	}
	else
	{
		throw std::runtime_error("Node already has a child with id " + newChild->id );
	}
}

std::shared_ptr<Rendering::SceneNode> Rendering::SceneNode::FindChild( std::string id )
{
	auto c = children.find(id);
	if( c == children.end() )
	{
		return NULL;
	}
	else
	{
		return c->second;
	}
}

std::shared_ptr<Rendering::SceneNode> Rendering::SceneNode::RemoveChild( std::string id )
{
	std::shared_ptr<SceneNode> n = FindChild( id );
	if( n != NULL )
	{
		children.erase(id);
	}
	return n;
}


void Rendering::SceneNode::SetParent(std::shared_ptr<SceneNode> in_parent)
{
	in_parent->AddChild( smartThis.lock() );
}




void Rendering::SceneNode::SetTransformation( transMatrix3D in_L )
{
	this->L = in_L;
}

void Rendering::SceneNode::SetTransformation( hVec3D pos, hVec3D rvec )
{
	throw std::runtime_error("scene node set transformation from pos, rvec needs implementing");
	// TODO!
	// use eigen angle axis to make rotation matrix and build L.
}

transMatrix3D &Rendering::SceneNode::GetTransformation()
{
	return L;
}

transMatrix3D Rendering::SceneNode::GetAbsoluteTransformation()
{
	if( parent == NULL )
	{
		return L;
	}
	return parent->GetAbsoluteTransformation() * L;
}


void Rendering::SceneNode::Render( RenderList &renderList )
{
	for( auto i = children.begin(); i != children.end(); ++i )
	{
		i->second->Render( renderList );
	}
}



// ----------------------------------------------------------------------------------------------------- //
//
//      Camera scene node: positions a camera in the scene, and specifies the projection matrix if the camera is active.
//
// ----------------------------------------------------------------------------------------------------- //

void Rendering::CameraNode::Render( RenderList &renderList )
{
	if( isActive )
	{
		// if this is the active camera, then nothing gets drawn, but it is up to
		// the renderer to ensure this camera is correctly used.
	}
	else
	{
		// if this is not the active camera, then we may choose to render some
		// simple mesh to indicate that this camera is present in the scene.
		// TODO: Camera mesh render.
	}
}


void Rendering::CameraNode::SetOrthoProjection(float left, float right, float top, float bottom, float nearClip, float farClip)
{
	transMatrix3D M;
	M(0,0) = 2.0 / (right-left);
	M(0,1) = 0.0;
	M(0,2) = 0.0;
	M(0,3) = -(right+left)/(right-left);

	M(1,0) = 0.0f;
	M(1,1) = 2 / (top-bottom);
	M(1,2) = 0.0f;
	M(1,3) = -(top+bottom)/(top-bottom);

	M(2,0) = 0.0;
	M(2,1) = 0.0;
	M(2,2) = -2 / (farClip-nearClip);
	M(2,3) = -(farClip+nearClip)/(farClip-nearClip);

	M(3,0) = 0.0;
	M(3,1) = 0.0;
	M(3,2) = 0.0;
	M(3,3) = 1.0;
	
	this->left = left;
	this->right = right;
	this->top = top;
	this->bottom = bottom;
	this->nearClip = nearClip;
	this->farClip = farClip;

	projMatrix = M;

}

void Rendering::CameraNode::SetPerspectiveProjection( transMatrix2D K, float w, float h, float nearClip, float farClip )
{
	// This should map our desired right-handed looking along +ve z camera coordinate system
	// to the appropriate OpenGL normalised device coordinates. Hopefully...
	projMatrix = ProjMatGLFromK( K, w, h, nearClip, farClip );

}

void Rendering::CameraNode::SetFromCalibration( const Calibration &calib, float nearClip, float farClip )
{
	// This sets the camera projection and camera position to match the input camera
	// calibration.
	SetPerspectiveProjection( calib.K, calib.width, calib.height, nearClip, farClip );
	L = calib.L;
}

void Rendering::CameraNode::LookAt( hVec3D centre, hVec3D up, hVec3D target )
{
	// NOTE: This assumes that we are looking at something in the same
	//       coordinate space as the camera (i.e. something with the same parent node).
	L = ::LookAt(centre, up, target);
}



// ----------------------------------------------------------------------------------------------------- //
//
//      Mesh scene node: positions a mesh instance in the scene, and specifies how to render it.
//
// ----------------------------------------------------------------------------------------------------- //

void Rendering::MeshNode::Render(RenderList &renderList)
{
	if( shaderProg == NULL || texture == NULL || mesh == NULL )
	{
		std::stringstream ss;
		ss << this->id << ": mesh node has no associated shader/texture/mesh  " << shaderProg << " " << texture << " " << mesh ;
		throw std::runtime_error(ss.str());
	}

	std::shared_ptr<RenderAction>   drawAction(  new DrawMeshAction(  renderList.GetRenderer(), this->id,
	                                                  shaderProg->GetID(),
	                                                  mesh,
	                                                  texture->GetID(),
	                                                  baseColour ) 
	                                          );

	drawAction->L = this->GetAbsoluteTransformation();

	// cout << drawAction->L << endl << texture->GetID() << endl;

	renderList.renderActions.push_back( drawAction );

	for( auto i = children.begin(); i != children.end(); ++i )
	{
		i->second->Render( renderList );
	}

}
