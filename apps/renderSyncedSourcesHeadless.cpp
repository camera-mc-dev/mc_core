#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
using std::cout;
using std::endl;
using std::vector;

#include "imgio/imagesource.h"
#include "imgio/vidsrc.h"
#include "calib/camNetworkCalib.h"

#include "renderer2/basicHeadlessRenderer.h"
#include "renderer2/geomTools.h"
#include "commonConfig/commonConfig.h"






int main(int argc, char* argv[])
{
	if( argc == 1 )
	{
		cout << "Tool to display synchronised images sources side-by-side" << endl;
		cout << "Handles either image or video sources." << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <outputDir> <source 0> <source 1> ... <source 15>";
		cout << endl << endl;
		exit(1);
	}
	
	CommonConfig ccfg;
	
	std::string outputDir( argv[1] );
	boost::filesystem::path p(outputDir);
	if( !boost::filesystem::exists(p) )
	{
		boost::filesystem::create_directories(p);
	}

	if( !boost::filesystem::is_directory(p) )
	{
		throw std::runtime_error("Output location exists and is not a directory." );
	}
	
	
	// if we have suitable sources, and that source has calibration grids, then it can be useful
	// to visualise the calibration grids on the sources. This is particularly useful for making
	// sure that the grids all have the same row/column point ids.
	std::vector< std::vector< std::vector< CircleGridDetector::GridPoint > > > grids;
	
	// create the image sources
	// At present, although the functionality is meant to be there,
	// the OpenCV methods to move to a specific frame of video are not working.
	// as such, we need a slightly different process if we use video sources.
	bool isVideoSources = false;
	std::vector< ImageSource* > sources;
	for( unsigned ac = 2; ac < argc; ++ac )
	{
		// is it a directory?
		if( boost::filesystem::is_directory( argv[ac] ))
		{
			auto src = (ImageSource*) new ImageDirectory( argv[ac] );
			sources.push_back(src);
		}
		else
		{
			auto src = (ImageSource*) new VideoSource( argv[ac], "none");
			sources.push_back(src);
			isVideoSources = true;
			
			// although we could do grids with video sources, we would need the calibration,
			// and right now, renderSyncedSources is only getting the video filename.
			// std::string gfn;
			// gfn = imgDirs[isc] + ".grids";
		}
	}

	cout << "got: " << sources.size() << " sources " << endl;
	
	grids.resize( sources.size() );
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		if( !isVideoSources )
		{
			// are there grids for this source?
			std::string gfn;
			gfn = std::string(argv[isc+1]) + "/grids";
			cout << gfn << " ? " << endl;
			std::ifstream infi(gfn);
			if( infi )
			{
				// we have a grids file for this directory source.
				CamNetCalibrator::ReadGridsFile(infi, grids.at(isc) );
				
				cout << "source " << isc << " had grids file." << endl;
			}
		}
	}

	// find the source with the least # of frames.
	unsigned minFrames = sources[0]->GetNumImages();
	for( unsigned isc = 1; isc < sources.size(); ++isc )
	{
		minFrames = std::min( minFrames, (unsigned)sources[isc]->GetNumImages() );
	}

	// decide how many rows and columns we want to show.
	unsigned numRows = 1;
	unsigned numCols = 1;
	if( sources.size() > 16)
		throw std::runtime_error("I'm just refusing to show that many images!");
	else
	{
		numRows = numCols = (int)ceil( sqrt(sources.size()) );
	}

	cout << "num rows: " << numRows << endl;

	// find out how big a window to make
	cout << "sizes of first images: " << endl;
	float maxW, maxH;
	maxW = maxH = 0.0;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		std::shared_ptr<Rendering::MeshNode> mn;
		cv::Mat img = sources[isc]->GetCurrent();
		
		maxW = std::max( (float)img.cols, maxW );
		maxH = std::max( (float)img.rows, maxH );
		
		cout << "\t" << img.cols << " " << img.rows << endl;
	}
	
	
	// enforcing a maximum window width and height of 800, regardless of what
	// the common config says
	float winW = std::min(4000u, ccfg.maxSingleWindowWidth);
	float winH = winW * (maxH * numRows)/(maxW * numCols);
	if( winH > std::min(4000u, ccfg.maxSingleWindowHeight) )
	{
		winH = std::min(4000u, ccfg.maxSingleWindowHeight);
		winW = winH * (maxW * numCols) / (maxH * numRows);
	}
	
	float rat = winH / winW;
	
	if( winW > ccfg.maxSingleWindowWidth )
	{
		winW = ccfg.maxSingleWindowWidth;
		winH = rat * winW / rat;
	}
	
	cout << "win W, H: " << winW << ", " << winH << endl;
	
	// create renderer
	std::shared_ptr<Rendering::BasicHeadlessRenderer> renderer;
	Rendering::RendererFactory::Create( renderer, winW,winH, "image source viewer" );
	
	float renW, renH;
	renW = numCols * maxW;
	renH = numRows * maxH;
	
	renderer->Get2dBgCamera()->SetOrthoProjection(0, renW, 0, renH, -10, 10 );
	
	std::vector< std::shared_ptr<Rendering::MeshNode> > imgCards;
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		std::shared_ptr<Rendering::MeshNode> mn;
		cv::Mat img = sources[isc]->GetCurrent();
		std::stringstream ss; ss << "image-" << isc;
		
		unsigned r = isc/numCols;
		unsigned c = isc%numCols;
		
		float x,y;
		x = c * maxW;
		y = r * maxH;
		
		mn = Rendering::GenerateImageNode( x, y, img.cols, img, ss.str(), renderer );
		imgCards.push_back(mn);
		renderer->Get2dBgRoot()->AddChild( mn );
	}
	
	
	unsigned ic = 0;
	bool done = false;
	while( ic < minFrames && !done )
	{
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			imgCards[isc]->GetTexture()->UploadImage( sources[isc]->GetCurrent() );
		}
		renderer->StepEventLoop();
		
		cv::Mat grab;
		grab = renderer->Capture();
		std::stringstream ss;
		ss << outputDir << "/"  << std::setw(6) << std::setfill('0') << ic << ".jpg";
		SaveImage( grab, ss.str() );
		
		++ic;
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			sources[isc]->Advance();
		}
		
		cout << ic << endl;
	}

}
