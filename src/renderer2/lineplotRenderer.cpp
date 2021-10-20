#include "renderer2/lineplotRenderer.h"

#include <iostream>
using std::cout;
using std::endl;

Rendering::LineplotRenderer::LineplotRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title)
{
	minX = 0;
	maxX = 1;
	minY = 0;
	maxY = 1;

	fixXAxis = false;
	fixYAxis = false;

	LoadFragmentShader("shaders/alphaLine.glsl", "alphaLineFrag" );
	CreateShaderProgram("basicVertex", "alphaLineFrag", "alphaLineShader");
}

void Rendering::LineplotRenderer::Clear()
{
	while( plots.size() > 0 )
	{
		RemovePlot( plots.begin()->first );
	}
}

void Rendering::LineplotRenderer::AddPlot(std::vector< Eigen::Vector2f > dataPoints, std::string label, Eigen::Vector4f colour )
{
//	cout << "d0" << endl;
	if( !tex )
	{
		cv::Mat t(10,10, CV_8UC3, cv::Scalar(255,255,255) );
		tex.reset( new Texture( smartThis ) );
		tex->UploadImage(t);
	}
	
//	cout << "d1" << endl;
	
	for( unsigned dc = 0; dc < dataPoints.size(); ++dc )
	{
		if(!fixXAxis)
		{
			minX = std::min( minX, dataPoints[dc](0) );
			maxX = std::max( maxX, dataPoints[dc](0) );
		}

		if(!fixYAxis)
		{
			minY = std::min( minY, dataPoints[dc](1) - 0.05f*maxY);
			maxY = std::max( maxY, dataPoints[dc](1) + 0.05f*maxY);
		}
	}
	
//	cout << "d2" << endl;

	auto i = plots.find(label);
	if( i != plots.end() )
	{
		i->second.data   = dataPoints;
		i->second.colour = colour;
	}
	else
	{
		Plot plt;
		plt.label = label;
		plt.data = dataPoints;
		plt.colour = colour;

		plots[label] = plt;
	}
	
//	cout << "d3" << endl;



	Update();
	
//	cout << "d4" << endl;
}

