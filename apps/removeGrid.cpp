#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
using std::cout;
using std::endl;
using std::vector;

#include "calib/camNetworkCalib.h"

int main(int argc, char* argv[])
{
	if( argc == 1 )
	{
		cout << "Tool to remove detected grid from grid file" << endl;
		cout << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <grid id> <grid file 0> ... <grid file n>";
		cout << endl << endl;
		exit(1);
	}
	
	int gridNum = atoi( argv[1] );
	
	std::vector< std::vector< std::vector< CircleGridDetector::GridPoint > > > grids;
	std::vector< std::string > gridsFiles;
	std::vector< bool > gotFile;
	for( unsigned c = 2; c < argc; ++c )
		gridsFiles.push_back( argv[c] );
	gotFile.assign( gridsFiles.size(), false );
	
	grids.resize( gridsFiles.size() );
	for( unsigned gfc = 0; gfc < gridsFiles.size(); ++gfc )
	{
		std::ifstream infi(gridsFiles[gfc]);
		if( infi )
		{
			// we have a grids file for this directory source.
			CamNetCalibrator::ReadGridsFile(infi, grids.at(gfc) );
			gotFile[ gfc ] = true;
		}
		else
		{
			cout << gridsFiles[gfc] << " - no" << endl;
		}
	}
	
	
	for( unsigned gfc = 0; gfc < gridsFiles.size(); ++gfc )
	{
		if( gotFile[gfc] )
		{
			if( gridNum >= 0 )
				grids[ gfc ][ gridNum ].clear();
			else
			{
				for( unsigned gc = 0; gc < grids[gfc].size(); ++gc )
					grids[gfc][gc].clear();
			}
			
			std::ofstream outfi( gridsFiles[gfc] );
			CamNetCalibrator::WriteGridsFile(outfi, grids.at(gfc) );
			outfi.close();
		}
	}
	
	
	return 0;
}
