#ifndef MC_PLY_READER_H
#define MC_PLY_READER_H

#include "math/mathTypes.h"

//
//  Load a point cloud.
//  This is not the fastest of loaders, but lets us hack the different use cases we have into it.
//  NOTE: Assumes there is only a single "element" named vertex. Will ignore other elements.
//        Will group recognised properties together: 
//         > basic properties:
//           - ( x, y, z) : "xyz"
//           - ( r, g, b) : "rgb"         if uchar will be scaled to float 0->1
//           - (nx,ny,nz) : "normal"
//
//         > gaussian splat properties:
//           - (     sx,      sy,       sz) : "gs-scale"
//           - (scale_0, scale_1,  scale_2) : "gs-scale"
//           - (rot_0, rot_1, rot_2, rot_3) : "gs-qwxyz"
//           - (f_dc_0,   f_dc_1,   f_dc_2) : "gs-fdc"
//           - (f_rest_0, ..., f_rest_xx  ) : "gs-frest"
//           - (opacity)                    : "gs-opacity:
//
std::map<std::string, genRowMajMatrix> LoadPlyPointCloud( std::string infn );



#endif
