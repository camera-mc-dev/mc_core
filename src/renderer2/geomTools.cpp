#include "renderer2/geomTools.h"
#include "renderer2/sceneGraph.h"
#include "math/products.h"

using Rendering::Mesh;

std::shared_ptr<Mesh> Rendering::GenerateCard(float width, float height, bool centreOrigin)
{
	std::shared_ptr<Mesh> card( new Mesh(4,2) );	// 4 verts, two faces (triangles).

	// if centreOrigin == true, the model coordinate space will
	// have origin in the middle of the generated geometry.
	// otherwise, we have a top-left origin.

	if( centreOrigin )
	{
		float wo2 = width/2;
		float ho2 = height/2;

		card->vertices << -wo2,  wo2,  wo2, -wo2,
		                  -ho2, -ho2,  ho2,  ho2,
		                     0,    0,    0,    0,
		                     1,    1,    1,    1;
	}
	else
	{
		card->vertices <<    0,  width,  width,      0,
		                     0,      0, height, height,
		                     0,      0,      0,      0,
		                     1,      1,      1,      1;
	}

	// normal of the card if -ve z so as to face the camera
	// by default.
	card->normals   << 0 ,  0,  0,  0,
					   0 ,  0,  0,  0,
					   -1, -1, -1, -1,
					   0 ,  0,  0,  0;

	card->texCoords << 0, 1, 1, 0,
					   0, 0, 1, 1;

	card->faces     << 0, 0,
					   1, 2,
					   2, 3;

	return card;
}


std::shared_ptr<Rendering::MeshNode> Rendering::GenerateImageNode(float tlx, float tly, float width, cv::Mat img, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	float ar = img.rows / (float)img.cols;
	auto card = Rendering::GenerateCard(width, width*ar, false);
	std::shared_ptr<Texture> tex = std::shared_ptr<Texture>( new Texture(ren) );
	tex->UploadImage(img);
	card->UploadToRenderer(ren);
	
	std::shared_ptr<Rendering::MeshNode> mn;
	Rendering::NodeFactory::Create(mn, id);
	
	mn->SetTexture(tex);
	mn->SetMesh(card);
	
	auto r = ren.lock();
	if( r )
	{
		mn->SetShader( r->GetShaderProg("basicShader") );
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating image node");
	}
	
	transMatrix3D T = transMatrix3D::Identity();
	T(0,3) = tlx;
	T(1,3) = tly;
	mn->SetTransformation(T);
	
	return mn;
}

std::shared_ptr<Rendering::MeshNode> Rendering::GenerateImageNode(float tlx, float tly, float width, float height, cv::Mat img, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	auto card = Rendering::GenerateCard(width, height, false);
	std::shared_ptr<Texture> tex = std::shared_ptr<Texture>( new Texture(ren) );
	tex->UploadImage(img);
	card->UploadToRenderer(ren);
	
	std::shared_ptr<Rendering::MeshNode> mn;
	Rendering::NodeFactory::Create(mn, id);
	
	mn->SetTexture(tex);
	mn->SetMesh(card);
	
	auto r = ren.lock();
	if( r )
	{
		mn->SetShader( r->GetShaderProg("basicShader") );
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating image node");
	}
	
	transMatrix3D T = transMatrix3D::Identity();
	T(0,3) = tlx;
	T(1,3) = tly;
	mn->SetTransformation(T);
	
	return mn;
}

