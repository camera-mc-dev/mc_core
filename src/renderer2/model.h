#ifndef ME_MODEL_3D_H
#define ME_MODEL_3D_H

#include "renderer2/mesh.h"
#include "renderer2/texture.h"
#include "renderer2/sceneGraph.h"

#include <vector>
#include <string>

// a 3D model is a combination of a set of meshes, their associated
// textures, and a render graph of how they connect to each other.
namespace Rendering
{
	class Model3D
	{
	public:

		Model3D(std::string filename)
		{
			Load(filename);
		}
		Model3D();


		void Load( std::string filename );

	protected:

		std::vector<Mesh>     meshes;
		std::vector<unsigned> meshTextureIndx; // mesh[i] requires textures[ meshTextureIndx[i] ]
		std::vector<std::string>  textureFiles;



		SceneNode *root;


	};
}
#endif
