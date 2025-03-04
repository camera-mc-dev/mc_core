#ifndef MC_PLY_READER_H
#define MC_PLY_READER_H

#include "math/mathTypes.h"

//
//  Load a point cloud with xyz and rgb.
//    returns matrix ( x,y,z, r,g,b )
//    r,g,b is in range 0->1 but file is assumed to be rgb as unsigned char.
//
genRowMajMatrix LoadPlyPointRGB( std::string infn );
genRowMajMatrix LoadPlyTxtPointRGB( std::string infn );


#endif
