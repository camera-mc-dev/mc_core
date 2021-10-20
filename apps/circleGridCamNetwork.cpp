#include "calib/camNetworkCalib.h"

int main(int argc, char *argv[] )
{
	// _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
	
	CamNetCalibrator calibrator(argv[1]);

	calibrator.Calibrate();
}
