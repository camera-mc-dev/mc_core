#include <iostream>
#include <ofstream>
#include <vector>
#include <string>
using std::cout;
using std::endl;
using std::vector;

#include "math/intersections.h"
include "math/matrixGenerators.h"

int main(int argc, char *argv[] )
{
	if( argc < 3 )
	{
		cout << "Tool to calculate the closest point to all the optical axes of the provided viewpoints." << endl;
		cout << "Assumes cameras are in a ring or half dome and looking at a \"centre\"." << endl;
		cout << "Usage: " << endl;
		cout << argv[0] << " < calib file 0> < calib file 1> ... < calib file n-1 > " << endl;
		exit(1);
	}
	
	std::vector< Calibration > calibs;
	calibs.resize( argc - 1 );
	
	for( unsigned ac = 1; ac < argc; ++ac )
	{
		calibs[ ac-1 ].Read( argv[ac] );
	}
	
	hVec3D zd; zd << 0,0,1,0;
	std::vector< hVec3D > starts( calibs.size() ), dirs( calibs.size() );
	for( unsigned cc = 0; cc < calibs.size(); ++cc )
	{
		starts[cc] = calibs[cc].GetCameraCentre();
		dirs[cc]   = calibs[cc].TransformToWorld( zd );
	}
	
	hVec3D viewCentre = IntersectRays( starts, dirs );
	
	std::ofstream resfi( "/tmp/viewDir" );
	resfi << viewCentre(0) << " " << viewCentre(1) << " " << viewCentre(2) << " " << viewCentre(3) << endl;
	
	cout << "view centre: " << viewCentre.transpose() << endl;
	
	return 0;
}




