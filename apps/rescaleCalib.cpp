#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
using std::cout;
using std::endl;
using std::vector;

#include "imgio/sourceFactory.h"
#include "calib/calibration.h"

int main(int argc, char* argv[])
{
	if( argc != 5 )
	{
		cout << "Rescale a calibration file for a new image width" << endl;
		cout << "Specify input calibration file, output calibration file, new image width and height" << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " < in file > < out file > < out width > <out height >";
		cout << endl << endl;
		exit(1);
	}
	
	std::string cfn( argv[1] );
	std::string ofn( argv[2] );
	
	int outWidth = atoi( argv[3] );
	int outHeight = atoi( argv[4] );
	
	Calibration cal;
	cal.Read( cfn );
	
	cal.RescaleImage( outWidth, outHeight );
	
	cal.Write( ofn ); 
}
