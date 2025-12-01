#include "renderer2/sdfText.h"
#include "renderer2/texture.h"
#include "renderer2/geomTools.h"
#include <sstream>

Rendering::SDFText::SDFText(std::string in_fontFile, std::weak_ptr<AbstractRenderer> in_renderer) : renderer( in_renderer )
{
	fontFile = in_fontFile;
	CreateSDFImage();

	sdfTex.reset( new Texture(renderer) );
	sdfTex->UploadImage( sdfImage );

	// to actually render text, we also need to load the appropriate shaders.
	// The SDFText class expects these to exist.
	std::stringstream ss;
	auto ren = renderer.lock();
	ss << ren->ccfg.shadersRoot << "/";
	ren->LoadVertexShader(ss.str() + "basicVertex.glsl", "sdfTextVertex"); // geometry is rendered by the basic vertex shader.
	ren->LoadFragmentShader(ss.str() + "sdfTextFrag.glsl", "sdfTextFrag");
	ren->CreateShaderProgram("sdfTextVertex", "sdfTextFrag", "sdfTextProg");
}

Rendering::SDFText::~SDFText()
{
}

void Rendering::SDFText::CreateSDFImage()
{
	FT_Library ftLibrary;
	FT_Face face;


	auto error = FT_Init_FreeType( &ftLibrary );
	if( error )
	{
		throw std::runtime_error("Error initialising freetype 2");
	}

	error = FT_New_Face(ftLibrary, fontFile.c_str(), 0, &face );
	if( error == FT_Err_Unknown_File_Format )
	{
		throw std::runtime_error(fontFile + "  specified font file is of a format freetype 2 can't handle");
	}
	else if(error)
	{
		throw std::runtime_error(fontFile + " Could not open specified font file.");
	}

	FT_Set_Pixel_Sizes(face, 0, 42);	// setting the font size to 48 pixels.

	GenerateGlyphs(face);
	PackGlyphs();
	PlaceGlyphs();
	GenerateSDF();
}

void Rendering::SDFText::GenerateGlyphs(FT_Face &face)
{
	// iterate all of the ascii characters.
	for(unsigned char c = 0; c < 128; ++c )
	{
		// loads and renders a bitmap of the glyph for the character.
		if(FT_Load_Char(face, c, FT_LOAD_RENDER))
		{
			throw std::runtime_error("Character glyph could not be loaded.");
		}

		FT_GlyphSlot gs = face->glyph;

		cv::Mat tmp( gs->bitmap.rows, gs->bitmap.width, CV_8UC1, gs->bitmap.buffer );
		cv::Mat gi = tmp.clone();

		Glyph g;
		g.img = gi;
		g.pos = cv::Rect( 0, 0, gs->bitmap.width, gs->bitmap.rows);
		g.offset << gs->bitmap_left, -gs->bitmap_top, 0.0f;
		g.advance = gs->advance.x;
		// there is an advance.y, but it should always be 0 for our purposes.

		glyphs.push_back(g);
	}
	assert( glyphs.size() == 128 );
}

void Rendering::SDFText::PackGlyphs()
{
	// we could do something much more space-efficient than what I'm currently
	// doing, but to do so is beyond my needs, and thus, I shan't!

	// I'll just give each character a hugely redundant 64x64 pixel square,
	// and have a 12x12 grid of characters, thus a bitmap of 768x768.
	// the bitmap should go in the middle of the 64x64 region.
	unsigned cind = 0;
	for( unsigned rc = 0; rc < 12; ++rc )
	{
		for( unsigned cc = 0; cc < 12; ++cc )
		{
			if( cind < glyphs.size() )
			{
				hVec2D tl;
				tl << cc*64, rc*64, 1.0f;

				hVec2D off;
				off(0) = 64/2 - glyphs[cind].pos.width/2;
				off(1) = 64/2 - glyphs[cind].pos.height/2;
				off(2) = 0.0f;

				tl = tl + off;

				glyphs[cind].pos.x = tl(0);
				glyphs[cind].pos.y = tl(1);

				++cind;
			}
		}
	}
}

void Rendering::SDFText::PlaceGlyphs()
{
	atlas = cv::Mat(768,768,CV_8UC1, cv::Scalar(0) );
	for( unsigned gc = 0; gc < glyphs.size(); ++gc )
	{
		if( glyphs[gc].pos.width > 0 && glyphs[gc].pos.height > 0 )
		{
			Glyph &g = glyphs[gc];
			float x1,y1,x2,y2;
			x1 = g.pos.x;
			y1 = g.pos.y;
			x2 = x1 + g.pos.width;
			y2 = y1 + g.pos.height;
			g.img.copyTo(atlas.rowRange(y1,y2).colRange(x1,x2) );
		}

	}

	cv::imwrite( "atlas1.png", atlas);
}

