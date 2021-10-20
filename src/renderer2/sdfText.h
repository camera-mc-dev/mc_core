#ifndef SDF_TEXT_H
#define SDF_TEXT_H

#include "cv.hpp"
#include <vector>
using std::vector;

#include <iostream>
using std::cout;
using std::endl;

#include "renderer2/baseRenderer.h"

#include "ft2build.h"
#include FT_FREETYPE_H

namespace Rendering
{
	class BaseRenderer;

	class SDFText
	{
	public:
		SDFText(std::string in_fontFile, std::weak_ptr<AbstractRenderer> in_renderer);
		~SDFText();

		void RenderString( std::string text, float height, float r, float g, float b, std::shared_ptr<Rendering::SceneNode> sn );
		
		void RenderString( std::string text, float height, hVec3D pos, Eigen::Vector4f colour, std::shared_ptr<Rendering::SceneNode> sn )
		{
			RenderString( text, height, colour(0), colour(1), colour(2), sn );
			transMatrix3D T = transMatrix3D::Identity();
			T.col(3) = pos;
			sn->SetTransformation(T);
		}
		
		void RenderString( std::string text, float height, hVec2D pos, Eigen::Vector4f colour, std::shared_ptr<Rendering::SceneNode> sn )
		{
			RenderString( text, height, colour(0), colour(1), colour(2), sn );
			transMatrix3D T = transMatrix3D::Identity();
			T(0,3) = pos(0);
			T(1,3) = pos(1);
			T(2,3) = 0.0f;
			sn->SetTransformation(T);
		}

	private:

		void CreateSDFImage();
		cv::Mat atlas;
		cv::Mat sdfImage;	// the signed distance field of the font.

		std::shared_ptr<Texture> sdfTex;

		std::string fontFile;

		void GenerateGlyphs(FT_Face &face);
		void PackGlyphs();
		void PlaceGlyphs();
		void GenerateSDF();


		struct Glyph
		{
			cv::Mat img;
			cv::Rect pos;	// position of the glyph in our image.
			hVec2D offset;	// rendering offsets for things like p.
			float advance;
		};
		std::vector< Glyph > glyphs;

		std::weak_ptr<AbstractRenderer> renderer;


	};
}
#endif