std::shared_ptr<Mesh> Rendering::GenerateFilledCircle(float radius, unsigned sections)
{
	std::shared_ptr<Mesh> circle( new Mesh(sections+1, sections) );

	// centre of the circle.
	circle->vertices(0,0) = 0.0f;
	circle->vertices(1,0) = 0.0f;
	circle->vertices(2,0) = 0.0f;
	circle->vertices(3,0) = 1.0f;

	circle->texCoords(0,0) = 0.5f;
	circle->texCoords(1,0) = 0.5f;

	circle->normals(0,0) =  0.0f;
	circle->normals(1,0) =  0.0f;
	circle->normals(2,0) = -1.0f;
	circle->normals(3,0) =  0.0f;

	for (unsigned  sc = 0; sc < sections; ++sc )
	{
		float theta = 2.0f * 3.1415926f * (float(sc) / (float)sections);
		float x = cosf(theta);
		float y = sinf(theta);

		circle->vertices(0,sc+1) = x*radius;
		circle->vertices(1,sc+1) = y*radius;
		circle->vertices(2,sc+1) = 0;
		circle->vertices(3,sc+1) = 1;

		circle->texCoords(0, sc+1) = x + 0.5f;
		circle->texCoords(1, sc+1) = y + 0.5f;

		circle->normals(0,sc+1) =  0.0f;
		circle->normals(1,sc+1) =  0.0f;
		circle->normals(2,sc+1) = -1.0f;
		circle->normals(3,sc+1) =  0.0f;
	}

	for( unsigned sc = 0; sc < sections; ++sc )
	{
		circle->faces(0, sc) = 0;
		circle->faces(1, sc) = sc;
		circle->faces(2, sc) = sc+1;

		if( sc == sections-1 )
			circle->faces(2,sc) = 1;
	}

	return circle;
}


std::shared_ptr<Mesh> Rendering::GenerateOutlineCircle(float radius, float thickness, unsigned sections)
{
	std::shared_ptr<Mesh> circle( new Mesh(sections*2, sections*2) );

	for (unsigned  sc = 0; sc < sections; ++sc )
	{
		float theta = 2.0f * 3.1415926f * (float(sc) / (float)sections);
		float x = cosf(theta);
		float y = sinf(theta);

		// outer..
		circle->vertices(0,sc*2) = x*radius;
		circle->vertices(1,sc*2) = y*radius;
		circle->vertices(2,sc*2) = 0;
		circle->vertices(3,sc*2) = 1;

		// inner
		circle->vertices(0,sc*2+1) = x*(radius-thickness);
		circle->vertices(1,sc*2+1) = y*(radius-thickness);
		circle->vertices(2,sc*2+1) = 0;
		circle->vertices(3,sc*2+1) = 1;

		// outer
		circle->texCoords(0, sc*2) = x + 0.5f;
		circle->texCoords(1, sc*2) = y + 0.5f;

		// inner
		circle->texCoords(0, sc*2+1) = x - 1.0/thickness*x + 0.5f;
		circle->texCoords(1, sc*2+1) = y - 1.0/thickness*y + 0.5f;

		// outer
		circle->normals(0,sc*2) =  0.0f;
		circle->normals(1,sc*2) =  0.0f;
		circle->normals(2,sc*2) = -1.0f;
		circle->normals(3,sc*2) =  0.0f;

		// inner
		circle->normals(0,sc*2+1) =  0.0f;
		circle->normals(1,sc*2+1) =  0.0f;
		circle->normals(2,sc*2+1) = -1.0f;
		circle->normals(3,sc*2+1) =  0.0f;
	}

	for (unsigned  sc = 0; sc < sections; ++sc )
	{
		// two faces (triangles) for each section
		circle->faces(0,sc*2)   = sc*2;
		circle->faces(1,sc*2)   = (sc+1)*2;
		circle->faces(2,sc*2)   = sc*2+1;

		circle->faces(0,sc*2+1) = (sc+1)*2;
		circle->faces(1,sc*2+1) = (sc+1)*2+1;
		circle->faces(2,sc*2+1) = (sc*2)+1;

		if( sc == sections-1 )
		{
			circle->faces(0,sc*2)   = sc*2;
			circle->faces(1,sc*2)   = 0;
			circle->faces(2,sc*2)   = sc*2+1;

			circle->faces(0,sc*2+1) = 0;
			circle->faces(1,sc*2+1) = 1;
			circle->faces(2,sc*2+1) = (sc*2)+1;
		}
	}

	return circle;
}


