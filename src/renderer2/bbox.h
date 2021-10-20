#include "renderer2/mesh.h"


namespace Rendering
{
	class BBox2D : public Mesh
	{
	public:
		BBox2D(hVec2D tl, hVec2D br, float thickness=3);
		BBox2D(cv::Rect r, float thickness=3);
		virtual ~BBox2D();


		void Set(hVec2D tl, hVec2D br, float thickness=3);
		void Set( cv::Rect r, float thickness=3 );
	};
};
