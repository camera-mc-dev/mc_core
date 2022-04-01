#include "mesh.h"
#include <iostream>
using std::endl;
using std::cout;


// // we can use the Assimp library for loading 3D models.
// #include <assimp/Importer.hpp>
// #include <assimp/scene.h>
// #include <assimp/postprocess.h>

void Rendering::Mesh::UploadToRenderer()
{
	auto ren = renderer.lock();
	UploadToRenderer(ren);
}

void Rendering::Mesh::UploadToRenderer(std::weak_ptr<AbstractRenderer> in_renderer)
{
	GLuint err;
	
	auto ren = renderer.lock();
	auto in_ren = in_renderer.lock();

	assert(in_ren);
	
	if( ren && in_ren && in_ren != ren)
	{
		// this mesh is being uploaded to a different renderer!
		throw std::runtime_error("Warning: uploading mesh to a different renderer. Easy to fix, not done!");
	}
	
	in_ren->SetActive();
	if( !ren ) // initialisations needed the first time we upload to this renderer
	{
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (on entry): " << err << endl;
		}

		glGenVertexArrays(1, &vertexArrayObject);
		glBindVertexArray(vertexArrayObject);

		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (vao): " << err << endl;
		}

		glGenBuffers(1, &vertexBuffer );
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (vertex buffer): " << err << endl;
		}

		glGenBuffers(1, &normalBuffer );
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (normal buffer): " << err << endl;
		}

		glGenBuffers(1, &texCoordBuffer );
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (tex coord buffer): " << err << endl;
		}

		glGenBuffers(1, &faceBuffer );
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (face buffer): " << err << endl;
		}
		
		glGenBuffers(1, &vertColourBuffer );
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "mesh upload error (vertex colour buffer): " << err << endl;
		}
		
		renderer = in_renderer;	// make sure we remember the renderer
	}

	// the rest of this is all that is needed if we were already associated to a renderer.
	glBindVertexArray( vertexArrayObject );

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer );
	glBufferData( GL_ARRAY_BUFFER, vertices.cols() * sizeof(float) * vertices.rows(), vertices.data(), GL_STATIC_DRAW );

	glBindBuffer(GL_ARRAY_BUFFER, normalBuffer );
	glBufferData( GL_ARRAY_BUFFER, normals.cols() * sizeof(float) * normals.rows(), normals.data(), GL_STATIC_DRAW );

	glBindBuffer(GL_ARRAY_BUFFER, texCoordBuffer );
	glBufferData( GL_ARRAY_BUFFER, texCoords.cols() * sizeof(float) * texCoords.rows(), texCoords.data(), GL_STATIC_DRAW );
	
	glBindBuffer(GL_ARRAY_BUFFER, vertColourBuffer );
	glBufferData( GL_ARRAY_BUFFER, vertColours.cols() * sizeof(float) * vertColours.rows(), vertColours.data(), GL_STATIC_DRAW );
	

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceBuffer );
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.cols() * sizeof(unsigned) * faces.rows(), faces.data(), GL_STATIC_DRAW );


	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "mesh upload error: " << err << endl;
	}

}

Rendering::Mesh::~Mesh()
{
	// remove from renderer
	auto ren = renderer.lock();
	if( ren )
	{
		ren->SetActive();
		
		glDeleteBuffers(1, &vertexBuffer );
		glDeleteBuffers(1, &normalBuffer );
		glDeleteBuffers(1, &texCoordBuffer );
		glDeleteBuffers(1, &vertColourBuffer );

		glDeleteBuffers(1, &faceBuffer );

		glDeleteVertexArrays(1, &vertexArrayObject);
	}
}
