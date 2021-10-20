#ifndef MESH_H
#define MESH_H

#include <vector>


#include "renderer2/renderingTypes.h"
#include "renderer2/abstractRenderer.h"
#include "math/products.h"

#include <iostream>
using std::cout;
using std::endl;

namespace Rendering
	{

	// forward declaration...
	class AbstractRenderer;

	// this stores geometry data (vertices, faces, normals etc),
	// of a single 3D mesh.
	// the class provides methods to upload or remove the
	// geometry data from the GPU.
	// A mesh can be on a single GPU context (right now).
	class Mesh
	{
	public:
		Mesh()
		{
		}

		// initialises the mesh. Note that this assumes this is a triangle mesh
		// with 3-vertices per face.
		Mesh(unsigned numVerts, unsigned numFaces, unsigned vertsPerFace=3 )
		{
			vertices    = vertexMatrix::Zero(4, numVerts);
			normals     = vertexMatrix::Zero(4, numVerts);
			texCoords   = vertexMatrix::Zero(2, numVerts);
			vertColours = vertexMatrix::Zero(4, numVerts);

			faces   = primitiveMatrix::Zero(vertsPerFace, numFaces);
		}

		~Mesh();

		void UploadToRenderer(std::weak_ptr<AbstractRenderer> renderer);
		void UploadToRenderer();


		GLuint GetVArrayObject()
		{
			if( renderer.use_count() == 0 )
				throw std::runtime_error( "mesh is not on GPU, can't return buffer id");
			return vertexArrayObject;
		}

		GLuint GetVBuffer()
		{
			if( renderer.use_count() == 0 )
				throw std::runtime_error( "mesh is not on GPU, can't return buffer id");
			return vertexBuffer;
		}

		GLuint GetTBuffer()
		{
			if( renderer.use_count() == 0 )
				throw std::runtime_error( "mesh is not on GPU, can't return buffer id");
			return texCoordBuffer;
		}

		GLuint GetNBuffer()
		{
			if( renderer.use_count() == 0 )
				throw std::runtime_error( "mesh is not on GPU, can't return buffer id");
			return normalBuffer;
		}

		GLuint GetFBuffer()
		{
			if( renderer.use_count() == 0 )
				throw std::runtime_error( "mesh is not on GPU, can't return buffer id");
			return faceBuffer;
		}

		GLuint GetVCBuffer()
		{
			if( renderer.use_count() == 0 )
				throw std::runtime_error( "mesh is not on GPU, can't return buffer id");
			return vertColourBuffer;
		}
		
		
		unsigned GetNumFaces()
		{
			return faces.cols();
		}
		
		// calculates the vertex normals assuming that each vertex normal is the mean of the face normals
		// for the faces that use that vertex.
		void CalculateMeanNormals()
		{
			if( faces.rows() < 3 )
				throw std::runtime_error( "Can not calculate vertex normals for primitives with fewer than 3 vertices.");
			normals = vertexMatrix::Zero( vertices.rows(), vertices.cols() );
			
			for( unsigned fc = 0; fc < faces.cols(); ++fc )
			{
				hVec3D a,b;
				//cout << faces.col(fc).transpose() << endl;
				a = vertices.col( faces(1,fc) ) - vertices.col( faces(0,fc) );
				b = vertices.col( faces(2,fc) ) - vertices.col( faces(1,fc) );
				a = a / a.norm();
				b = b / b.norm();
				auto c = Cross( a, b );
				
				for( unsigned vc = 0; vc < faces.rows(); ++vc )
				{
					normals.col( faces(vc,fc) ) += c;
					normals(3, faces(vc,fc) ) += 1.0;	// use homogeneous coord to count contributing faces.
				}
			}
			
			for( unsigned nc = 0; nc < normals.cols(); ++nc )
			{
				normals.col(nc) /= normals(3,nc);
				normals(3,nc) = 0.0f;
			}
		}

		vertexMatrix vertices;
		vertexMatrix normals;
		vertexMatrix texCoords;
		vertexMatrix vertColours;

		primitiveMatrix faces;

	protected:

		std::weak_ptr<AbstractRenderer> renderer;

		// there are advantages to keeping vertices and normals and texCoords
		// all in one buffer. But I want to keep my life simple for now!
		GLuint vertexArrayObject;
		GLuint vertexBuffer;
		GLuint normalBuffer;
		GLuint texCoordBuffer;
		GLuint vertColourBuffer;

		GLuint faceBuffer;


	};
} // namespace
#endif