std::shared_ptr<Mesh> Rendering::GenerateThickLine2D( std::vector< Eigen::Vector2f > points, float thickness)
{
	std::shared_ptr<Mesh> mesh( new Mesh( points.size()*2, (points.size()-1)*2, 3 ) );

	// create the vertices.
	// make sure the mesh is correct.
	for( unsigned pc = 0; pc < points.size(); ++pc )
	{
		// we need to put a vertex just off the line centre
		// to be able to give us width. However, it is not
		// enough to just put it directly above, we must get
		// the width with respect to the direction of the line.
		Eigen::Vector2f d0, d1;
		bool gd0, gd1;
		gd0 = gd1 = false;
		if( pc < points.size() -1 )
		{
			d1 = points[pc+1] - points[pc];
			gd1 = true;
		}
		if( pc > 0 )
		{
			d0 = points[pc] - points[pc-1];
			gd0 = true;
		}

		if( gd0 && !gd1 )
			d1 = d0;
		else if( !gd0 && gd1 )
			d0 = d1;
		if( !gd0 && !gd1)
		{
			// only one point, so, can't really do a line of it anyway.
			throw std::runtime_error("only one point in line plot?");
		}

		// from a discussion here:
		// https://forum.libcinder.org/topic/smooth-thick-lines-using-geometry-shader
		// Either I've missed something, or even this does not make my lines a
		// constant width. Grrrr.
		Eigen::Vector2f d = (d0.normalized() + d1.normalized()).normalized();
		Eigen::Vector2f n; n << d1(1), -d1(0); n /= n.norm();
		Eigen::Vector2f mitre; mitre << d(1), -d(0);
		float l =  thickness * mitre.dot(n);


		// and that gives us two vertices...
		Eigen::Vector2f p = points[pc] + l*mitre;
		mesh->vertices(0, pc*2  ) = p(0);
		mesh->vertices(1, pc*2  ) = p(1);
		mesh->vertices(2, pc*2  ) = 0;
		mesh->vertices(3, pc*2  ) = 1;

		p = points[pc] - l*mitre;
		mesh->vertices(0, pc*2+1 ) = p(0);
		mesh->vertices(1, pc*2+1 ) = p(1);
		mesh->vertices(2, pc*2+1 ) = 0;
		mesh->vertices(3, pc*2+1 ) = 1;

		mesh->texCoords(0, pc*2  ) =  0;
		mesh->texCoords(1, pc*2  ) =  1;

		mesh->texCoords(0, pc*2+1) =  0;
		mesh->texCoords(1, pc*2+1) = -1;
	}

	for( unsigned pc = 0; pc < points.size()-1; ++pc )
	{
		// faces are triangles, so two triangles per section.
		unsigned vi = pc*2;
		mesh->faces(0, pc*2) = vi;
		mesh->faces(1, pc*2) = vi+2;
		mesh->faces(2, pc*2) = vi+1;

		mesh->faces(0, pc*2+1) = vi+2;
		mesh->faces(1, pc*2+1) = vi+3;
		mesh->faces(2, pc*2+1) = vi+1;
	}

	return mesh;
}


std::shared_ptr<Mesh> Rendering::GenerateCube(hVec3D centre, float sideLength)
{
	std::shared_ptr<Mesh> m( new Rendering::Mesh(8,12,3) );
	
	float slo2 = sideLength / 2.0f;
	m->vertices << -slo2, -slo2,  slo2,  slo2,   -slo2, -slo2,  slo2,  slo2,
	               -slo2,  slo2,  slo2, -slo2,   -slo2,  slo2,  slo2, -slo2,
	               -slo2, -slo2, -slo2, -slo2,    slo2,  slo2,  slo2,  slo2,
	                   0,     0,     0,     0,       0,     0,     0,     0;
	for( unsigned cc = 0; cc < 8; ++cc )
		m->vertices.col(cc) += centre;
	
	m->faces    << 0, 0,   1, 1,   2, 2,   3, 3,   4, 4,   1, 1,   
	               1, 5,   2, 6,   3, 7,   0, 4,   5, 6,   2, 3,
	               5, 4,   6, 5,   7, 6,   4, 7,   6, 7,   3, 0;
	
	// hmmm... what should the normals be? One normal per vertex, so a cube mesh's
	// hard right-angles will be.... stupid....
	
	return m;
}

