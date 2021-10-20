#ifndef ME_GEOM_MAKER
#define ME_GEOM_MAKER

#include "renderer2/mesh.h"

#include <memory>

namespace Rendering
{

	// Generate a 2D rectangle of the given width/height.
	std::shared_ptr<Mesh> GenerateCard(float width, float height, bool centreOrigin=true);

	// Generate a 2D filled circle of the given radius and number of sections.
	std::shared_ptr<Mesh> GenerateFilledCircle(float radius, unsigned sections=20);

	// Generate a 2D outline circle of the given radius and thickness and number of sections.
	std::shared_ptr<Mesh> GenerateOutlineCircle(float radius, float thickness, unsigned sections=20);

	// Generate thick line from a set of 2D points.
	// the line is assumed to be in the xy (z=0) plane.
	std::shared_ptr<Mesh> GenerateThickLine2D( std::vector< Eigen::Vector2f > points, float thickness);

	std::shared_ptr<Mesh> GenerateBoundingBox( hVec2D tl, hVec2D br, float thickness );
	
	std::shared_ptr<Mesh> GenerateCube(hVec3D centre, float sideLength);
	std::shared_ptr<Mesh> GenerateWireCube(hVec3D centre, float sideLength);
	
	std::shared_ptr<Mesh> GenerateRect(hVec3D centre, float xlen, float ylen, float zlen);
	
	std::shared_ptr<Rendering::MeshNode> GenerateCubeNode( hVec3D centre, float sideLength, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );
	std::shared_ptr<Rendering::MeshNode> GenerateRectNode( hVec3D centre, float xlen, float ylen, float zlen, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );
	
	// generate image node with the aspect ratio of the original image
	std::shared_ptr<MeshNode> GenerateImageNode(float tlx, float tly, float width, cv::Mat img, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );
	// generate image node with the input aspect ratio.
	std::shared_ptr<MeshNode> GenerateImageNode(float tlx, float tly, float width, float height, cv::Mat img, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );
	std::shared_ptr<MeshNode> GenerateLineNode2D( std::vector< Eigen::Vector2f > points, float thickness, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );
	
	std::shared_ptr<MeshNode> GenerateCircleNode2D( hVec2D centre, float radius, float thickness, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );

	std::shared_ptr<SceneNode> GenerateAxisNode3D( float length, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren );
	
	
	std::shared_ptr<Mesh> GenerateBone( hVec3D start, hVec3D end, hVec3D camCent, float halfWidth );
}

#endif
