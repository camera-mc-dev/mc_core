#ifdef HAVE_HIGH_FIVE

#include "imgio/sourceFactory.h"
#include "imgio/hdf5source.h"

int main(int argc, char *argv[] )
{
	if( argc != 3 )
	{
		cout << "test tool to take in an image source and write the images ito an hdf5 file" << endl;
		cout << "The main aim is to test writing images, especially raw images, such as from" << endl;
		cout << "a live camera grabber, into an hdf5 file instead of thousands of small files" << endl;
		cout << endl;
		cout << "Usage: " << endl;
		cout << argv[0] << " <input source> <output file> " << endl;
		cout << endl;
		exit(0);
	}
	
	auto sp = CreateSource( argv[1] );
	
	HDF5ImageWriter writer( argv[2] );
	
	bool done = false;
	cv::Mat img;
	while( !done )
	{
		img = sp.source->GetCurrent();
		
		writer.AddImage( img, sp.source->GetCurrentFrameID() );
		
		done = !sp.source->Advance();
	}
}

#endif
