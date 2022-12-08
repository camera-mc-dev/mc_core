#ifndef LINEPLOT_RENDERER
#define LINEPLOT_RENDERER

#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "renderer2/sdfText.h"

#include <map>
#include <vector>

namespace Rendering
{

	class LineplotRenderer : public Rendering::BasicRenderer
	{
		friend class RendererFactory;
		template<class T0, class T1 > friend class RenWrapper;
		
	protected:
		LineplotRenderer(unsigned width, unsigned height, std::string title);
	
	public:
		~LineplotRenderer()
		{
			Clear();
		}
		void Clear();
		void AddPlot(std::vector< Eigen::Vector2f > dataPoints, std::string label, Eigen::Vector4f colour );
		void RemovePlot( std::string label );
		void Update();

		bool fixXAxis;
		bool fixYAxis;
		float minX, maxX;
		float minY, maxY;

		std::shared_ptr<Texture> tex;
		
		bool Step();

	private:

		struct Plot
		{
			std::string label;
			std::vector< Eigen::Vector2f > data;
			std::shared_ptr<Mesh>     mesh;
			std::shared_ptr<MeshNode> node;
			Eigen::Vector4f colour;
		};

		std::map< std::string, Plot > plots;

		void GetPlotMesh(Plot &plt);
		
		std::map< std::string, std::shared_ptr< MeshNode > > axes;



	};

}

#endif
