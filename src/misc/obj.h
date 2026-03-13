#ifndef MC_DEV_OBJ_H
#define MC_DEV_OBJ_H

#include "renderer2/sceneGraph.h"
#include "renderer2/geomTools.h"

void LoadOBJ( std::string filepath, std::shared_ptr< Rendering::Mesh > &mesh, cv::Mat &texImg );
void LoadOBJ( std::string filepath, std::shared_ptr< Rendering::Mesh > &mesh );



#endif