void Rendering::SDFText::GenerateSDF()
{
	// the signed distance field is very simple, in theory!
	// pixels inside each glyph should have negative value,
	// and pixels outside each glyph should have positive.
	// except we scale that -ve/+ve so everything is +ve.
	// it should be pretty trivial to compute using the
	// OpenCV distance transforms....

	// we need two versions.. one white on black,
	// the other black on white.
	cv::Mat wob, bow;
	wob = atlas.clone();
	bow = cv::Scalar(255) - wob;

	cv::Mat dwob, dbow;
	cv::distanceTransform(wob, dwob, cv::DIST_L2, cv::DIST_MASK_PRECISE );
	cv::distanceTransform(bow, dbow, cv::DIST_L2, cv::DIST_MASK_PRECISE );

	// threshold at a distance of 10
	cv::threshold(dwob, dwob, 10.0f, 10.0f, cv::THRESH_TRUNC );
	cv::threshold(dbow, dbow, 10.0f, 10.0f, cv::THRESH_TRUNC );

	// cv::Mat tmp;
	// tmp = (dwob/10.0f) * 255.0f;
	// cv::imwrite("dwob.png", tmp);
	// tmp = (dbow/10.0f) * 255.0f;
	// cv::imwrite("dbow.png", tmp);

	sdfImage = cv::Mat( atlas.rows, atlas.cols, CV_8UC1, cv::Scalar(0) );
	for( unsigned rc = 0; rc < atlas.rows; ++rc )
	{
		#pragma omp parallel for
		for( unsigned cc = 0; cc < atlas.cols; ++cc )
		{
			float a,b;
			unsigned char c;
			a = dwob.at<float>(rc,cc);	// inside value
			b = dbow.at<float>(rc,cc);  // outside value
			if( a == b )
			{
				c = 128;
			}
			if( a > b )
			{
				// pixel is inside, so value is -ve
				c = 128 - a;
			}
			else
			{
				// pixel is outside, so value is +ve
				c = 128 + b;
			}
			sdfImage.at<unsigned char>(rc,cc) = c;
		}
	}
	// cv::flip( sdfImage, sdfImage, 0);
	cv::imwrite("sdfImage1.png", sdfImage);
}





void Rendering::SDFText::RenderString(std::string text, float height, float cr, float cg, float cb, std::shared_ptr<Rendering::SceneNode> sn)
{
	auto ren = renderer.lock();
	ren->SetActive();
	
	// scale...
	float scale = (1.0 / 48.0) * height;

	// the scene node should be empty, and there should be no geometry.
	assert( sn->GetNumChildren() == 0 );

	// now all we need to do (ha ha ha) is assemble some glyphs into
	// the correct locations as simple textured card meshes with the
	// appropriate shader program. Easy...
	hVec2D tl;
	tl << 0,0,1.0;
	for( unsigned i = 0; i < text.size(); ++i )
	{
		// cout << "tl: " << tl.transpose() << endl;
		unsigned char c = text[i];

		if( c == '\n' )
		{
			tl(0) = 0.0f;
			tl(1) += 1.01 * height;
			continue;
		}

		Glyph &g = glyphs[c];

		// so we know which character we're drawing. The next question is,
		// how big is the geometry?
		// We know the full glyph size was 48 pixels.
		// we also know the actual glyph size...
		float w = scale * g.pos.width;
		float h = scale * g.pos.height;

		auto geo = GenerateCard(w,h,false);

		// we need to adjust the texture coordinates however....
		float colsf, rowsf;
		colsf = sdfImage.cols; rowsf = sdfImage.rows;
		geo->texCoords(0,0) = g.pos.x / colsf;
		geo->texCoords(1,0) = g.pos.y / rowsf;

		geo->texCoords(0,1) = (g.pos.x + g.pos.width) / colsf;
		geo->texCoords(1,1) = g.pos.y / rowsf;

		geo->texCoords(0,2) = (g.pos.x + g.pos.width) / colsf;
		geo->texCoords(1,2) = (g.pos.y + g.pos.height) / rowsf;

		geo->texCoords(0,3) = g.pos.x / colsf;
		geo->texCoords(1,3) = (g.pos.y + g.pos.height) / rowsf;

		// cout << "texCoords: " << endl << geom[i]->texCoords * 768 << endl;

		geo->UploadToRenderer(renderer);

		// the basic geometry was thus easy. Now we need to create the
		// appropriate scene node.
		std::stringstream ss;
		ss << "text_" << i << "_" << c;
		std::shared_ptr<Rendering::MeshNode> charNode;
		Rendering::NodeFactory::Create(charNode, ss.str() );
		hVec2D offset = scale * g.offset;
		hVec2D p = tl + offset;
		transMatrix3D T = transMatrix3D::Identity();
		T(0,3) = p(0);
		T(1,3) = p(1);
		T(2,3) = 0.0;

		// cout << "p : " << p.transpose() << endl;
		// cout << "ga: " << g.advance << endl;


		Eigen::Vector4f baseCol; baseCol << cr,cg,cb,1.0;
		charNode->SetMesh(geo);
		charNode->SetTexture(sdfTex);
		charNode->SetShader( ren->GetShaderProg("sdfTextProg") );
		charNode->SetTransformation(T);
		charNode->SetBaseColour(baseCol);

		tl(0) += scale * (g.advance / 64.0);

		sn->AddChild(charNode);
	}
}