std::shared_ptr<Mesh> Rendering::GenerateRect(hVec3D centre, float xlen, float ylen, float zlen)
{
	std::shared_ptr<Mesh> m( new Rendering::Mesh(8,12,3) );
	
	float xlo2 = xlen / 2.0f;
	float ylo2 = ylen / 2.0f;
	float zlo2 = zlen / 2.0f;
	m->vertices << -xlo2, -xlo2,  xlo2,  xlo2,   -xlo2, -xlo2,  xlo2,  xlo2,
	               -ylo2,  ylo2,  ylo2, -ylo2,   -ylo2,  ylo2,  ylo2, -ylo2,
	               -zlo2, -zlo2, -zlo2, -zlo2,    zlo2,  zlo2,  zlo2,  zlo2,
	                   0,     0,     0,     0,       0,     0,     0,     0;
	for( unsigned cc = 0; cc < 8; ++cc )
		m->vertices.col(cc) += centre;
	
	m->faces    << 0, 0,   1, 1,   2, 2,   3, 3,   4, 4,   1, 1,   
	               1, 5,   2, 6,   3, 7,   0, 4,   5, 6,   2, 3,
	               5, 4,   6, 5,   7, 6,   4, 7,   6, 7,   3, 0;
	
	// hmmm... what should the normals be? One normal per vertex, so a cube mesh's
	// hard right-angles will be.... stupid....
	
	return m;
}



std::shared_ptr<Mesh> Rendering::GenerateWireCube(hVec3D centre, float sideLength)
{
	std::shared_ptr<Mesh> m( new Rendering::Mesh(8,12,2) );
	
	float slo2 = sideLength / 2.0f;
	m->vertices << -slo2, -slo2,  slo2,  slo2,   -slo2, -slo2,  slo2,  slo2,
	               -slo2,  slo2,  slo2, -slo2,   -slo2,  slo2,  slo2, -slo2,
	               -slo2, -slo2, -slo2, -slo2,    slo2,  slo2,  slo2,  slo2,
	                   0,     0,     0,     0,       0,     0,     0,     0;
	for( unsigned cc = 0; cc < 8; ++cc )
		m->vertices.col(cc) += centre;

//  wtf is wrong with this?
//	m->faces    << 0, 1, 2, 3,   4, 5, 6, 7,   0, 1, 2, 3,   
//	               1, 2, 3, 0,   5, 6, 7, 4,   4, 5, 6, 7;
	m->faces(0,0)  = 0; m->faces(1,0)  = 1;
	m->faces(0,1)  = 1; m->faces(1,1)  = 2;
	m->faces(0,2)  = 2; m->faces(1,2)  = 3;
	m->faces(0,3)  = 3; m->faces(1,3)  = 0;
	
	m->faces(0,4)  = 4; m->faces(1,4)  = 5;
	m->faces(0,5)  = 5; m->faces(1,5)  = 6;
	m->faces(0,6)  = 6; m->faces(1,6)  = 7;
	m->faces(0,7)  = 7; m->faces(1,7)  = 4;
	
	m->faces(0,8)  = 0; m->faces(1,8)  = 4;
	m->faces(0,9)  = 1; m->faces(1,9)  = 5;
	m->faces(0,10) = 2; m->faces(1,10) = 6;
	m->faces(0,11) = 3; m->faces(1,11) = 7;
	
	// normals irrelevant for wireframe... so ignore :)
	
	return m;
		
}


