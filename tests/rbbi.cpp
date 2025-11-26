#include "math/intersections.h"
#include <iostream>
using std::cout;
using std::endl;

int main(void)
{
	hVec3D p0; p0 <<  0.0f, 0.0f, -5.0f, 1.0f;
	hVec3D rd; rd <<  0.0f, 0.0f,  1.0f, 0.0f;
	
	for( float x = -5.0; x < 5.0; x += 0.02 )
	{
		float tm, tM;
		bool tst = IntersectRayAABox3D( p0, rd, {{x-2.0f, x+1.0f},{-1.0f,1.0f},{-1.0f,1.0f}}, tm, tM );
		cout << x << " : " << tst << " " << tm << " " << tM << endl;
	}
}