void Rendering::LineplotRenderer::GetPlotMesh( Plot &plt )
{

	// how thick should the line be?
	float ww, wh;
	ww = win.getSize().x;
	wh = win.getSize().y;

	float dh, dw;
	dw = maxX - minX;
	dh = maxY - minY;

	float t = std::min( (3.5 / wh) * dh, (3.5 / ww) * dw );

	// if there's no meshnode, make one.
	if( !plt.node )
	{
		std::stringstream ss;
		ss << "plotMesh_" << plt.label;
		Rendering::NodeFactory::Create( plt.node, ss.str() );
	}

	// if there is a mesh, but it is the wrong size, get rid of it.
	if( plt.mesh && plt.mesh->vertices.cols() != plt.data.size()*2 )
	{
		plt.mesh = nullptr;
	}

	// if there's no mesh, make a new one.
	// note that asking OpenGL to draw a thick line is deprecated,
	// and as such, we thus need to draw thick lines by drawing thin
	// quads. Hence, we need twice as many vertices as we should.
	// Also, we can't use quads, they have to be triangles, so more
	// faces than we might like as well...
	if( !plt.mesh )
	{
		plt.mesh.reset( new Mesh( plt.data.size()*2, (plt.data.size()-1)*2, 3 ) );
	}

	// make sure the mesh is correct.
	for( unsigned dc = 0; dc < plt.data.size(); ++dc )
	{
		// we need to put a vertex just off the line centre
		// to be able to give us width. However, it is not
		// enough to just put it directly above, we must get
		// the width with respect to the direction of the line.
		Eigen::Vector2f d0, d1;
		bool gd0, gd1;
		gd0 = gd1 = false;
		if( dc < plt.data.size() -1 )
		{
			d1 = plt.data[dc+1] - plt.data[dc];
			gd1 = true;
		}
		if( dc > 0 )
		{
			d0 = plt.data[dc] - plt.data[dc-1];
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
		float l =  t * mitre.dot(n);


		// and that gives us two vertices...
		Eigen::Vector2f p = plt.data[dc] + l*mitre;
		plt.mesh->vertices(0, dc*2  ) = p(0);
		plt.mesh->vertices(1, dc*2  ) = p(1);
		plt.mesh->vertices(2, dc*2  ) = 0;
		plt.mesh->vertices(3, dc*2  ) = 1;

		p = plt.data[dc] - l*mitre;
		plt.mesh->vertices(0, dc*2+1 ) = p(0);
		plt.mesh->vertices(1, dc*2+1 ) = p(1);
		plt.mesh->vertices(2, dc*2+1 ) = 0;
		plt.mesh->vertices(3, dc*2+1 ) = 1;

		plt.mesh->texCoords(0, dc*2  ) =  0;
		plt.mesh->texCoords(1, dc*2  ) =  1;

		plt.mesh->texCoords(0, dc*2+1) =  0;
		plt.mesh->texCoords(1, dc*2+1) = -1;
	}


	for( unsigned dc = 0; dc < plt.data.size()-1; ++dc )
	{
		// faces are triangles, so two triangles per section.
		unsigned vi = dc*2;
		plt.mesh->faces(0, dc*2) = vi;
		plt.mesh->faces(1, dc*2) = vi+2;
		plt.mesh->faces(2, dc*2) = vi+1;

		plt.mesh->faces(0, dc*2+1) = vi+2;
		plt.mesh->faces(1, dc*2+1) = vi+3;
		plt.mesh->faces(2, dc*2+1) = vi+1;
	}

	plt.mesh->UploadToRenderer(smartThis);
	plt.node->SetMesh( plt.mesh );
	plt.node->SetShader( GetShaderProg("alphaLineShader") );
	plt.node->SetTexture(tex);
	plt.node->SetBaseColour( plt.colour );

	plots[plt.label] = plt;

	Get2dFgRoot()->AddChild( plt.node );

}

void Rendering::LineplotRenderer::Update()
{
	if( axes.size() > 0 )
	{
		axes["x"]->RemoveFromParent();
		axes["y"]->RemoveFromParent();
	}
	
	std::vector< Eigen::Vector2f > points(2);
	
	// how thick should the line be?
	float ww, wh;
	ww = win.getSize().x;
	wh = win.getSize().y;

	float dh, dw;
	dw = maxX - minX;
	dh = maxY - minY;

	float t = std::min( (3.5 / wh) * dh, (3.5 / ww) * dw );
	
	points[0] << minX-1, 0;
	points[1] << maxX+1, 0;
	axes["x"] = GenerateLineNode2D( points, 0.5*t, "X-axis", smartThis );
		
	points[0] << 0, minY-1;
	points[1] << 0, maxY+1;
	axes["y"] = GenerateLineNode2D( points, 0.5*t, "Y-axis", smartThis );
		
	Eigen::Vector4f grey; grey << 0.7, 0.7, 0.7, 0.4;
	axes["x"]->SetBaseColour( grey );
	axes["y"]->SetBaseColour( grey );
	
	Get2dFgRoot()->AddChild( axes["x"] );
	Get2dFgRoot()->AddChild( axes["y"] );
	
	Get2dFgCamera()->SetOrthoProjection(minX, maxX, maxY, minY, -100, 100 );
	for( auto i = plots.begin(); i != plots.end(); ++i )
	{
		GetPlotMesh(i->second);
	}
	
}

void Rendering::LineplotRenderer::RemovePlot(std::string label)
{
	auto i = plots.find(label);
	if( i != plots.end() )
	{
		i->second.node->RemoveFromParent();
		plots.erase(i);
	}
}


bool Rendering::LineplotRenderer::Step()
{
	Render();

	win.setActive();
	sf::Event ev;
	while( win.pollEvent(ev) )
	{
		if( ev.type == sf::Event::Closed )
			return false;
	}
	return true;
}