std::shared_ptr<Rendering::MeshNode> Rendering::GenerateCubeNode( hVec3D centre, float sideLength, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	// create the node.
	std::shared_ptr< MeshNode > mn;
	Rendering::NodeFactory::Create( mn, id );
	
	// create the mesh....
	auto m = GenerateCube( centre, sideLength );
	
	// set properties
	auto r = ren.lock();
	if( r )
	{
		mn->SetShader( r->GetShaderProg("basicShader") );
		mn->SetTexture( r->GetBlankTexture() );
		
		m->UploadToRenderer(r);
		mn->SetMesh(m);
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating cube node");
	}
	
	return mn;
}

std::shared_ptr<Rendering::MeshNode> Rendering::GenerateRectNode( hVec3D centre, float xlen, float ylen, float zlen, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	// create the node.
	std::shared_ptr< MeshNode > mn;
	Rendering::NodeFactory::Create( mn, id );
	
	// create the mesh....
	auto m = GenerateRect( centre, xlen, ylen, zlen );
	
	// set properties
	auto r = ren.lock();
	if( r )
	{
		mn->SetShader( r->GetShaderProg("basicShader") );
		mn->SetTexture( r->GetBlankTexture() );
		
		m->UploadToRenderer(r);
		mn->SetMesh(m);
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating cube node");
	}
	
	return mn;
}

std::shared_ptr<Rendering::MeshNode> Rendering::GenerateLineNode2D( std::vector< Eigen::Vector2f > points, float thickness, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	// create the node.
	std::shared_ptr< MeshNode > mn;
	Rendering::NodeFactory::Create( mn, id );
	
	// create the mesh....
	auto m = GenerateThickLine2D( points, thickness );
	
	// set properties
	auto r = ren.lock();
	if( r )
	{
		mn->SetShader( r->GetShaderProg("basicShader") );
		mn->SetTexture( r->GetBlankTexture() );
		
		m->UploadToRenderer(r);
		mn->SetMesh(m);
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating line 2d node");
	}
		
	return mn;
}


std::shared_ptr<Rendering::MeshNode> Rendering::GenerateCircleNode2D( hVec2D centre, float radius, float thickness, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	// create the node.
	std::shared_ptr< MeshNode > mn;
	Rendering::NodeFactory::Create( mn, id );
	
	// create the mesh....
	auto m = GenerateOutlineCircle(radius, thickness);
	
	// set properties
	auto r = ren.lock();
	if( r )
	{
		mn->SetShader( r->GetShaderProg("basicShader") );
		mn->SetTexture( r->GetBlankTexture() );
		
		m->UploadToRenderer(r);
		mn->SetMesh(m);
		
		transMatrix3D T = transMatrix3D::Identity();
		T.block(0,3, 3, 1) = centre;
		mn->SetTransformation(T);
		
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating image node");
	}
		
	return mn;
}

