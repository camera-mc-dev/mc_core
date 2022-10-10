#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
using std::cout;
using std::endl;
using std::vector;

#include "imgio/sourceFactory.h"
#include "calib/camNetworkCalib.h"

#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "commonConfig/commonConfig.h"

#include <boost/filesystem.hpp>



class PauseAdvanceRegressRenderer : public Rendering::BasicRenderer
{
public:
	friend class Rendering::RendererFactory;		

	bool Step(bool &paused, bool &advance, bool &regress)
	{
		win.setActive();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Render();

		sf::Event ev;
		while( win.pollEvent(ev) )
		{
			if( ev.type == sf::Event::Closed )
				return true;
			
			if( ev.type == sf::Event::KeyReleased )
			{
				if (ev.key.code == sf::Keyboard::Space )
				{
					paused = !paused;
				}
				if (paused && ev.key.code == sf::Keyboard::Up )
				{
					advance = true;
				}
				if (paused && ev.key.code == sf::Keyboard::Down )
				{
					regress = true;
				}
			}
			
		}
		return false;
	}
protected:
	PauseAdvanceRegressRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title)
	{

	}
};




int main(int argc, char* argv[])
{
	if( argc == 1 )
	{
		cout << "Tool to display synchronised images sources side-by-side" << endl;
		cout << "Handles either image or video sources." << endl;
		cout << "Usage:" << endl;
		cout << argv[0] << " <source 0> <source 1> ... <source 15>";
		cout << endl << endl;
		exit(1);
	}
	
	CommonConfig ccfg;
	
	
	// if we have suitable sources, and that source has calibration grids, then it can be useful
	// to visualise the calibration grids on the sources. This is particularly useful for making
	// sure that the grids all have the same row/column point ids.
	std::vector< std::vector< std::vector< CircleGridDetector::GridPoint > > > grids;
	std::vector< std::string > gridsFiles;
	
	// create the image sources
	// At present, although the functionality is meant to be there,
	// the OpenCV methods to move to a specific frame of video are not working.
	// as such, we need a slightly different process if we use video sources.
	bool isVideoSources = false;
	std::vector< std::shared_ptr<ImageSource> > sources;
	for( unsigned ac = 1; ac < argc; ++ac )
	{
		// Let the factory make the source, but we'll do some extra work to see if there are 
		// grid files or calib files in default locations.

		std::string cfn = std::string(argv[ac]) + std::string(".calib");
		if( boost::filesystem::exists( cfn ) )
		{
			auto sp = CreateSource( argv[ac], cfn );
			sources.push_back( sp.source );
		}
		else
		{
			auto sp = CreateSource( argv[ac] );
			sources.push_back( sp.source );
		}
		
		
		
		std::string gfn0 = std::string(argv[ac]) + std::string(".grids");
		std::string gfn1 = std::string(argv[ac]) + std::string("/grids");
		if( boost::filesystem::exists( gfn0 ) )
		{
			gridsFiles.push_back(gfn0);
		}
		else if( boost::filesystem::exists( gfn1 ) )
		{
			gridsFiles.push_back(gfn1);
		}
		else
		{
			gridsFiles.push_back("none");
		}
		
		
	}
	
	cout << "got: " << sources.size() << " sources " << endl;
	grids.resize( sources.size() );
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		std::ifstream infi(gridsFiles[isc]);
		if( infi )
		{
			// we have a grids file for this directory source.
			CamNetCalibrator::ReadGridsFile(infi, grids.at(isc) );
			
			cout << "source " << isc << " had grids file." << endl;
		}
		else
		{
			cout << gridsFiles[isc] << " - no" << endl;
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
	
	float winW = ccfg.maxSingleWindowWidth;
	float winH = winW * (maxH * numRows)/(maxW * numCols);
	if( winH > ccfg.maxSingleWindowHeight )
	{
		winH = ccfg.maxSingleWindowHeight;
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
	std::shared_ptr<PauseAdvanceRegressRenderer> renderer;
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

	
	bool done   = false;
	bool paused = true;
	bool advance = false;
	bool regress = false;
	unsigned ic = 0;
	while( ic < minFrames && !done )
	{
		done = renderer->Step(paused, advance, regress);
		while( paused && !advance && !regress )
		{
			done = renderer->Step( paused, advance, regress );
			if( done )
				break;
		}
		
// 		cv::Mat grab;
// 		grab = renderer->Capture();
// 		std::stringstream ss;
// 		ss << "maskCompareVid/" << std::setw(6) << std::setfill('0') << ic << ".jpg";
// 		SaveImage( grab, ss.str() );
		
		if( !paused || advance )
		{
			++ic;
			for( unsigned isc = 0; isc < sources.size(); ++isc )
			{
				sources[isc]->Advance();
			}
		}
		else if( regress )
		{
			if( ic > 0 )
			{
				ic -= 1;
				for( unsigned isc = 0; isc < sources.size(); ++isc )
					sources[isc]->JumpToFrame(ic);
			}
		}
		
		if( !paused || (advance || regress) )
		{
			for( unsigned isc = 0; isc < sources.size(); ++isc )
			{
				cv::Mat img = sources[isc]->GetCurrent();
				if( grids.size() > 0 && grids[isc].size() > 0 && grids[isc][ic].size() > 0 )
				{
					float nrows = 0;
					float ncols = 0;
					for( unsigned pc = 0; pc < grids[isc][ic].size(); ++pc )
					{
						nrows = std::max( nrows, grids[isc][ic][pc].row+1.0f );
						ncols = std::max( nrows, grids[isc][ic][pc].col+1.0f );
					}
					cv::Point oCorner, xCorner, yCorner;
					int xCornerCol, yCornerRow;
					xCornerCol = yCornerRow = 0;
					for( unsigned pc = 0; pc < grids[isc][ic].size(); ++pc )
					{
						float x,y;
						x = grids[isc][ic][pc].pi(0);
						y = grids[isc][ic][pc].pi(1);
						int row,col;
						row = grids[isc][ic][pc].row;
						col = grids[isc][ic][pc].col;
						
						float red,green,blue;
						red   = (row/nrows) * 255.0f;
						green = (col/ncols) * 255.0f;
						blue  = 255.0f;
						
						if( row == 0 && col == 0 )
						{
							oCorner = cv::Point(x,y);
						}
						else if( row == 0 && col > xCornerCol )
						{
							xCorner = cv::Point(x,y);
							xCornerCol = col;
						}
						else if( col == 0 && row > yCornerRow )
						{
							yCorner = cv::Point(x,y);
							yCornerRow = row;
						}
						
						cv::circle( img, cv::Point(x,y), 15, cv::Scalar(blue,green,red), 4 );
					}
					
					//
					// draw an arrow from origin to x-axis, y-axis corners.
					//
					if( xCornerCol > 0 && yCornerRow > 0 )
					{
						cv::line( img, oCorner, xCorner, cv::Scalar(255,255,255), 7 );
						cv::line( img, oCorner, xCorner, cv::Scalar(0,0,255), 3 );
						
						cv::line( img, oCorner, yCorner, cv::Scalar(0,0,0), 7 );
						cv::line( img, oCorner, yCorner, cv::Scalar(0,255,0), 3 );
					}
					
				}
				
				imgCards[isc]->GetTexture()->UploadImage( img );
			}
			cout << ic << endl;
		}
		
		advance = regress = false;
	}

}
