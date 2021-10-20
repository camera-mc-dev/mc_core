#include "renderer2/model.h"

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags


void Rendering::Model3D::Load(std::string filename)
{
	// we'll use the Assimp library for loading
	// 3D models.
	// This approach is inferred from tutorials,
	// there is probably much more we could actually do here!
	Assimp::Importer importer;

	// TODO: Find out what the post processing is doing, and what
	// other options may be useful.
	const aiScene* scene = importer.ReadFile( filename,
		 						aiProcess_CalcTangentSpace       |
								aiProcess_Triangulate            |
								aiProcess_JoinIdenticalVertices  |
								aiProcess_SortByPType);

	if( scene == NULL )
	{
		std::string error("Could not open model file: ");
		error = error + filename;
		error = error + "\n";
		error = error + importer.GetErrorString();
		throw std::runtime_error(error);
	}

	// now we can read each of the meshes in the "scene":
	const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);
	for( unsigned mc = 0; mc < scene->mNumMeshes; ++mc )
	{
		const aiMesh* fileMesh = scene->mMeshes[mc];
		meshTextureIndx.push_back( fileMesh->mMaterialIndex );

		// NOTE: We're still assuming, and not checking, that
		// the mesh will be a triangle mesh!
		Mesh mesh( fileMesh->mNumVertices, fileMesh->mNumFaces );

		// populate vertices, normals, texture coords.
		for( unsigned vc = 0; vc < fileMesh->mNumVertices; ++vc )
		{
			const aiVector3D* v = &(fileMesh->mVertices[mc]);
			const aiVector3D* n = &(fileMesh->mNormals[mc]);
			const aiVector3D* t = &Zero3D;

			// NOTE: The mesh might actually have multiple texture coordinates
			//       per vertex. We just take the first one and ignore, for now.
			if( fileMesh->HasTextureCoords(0) )
			{
				t = &(fileMesh->mTextureCoords[0][mc]);
			}

			mesh.vertices(0, mc) = v->x;
			mesh.vertices(1, mc) = v->y;
			mesh.vertices(2, mc) = v->z;
			mesh.vertices(3, mc) = 1.0f;

			mesh.normals(0, mc)  = n->x;
			mesh.normals(1, mc)  = n->y;
			mesh.normals(2, mc)  = n->z;
			mesh.normals(3, mc)  = 0.0f;

			mesh.texCoords(0, mc) = t->x;
			mesh.texCoords(1, mc) = t->y;


		}

		// populate faces.
		for( unsigned fc = 0; fc < fileMesh->mNumFaces; ++fc )
		{
			const aiFace &f = fileMesh->mFaces[fc];
			assert(f.mNumIndices == 3);	// make sure we have triangles.

			mesh.faces(0, fc) = f.mIndices[0];
			mesh.faces(1, fc) = f.mIndices[1];
			mesh.faces(2, fc) = f.mIndices[2];
		}

	}

	// and then the textures
	for( unsigned tc = 0; tc < scene->mNumMaterials; ++tc )
	{
		const aiMaterial* material = scene->mMaterials[tc];

		// the material is far more complex than what I'm even
		// vaguely contemplating dealing with.
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString path;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
			{
				std::string fullPath = path.data;
				textureFiles.push_back( fullPath );
			}
		}
		else
		{
			// no texture we can use (maybe...), so say that by
			// giving an index larger than the available number of textures.
			textureFiles.push_back("");
		}
	}

	// many many many more things can be loaded,
	// including bones and any number of other more complicated things.
	// we can worry about them at a much much later stage. Hopefully.



}