std::shared_ptr<Rendering::SceneNode> Rendering::GenerateAxisNode3D( float length, std::string id, std::weak_ptr<Rendering::AbstractRenderer> ren )
{
	
	// becase I haven't put in the ability for meshes to have more than one 
	// colour, I'll make my axes as three mesh nodes.
	
	// create the root node.
	std::shared_ptr< Rendering::SceneNode > sn;
	Rendering::NodeFactory::Create( sn, id );
	
	std::stringstream ss;
	
	float shortLen = length/20.0f;

	ss << id << "-x";
	std::shared_ptr< Rendering::MeshNode > mnx;
	//Rendering::NodeFactory::Create( mnx, ss.str() );
	hVec3D xcent; xcent << length/2, 0.0, 0.0, 1.0;
	mnx = GenerateRectNode( xcent, length, shortLen, shortLen, ss.str(), ren );
	
	ss.str("");
	ss << id << "-y";
	std::shared_ptr< Rendering::MeshNode > mny;
	//Rendering::NodeFactory::Create( mny, ss.str() );
	hVec3D ycent; ycent << 0.0, length/2, 0.0, 1.0;
	mny = GenerateRectNode( ycent, shortLen, length, shortLen, ss.str(), ren );

	ss.str("");
	ss << id << "-z";
	std::shared_ptr< Rendering::MeshNode > mnz;
	//Rendering::NodeFactory::Create( mnz, ss.str() );
	hVec3D zcent; zcent << 0.0, 0.0, length/2,  1.0;
	mnz = GenerateRectNode( zcent, shortLen, shortLen, length, ss.str(), ren );
	
	
	/*
	
	// I obviously wrote a version of this that was intended to use an geometry shader to get a thick line,
	// but I don't remember ever having that actually work, so I clearly got careless in doing some merging 
	// for this to have actually made it into code.
	
	
	// so that's the nodes, now for the meshes themselves.
	std::shared_ptr<Mesh> mx( new Rendering::Mesh(2,1,2) );
	std::shared_ptr<Mesh> my( new Rendering::Mesh(2,1,2) );
	std::shared_ptr<Mesh> mz( new Rendering::Mesh(2,1,2) );
	
	mx->faces << 0,1;
	mx->vertices << 0, length,
	                0,      0,
	                0,      0,
	                1,      1;
	
	my->faces << 0,1;
	my->vertices << 0,      0,
	                0, length,
	                0,      0,
	                1,      1;
	
	mz->faces << 0,1;
	mz->vertices << 0,      0,
	                0,      0,
	                0, length,
	                1,      1;
	
	*/
	
	auto sren = ren.lock();
	if( sren )
	{
		Eigen::Vector4f r,g,b;
		r << 1,0,0,1;
		g << 0,1,0,1;
		b << 0,0,1,1;
		
		//mnx->SetShader( sren->GetShaderProg("thickLine") );
		mnx->SetShader( sren->GetShaderProg("basicColourShader") );
		mnx->SetTexture( sren->GetBlankTexture() );
		mnx->SetBaseColour(r);
		//mx->UploadToRenderer(sren);
		//mnx->SetMesh(mx);
		
		//mny->SetShader( sren->GetShaderProg("thickLine") );
		mny->SetShader( sren->GetShaderProg("basicColourShader") );
		mny->SetTexture( sren->GetBlankTexture() );
		mny->SetBaseColour(g);
		//my->UploadToRenderer(sren);
		//mny->SetMesh(my);
		
		//mnz->SetShader( sren->GetShaderProg("thickLine") );
		mnz->SetShader( sren->GetShaderProg("basicColourShader") );
		mnz->SetTexture( sren->GetBlankTexture() );
		mnz->SetBaseColour(b);
		//mz->UploadToRenderer(sren);
		//mnz->SetMesh(mz);
		
		sn->AddChild( mnx );
		sn->AddChild( mny );
		sn->AddChild( mnz );
		
		return sn;
	}
	else
	{
		throw std::runtime_error("invalid or expired renderer when generating cube node");
	}
	
	
	
}


// a quad such that the face is facing the camera.
std::shared_ptr<Mesh>  Rendering::GenerateBone( hVec3D start, hVec3D end, hVec3D camCent, float halfWidth )
{
	hVec3D camToStart = start - camCent;
	hVec3D boneDir    = end - start;
	hVec3D perp       = Cross( camToStart, boneDir );
	
	perp = perp / perp.norm();
	
	std::shared_ptr<Mesh> card( new Mesh(4,2,3) );
	
	card->vertices.col(0) = start - halfWidth * perp;
	card->vertices.col(1) = start + halfWidth * perp;
	card->vertices.col(2) = end   + halfWidth * perp;
	card->vertices.col(3) = end   - halfWidth * perp;
	
	card->faces << 0, 0,
	               1, 2,
	               2, 3;
	
	hVec3D n = camToStart / camToStart.norm();
	card->normals.col(0) = n;
	card->normals.col(1) = n;
	card->normals.col(2) = n;
	card->normals.col(3) = n;
	
	return card;
}                 
