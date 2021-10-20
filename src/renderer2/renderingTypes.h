#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

#include <Eigen/Dense>

namespace Rendering
{

	// OpenGL compatible matrices for storing
	// vertex data for rendering purposes.
	typedef Eigen::Matrix <float,    Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor > vertexMatrix;
	typedef Eigen::Matrix <unsigned, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor > primitiveMatrix;
}
#endif
