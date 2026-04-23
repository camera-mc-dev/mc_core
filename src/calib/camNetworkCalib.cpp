#include <algorithm>
#include <sstream>
#include <thread>
#include <cmath>

#include "calib/camNetworkCalib.h"
#include "math/intersections.h"
#include "imgio/sourceFactory.h"
#include "calib/calibrationC.h"
#include "commonConfig/commonConfig.h"

#include "renderer2/basicHeadlessRenderer.h"

#include <iomanip>

#include <opencv2/sfm/fundamental.hpp>

void CamNetCalibrator::ReadConfig()
{
	CommonConfig ccfg;
	dataRoot = ccfg.dataRoot;
	auxMatchesFile = "";
	try
	{
		cfg.readFile(cfgFile.c_str());
		if( cfg.exists("dataRoot") )
			dataRoot = (const char*)cfg.lookup("dataRoot");
		
		testRoot = (const char*)cfg.lookup("testRoot");
		libconfig::Setting &idirs = cfg.lookup("imgDirs");
		for( unsigned ic = 0; ic < idirs.getLength(); ++ic )
		{
			std::string s;
			s = dataRoot + testRoot + (const char*) idirs[ic];
			imgDirs.push_back(s);

			srcId2Indx[ (const char*) idirs[ic] ] = ic;
		}
		maxGridsForInitial = cfg.lookup("maxGridsForInitial");
		
		if( cfg.exists("downHints") )
		{
			libconfig::Setting &dhints = cfg.lookup("downHints");
			assert( dhints.getLength() == imgDirs.size() );
			downHints.resize( dhints.getLength() );
			for( unsigned c = 0; c < downHints.size(); ++c )
			{
				downHints[c] << dhints[c][0], dhints[c][1], 0.0f;
			}
		}
		else
		{
			cout << "no downHints in config file, assuming down is (0,1)" << endl;
			sleep(2);
			downHints.resize( imgDirs.size() );
			for( unsigned c = 0; c < downHints.size(); ++c )
			{
				downHints[c] << 0, 1, 0;
			}
		}

		// Basic information about the grid
		gridRows = cfg.lookup("grid.rows");
		gridCols = cfg.lookup("grid.cols");
		gridRSpacing = cfg.lookup("grid.rspacing");
		gridCSpacing = cfg.lookup("grid.cspacing");
		isGridLightOnDark = cfg.lookup("grid.isLightOnDark");
		if( cfg.exists("grid.useHypothesis") )
			useHypothesis     = cfg.lookup("grid.useHypothesis");
		
		if( cfg.exists("grid.hasAlignmentDots") )
			gridHasAlignmentDots = cfg.lookup("grid.hasAlignmentDots");
		
		minSharedGrids = cfg.lookup("minSharedGrids");
		sbaVerbosity = cfg.lookup("SBAVerbosity");

		visualise = 0;
		if( cfg.exists("visualise") )
			visualise = cfg.lookup("visualise");
		
		forceOneCam = true;
		if( cfg.exists("forceOneCam") )
			forceOneCam = cfg.lookup("forceOneCam");
			
		intrinsicsOnly = false;
		if( cfg.exists("intrinsicsOnly") )
			intrinsicsOnly = cfg.lookup("intrinsicsOnly");
		
		onlyExtrinsicsForBA = false;
		if( cfg.exists("onlyExtrinsicsForBA") )
			onlyExtrinsicsForBA = cfg.lookup("onlyExtrinsicsForBA");
		
		minGridsToInitialiseCam = 4;
		if( cfg.exists("minGridsToInitialiseCam") )
			minGridsToInitialiseCam = cfg.lookup("minGridsToInitialiseCam");

		if( cfg.exists("matchesFile") )
		{
			auxMatchesFile = dataRoot + testRoot + (const char*)cfg.lookup("matchesFile");
		}
		
		rootCam = 99999999;
		if( cfg.exists("rootCam") )
		{
			rootCam = cfg.lookup("rootCam");
		}
		
		
		numIntrinsicsToSolve = 3;
		if( cfg.exists("numIntrinsicsToSolve" ) )
		{
			numIntrinsicsToSolve = cfg.lookup("numIntrinsicsToSolve");
		}
		assert( numIntrinsicsToSolve == 5 || numIntrinsicsToSolve == 4 || numIntrinsicsToSolve == 3 || numIntrinsicsToSolve == 1 || numIntrinsicsToSolve == 0 );
		numDistortionToSolve = 2;
		if( cfg.exists("numDistortionToSolve" ) )
		{
			numDistortionToSolve = cfg.lookup("numDistortionToSolve");
		}
		assert( numDistortionToSolve == 5 || numDistortionToSolve == 4 || numDistortionToSolve == 2 || numDistortionToSolve == 1 || numDistortionToSolve == 0 );
	}
	catch( libconfig::SettingException &e)
	{
		cout << "Setting error: " << endl;
		cout << e.what() << endl;
		cout << e.getPath() << endl;
		exit(0);
	}
	catch( libconfig::ParseException &e )
	{
		cout << "Parse error:" << endl;
		cout << e.what() << endl;
		cout << e.getError() << endl;
		cout << e.getFile() << endl;
		cout << e.getLine() << endl;
		exit(1);
	}
	
	
	if( cfg.exists("useSBA") && cfg.lookup("useSBA") )
	{
		cout << "Bundle Adjust will solve for: " << endl;
		cout << numIntrinsicsToSolve << " intrinsics: ";
		std::vector< std::string > intrinsicNames;
		intrinsicNames.push_back( "f" );
		intrinsicNames.push_back( "cx" );
		intrinsicNames.push_back( "cy" );
		intrinsicNames.push_back( "a" );
		intrinsicNames.push_back( "s" );
		for( unsigned ic = 0; ic < numIntrinsicsToSolve; ++ic )
		{
			cout << intrinsicNames[ic] << " ";
		}
		cout << endl;
		
		cout << numDistortionToSolve << " distortions: ";
		std::vector< std::string > distNames;
		distNames.push_back( "k1 (r2)" );
		distNames.push_back( "k2 (r4)" );
		distNames.push_back( "p1" );
		distNames.push_back( "p2" );
		distNames.push_back( "k3 (r6)" );
		for( unsigned ic = 0; ic < numDistortionToSolve; ++ic )
		{
			cout << distNames[ic] << " ";
		}
		cout << endl;
	}
	

	// now create the image sources
	// we'll assume they are image directories.
	for( unsigned ic = 0; ic < imgDirs.size(); ++ic )
	{
		cout << "creating source: " << imgDirs[ic] << endl;
		/*
		ImageSource *isrc;
		if( boost::filesystem::is_directory( imgDirs[ic] ))
		{
			isrc = (ImageSource*) new ImageDirectory(imgDirs[ic]);
			isDirectorySource.push_back(true);
		}
		else
		{
			std::string cfn = imgDirs[ic] + std::string(".calib");
			if( boost::filesystem::exists( cfn ) )
				isrc = (ImageSource*) new VideoSource(imgDirs[ic], cfn);
			else
				isrc = (ImageSource*) new VideoSource(imgDirs[ic], "none");
			isDirectorySource.push_back(false);
		}
		sources.push_back( isrc );
		/*/
		auto sh = CreateSource( imgDirs[ic] );
		sources.push_back( sh.source );
		isDirectorySource.push_back( sh.isDirectorySource );
		tagFreePaths.push_back( sh.path );
		//*/
	}

	if( auxMatchesFile.size() > 0 )
		LoadAuxMatches();
	
	std::stringstream ss;
	ss << dataRoot << "/" << testRoot << "/grids3D";
	out3DFile = ss.str();
	
	ss.str("");
	ss << dataRoot << "/" << testRoot << "/gridErrors";
	outErrsFile = ss.str();
	
	cout << out3DFile << endl;
	
}

void CamNetCalibrator::LoadAuxMatches()
{
	auxMatches.clear();
	std::ifstream infi( auxMatchesFile );
	if( !infi )
	{
		cout << "============================================================================================" << endl;
		cout << "      warning! matches file: '" << auxMatchesFile << "' could not be opened " << endl;
		cout << "============================================================================================" << endl;
		sleep(2);
		return;
	}

	unsigned numSources;
	infi >> numSources;
	std::vector<std::string> srcIds;
	for(unsigned ic = 0; ic < numSources; ++ic )
	{
		std::string s;
		infi >> s;
		srcIds.push_back(s);
	}

	unsigned numMatches;
	infi >> numMatches;
	for( unsigned mc = 0; mc < numMatches; ++mc )
	{
		unsigned id;
		unsigned numViews;
		infi >> id;
		infi >> numViews;

		PointMatch pm;
		pm.id = id;
		pm.has3D = false;
		for(unsigned vc = 0; vc < numViews; ++vc )
		{
			std::string sid;
			infi >> sid;
			hVec2D p;
			infi >> p(0);
			infi >> p(1);
			infi >> p(2);

			unsigned indx = srcId2Indx[sid];
			pm.p2D[indx] = p;
		}
		auxMatches.push_back(pm);
	}
	cout << "Read " << auxMatches.size() << " auxilliary point matches" << endl;
}



void CamNetCalibrator::Calibrate()
{
	GetGrids();
	CalibrateIntrinsics();
	
	if( intrinsicsOnly )
	{
		Ls.assign( grids.size(), transMatrix3D::Identity() );
	}
	else
	{
		

		// make sure we only have grids that are visible in more than
		// a single camera - unless we only have one camera!
		std::vector< std::vector< std::vector< CircleGridDetector::GridPoint > > > valGrids;

		unsigned numTotalGrids = grids[0].size();
		for( unsigned cc = 0; cc < grids.size(); ++cc )
		{
			numTotalGrids = std::min( (size_t)numTotalGrids, grids[cc].size() );
		}
		
		valGrids.resize(grids.size());
		gridFrames.clear();
		for( unsigned gc = 0; gc < numTotalGrids; ++gc )
		{
			unsigned cnt = 0;
			for( unsigned cc = 0; cc < grids.size(); ++cc )
			{
				if( grids[cc][gc].size() > 0 )
					++cnt;
			}

			if(cnt > 1 || grids.size() == 1)	// we care about grids with more than one camera, or all grids if there's only one camera.
			{
				for( unsigned cc = 0; cc < grids.size(); ++cc )
				{
					valGrids[cc].push_back( grids[cc][gc] );
				}
				gridFrames.push_back(gc);
			}
		}

		grids.clear();
		grids = valGrids;

		DetermineGridVisibility();

		//exit(1);

		CalibrateExtrinsics();
	}

	cout << "done calibration." << endl;
	cout << "saving..." << endl;
	
	
	SaveResults();

	

	hVec3D o;
	o << 0,0,0,1.0;
	cout << "camera centres: " << endl;
	for( unsigned cc = 0; cc < Ls.size(); ++cc )
	{
		cout << (Ls[cc].inverse() * o).transpose() << endl;
	}

}

void CamNetCalibrator::FindGridThread(std::shared_ptr<ImageSource> dir, unsigned isc, omp_lock_t &coutLock, hVec2D downHint)
{
	// Print startup message.
	//coutLock.lock();
	omp_set_lock( &coutLock );
	cout << "Thread for directory number: " << isc << " has started." << endl;
	// cout << dir->GetCurrent().cols << " " << dir->GetCurrent().rows << " " << isGridLightOnDark << " " << useHypothesis << " " << gridRows << " " << gridCols << endl;
	

	unsigned imgCount = 0, gridCount = 0;
	cv::Mat currentImage, grey;

	
	CircleGridDetector *cgd;
	if( cfg.exists("gridFinder" ) )
	{
		cgd = new CircleGridDetector( dir->GetCurrent().cols,dir->GetCurrent().rows, cfg.lookup("gridFinder"), downHint );
	}
	else
	{
		cgd = new CircleGridDetector(dir->GetCurrent().cols,dir->GetCurrent().rows, useHypothesis, false, CircleGridDetector::MSER_t, downHint );
	}
	
	cout << "potentialLinesNumNearest     : " << cgd->potentialLinesNumNearest      << endl;
	cout << "parallelLineAngleThresh      : " << cgd->parallelLineAngleThresh       << endl;
	cout << "parallelLineLengthRatioThresh: " << cgd->parallelLineLengthRatioThresh << endl;
	
	cout << "gridLinesParallelThresh      : " << cgd->gridLinesParallelThresh       << endl;
	cout << "gridLinesPerpendicularThresh : " << cgd->gridLinesPerpendicularThresh  << endl;
	
	cout << "gapThresh                    : " << cgd->gapThresh                     << endl;
	
	cout << "alignDotDistanceThresh       : " << cgd->alignDotDistanceThresh        << endl;
	cout << "alignDotSizeDiffThresh       : " << cgd->alignDotSizeDiffThresh        << endl;
	
	cout << "MSER_delta                   : " << cgd->MSER_delta                    << endl;
	cout << "MSER_minArea                 : " << cgd->MSER_minArea                  << endl;
	cout << "MSER_maxArea                 : " << cgd->MSER_maxArea                  << endl;
	cout << "MSER_maxVariation            : " << cgd->MSER_maxVariation             << endl;
	
	cout << "maxGridlineError             : " << cgd->maxGridlineError              << endl;
	cout << "maxHypPointDist              : " << cgd->maxHypPointDist               << endl;
	
// 	exit(0);
	
	omp_unset_lock( &coutLock );
// 	coutLock.unlock();
	
	std::vector< CircleGridDetector::GridPoint > gps;
	do
	{
		// Get current image and convert it to greyscale.
		currentImage = dir->GetCurrent();
		cv::cvtColor(currentImage, grey, cv::COLOR_BGR2GRAY);
		++imgCount;
		gridProg[isc] = imgCount;
		
		// Find grid.
		cgd->FindGrid(grey, gridRows, gridCols, gridHasAlignmentDots, isGridLightOnDark,  gps);
		
		// if we didn't find anything, we still want to push
		// in something, even if empty.
		grids[isc].push_back(gps);
		
		// Increment grid number if valid grid found.
		if( gps.size() > 0 )
		{
			++gridCount;
			
			++gridFound[isc];
		}
		
		// Print status message.
// 		coutLock.lock();
		omp_set_lock( &coutLock );
		//cout << "Dir No. " << isc << ": " << gridCount << " grids out of " << imgCount << " images." << endl;
		
		for( unsigned c = 0; c < gridFound.size(); ++c )
		{
			cout << "Dir No. " << std::setw(4) << c << " : " << std::setw(6) << gridFound[c] << " grids out of " << std::setw(6) << gridProg[c] << " images " << endl;
		}
		cout << "-------------------------------------" << endl;
		
		omp_unset_lock( &coutLock );
// 		coutLock.unlock();

	} while(dir->Advance()); // Stop when reach end of image directory.

	// Print status message.
// 	coutLock.lock();
	omp_set_lock( &coutLock );
	cout << "Finding grids for directory number " << isc << " has finished." << endl;
	omp_unset_lock( &coutLock );
// 	coutLock.unlock();

	// Save grids to a file.
	std::string filePath;
	if( isDirectorySource[isc] )
		filePath = tagFreePaths[isc] + "grids";
	else
		filePath = tagFreePaths[isc] + ".grids";
// 	coutLock.lock();
	omp_set_lock( &coutLock );
	cout << "Writing grids file: " << filePath << endl;
	omp_unset_lock( &coutLock );
// 	coutLock.unlock();

	std::ofstream gridFile(filePath);
	WriteGridsFile( gridFile, grids[isc] );
	gridFile.close();

	delete cgd;
	
	// Print termination message.
// 	coutLock.lock();
	omp_set_lock( &coutLock );
	cout << "Thread for source number " << isc << " has finished." << endl;
	omp_unset_lock( &coutLock );
// 	coutLock.unlock();
}

void CamNetCalibrator::GetGrids()
{
	grids.clear();
	grids.resize( sources.size() );

	if( cfg.exists("useExistingGrids") && cfg.lookup("useExistingGrids") )
	{
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			std::string s;
			if( isDirectorySource[isc] )
				s = tagFreePaths[isc] + "grids";
			else
				s = tagFreePaths[isc] + ".grids";
			std::ifstream infi(s);

			if( !infi.is_open() )
			{
				throw std::runtime_error(std::string("Could not open grids file: ") + s);
			}

			cout << "Reading grids from file: " << s << endl;

			ReadGridsFile(infi, grids.at(isc) );
			
			cout << grids[isc].size() << endl;
			int count = 0;
			int icount = 0;
			for( unsigned gc = 0; gc < grids[isc].size(); ++gc )
			{
				if( grids[isc][gc].size() > 0 && grids[isc][gc].size() != gridRows*gridCols )
				{
// 					cout << "\t incomplete grid - ignoring." << endl;
					grids[isc][gc].clear();
					++icount;
				}
				else if( grids[isc][gc].size() == gridRows*gridCols )
				{
					++count;
				}
				
				for( unsigned pc = 0; pc < grids[isc][gc].size(); ++pc )
				{
					bool check = 
					              grids[isc][gc][pc].row < gridRows &&
					              grids[isc][gc][pc].col < gridCols &&
					              
					              grids[isc][gc][pc].row >= 0       &&
					              grids[isc][gc][pc].col >= 0        ;
					
					if( !check )
					{
						cout << "\t grid failed check - ignoring." << endl;
						++icount;
						grids[isc][gc].clear();
						break;
					}
					
				}
			}
			cout << count << " " << icount << endl;
		}
	}
	else
	{
		cout << "Getting grids - be patient!" << endl;
		
		
		// Determine number of concurrent threads supported.
		unsigned long numThreads = std::thread::hardware_concurrency();
		cout << numThreads << " concurrent threads are supported.\n";
		cout << "sources: " << sources.size() << endl;
		
		gridProg.assign( sources.size(), 0 );
		gridFound.assign( sources.size(), 0 );
		omp_lock_t coutLock;
		omp_init_lock( &coutLock );
		#pragma omp parallel for
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			FindGridThread( sources[isc], isc, coutLock, downHints[isc] );
		}
	
	}
	
	
// 	//
// 	// Trying to find bad grids is currently a nightmare. 
// 	// So here's an alternative, showing only the grids we've accepted for use.
// 	//
// 	cout << " --- rendering all grid images for easier hunting" << endl;
// 	CommonConfig ccfg;
// 	int imgc = 0;
// 	int mxgc = 0;
// 	for( unsigned isc = 0; isc < sources.size(); ++isc )
// 	{
// 		mxgc = std::max( (size_t)mxgc, grids[isc].size() );
// 	}
// 	
// 	unsigned numRows = 1;
// 	unsigned numCols = 1;
// 	if( sources.size() > 16)
// 		throw std::runtime_error("I'm just refusing to show that many images!");
// 	else
// 	{
// 		numRows = numCols = (int)ceil( sqrt(sources.size()) );
// 	}
// 	
// 	// find out how big a window to make
// 	cout << "sizes of first images: " << endl;
// 	float maxW, maxH;
// 	maxW = maxH = 0.0;
// 	for( unsigned isc = 0; isc < sources.size(); ++isc )
// 	{
// 		std::shared_ptr<Rendering::MeshNode> mn;
// 		cv::Mat img = sources[isc]->GetCurrent();
// 		
// 		maxW = std::max( (float)img.cols, maxW );
// 		maxH = std::max( (float)img.rows, maxH );
// 		
// 		cout << "\t" << img.cols << " " << img.rows << endl;
// 	}
// 	
// 	float winW = ccfg.maxSingleWindowWidth;
// 	float winH = winW * (maxH * numRows)/(maxW * numCols);
// 	if( winH > ccfg.maxSingleWindowHeight )
// 	{
// 		winH = ccfg.maxSingleWindowHeight;
// 		winW = winH * (maxW * numCols) / (maxH * numRows);
// 	}
// 	
// 	float rat = winH / winW;
// 	
// 	if( winW > ccfg.maxSingleWindowWidth )
// 	{
// 		winW = ccfg.maxSingleWindowWidth;
// 		winH = rat * winW / rat;
// 	}
// 	
// 	cout << "win W, H: " << winW << ", " << winH << endl;
// 	
// 	std::shared_ptr< Rendering::BasicHeadlessRenderer > gridRen;
// 	Rendering::RendererFactory::Create( gridRen, winW,winH, "valid grids" );
// 	
// 	
// 	float renW, renH;
// 	renW = numCols * maxW;
// 	renH = numRows * maxH;
// 	
// 	gridRen->Get2dBgCamera()->SetOrthoProjection(0, renW, 0, renH, -10, 10 );
// 	
// 	std::vector< std::shared_ptr<Rendering::MeshNode> > imgCards;
// 	for( unsigned isc = 0; isc < sources.size(); ++isc )
// 	{
// 		std::shared_ptr<Rendering::MeshNode> mn;
// 		cv::Mat img = sources[isc]->GetCurrent();
// 		std::stringstream ss; ss << "image-" << isc;
// 		
// 		unsigned r = isc/numCols;
// 		unsigned c = isc%numCols;
// 		
// 		float x,y;
// 		x = c * maxW;
// 		y = r * maxH;
// 		
// 		mn = Rendering::GenerateImageNode( x, y, img.cols, img, ss.str(), gridRen );
// 		imgCards.push_back(mn);
// 		gridRen->Get2dBgRoot()->AddChild( mn );
// 	}
// 	
// 	
// 	
// 	for( unsigned isc = 0; isc < sources.size(); ++isc )
// 	{
// 		sources[isc]->JumpToFrame(0);
// 	}
// 	int fc = 0;
// 	for( unsigned gc = 0; gc < mxgc; ++gc )
// 	{
// 		bool use = false;
// 		for( unsigned isc = 0; isc < sources.size(); ++isc )
// 			if( grids[isc][gc].size() == gridRows*gridCols )
// 				use = true;
// 		
// 		if( use )
// 		{
// 			for( unsigned isc = 0; isc < sources.size(); ++isc )
// 			{
// 				//sources[isc]->JumpToFrame(gc);
// 				while( sources[isc]->GetCurrentFrameID() != gc )
// 					sources[isc]->Advance();
// 				
// 				cv::Mat img = sources[isc]->GetCurrent();
// 				
// 				for( unsigned pc = 0; pc < grids[isc][gc].size(); ++pc )
// 				{
// 					float x,y;
// 					x = grids[isc][gc][pc].pi(0);
// 					y = grids[isc][gc][pc].pi(1);
// 					int row,col;
// 					row = grids[isc][gc][pc].row;
// 					col = grids[isc][gc][pc].col;
// 					
// 					float red,green,blue;
// 					red   = (std::min(row,10)/10.0f) * 255.0f;
// 					green = (std::min(col,10)/10.0f) * 255.0f;
// 					blue  = 255.0f;
// 					
// 					cv::circle( img, cv::Point(x,y), 15, cv::Scalar(blue,green,red), 4 );
// 				}
// 				
// 				imgCards[isc]->GetTexture()->UploadImage( img );
// 			}
// 			
// 			gridRen->StepEventLoop();
// 			cv::Mat grab = gridRen->Capture();
// 			std::stringstream ss;
// 			ss << "gridImgs/" << std::setw(6) << std::setfill('0') << fc << "_" << std::setw(6) << std::setfill('0') << gc << ".jpg";
// 			SaveImage( grab, ss.str() );
// 			
// 			++fc;
// 		}
// 	}
	
}


void CamNetCalibrator::WriteGridsFile( std::ofstream &outfi, vector< vector< CircleGridDetector::GridPoint > > &grids )
{
	for (unsigned gc = 0; gc < grids.size(); ++gc)
	{
		outfi << gc << " " << grids[gc].size() << endl;
		for (unsigned pc = 0; pc < grids[gc].size(); ++pc)
		{
			outfi << "\t" << grids[gc][pc].row << " "
							 << grids[gc][pc].col << " "
							 << grids[gc][pc].pi(0) << " "
							 << grids[gc][pc].pi(1) << endl;
		}
	}
}

void CamNetCalibrator::ReadGridsFile( std::ifstream &infi, vector< vector< CircleGridDetector::GridPoint > > &fgrids )
{
	while(infi)
	{
		unsigned gc;
		unsigned count;
		infi >> gc;
		infi >> count;

		vector< CircleGridDetector::GridPoint > ps;
		for( unsigned pc = 0; pc < count; ++pc )
		{
			CircleGridDetector::GridPoint p;
			infi >> p.row;
			infi >> p.col;
			infi >> p.pi(0);
			infi >> p.pi(1);
			p.pi(2) = 1.0;
			ps.push_back(p);
		}
		
		if( infi )
		{
			std::sort( ps.begin(), ps.end() );
			fgrids.push_back(ps);
		}
	}
}


void CamNetCalibrator::CalibrateIntrinsics()
{
	// useful to know the size of each source's image.
	std::vector< cv::Size > imgSizes( sources.size() );
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		imgSizes[isc] = cv::Size( sources[isc]->GetCurrent().cols, sources[isc]->GetCurrent().rows );
	}

	// construct the coordinates of the grid points, in the space of the grid.
	BuildGrid(objCorners, gridRows, gridCols, gridRSpacing, gridCSpacing );

	// Load or calculate intrinsics using visible grids.
	Ks.resize(sources.size());
	ks.resize(sources.size());
	if( cfg.exists("useExistingIntrinsics") && cfg.lookup("useExistingIntrinsics") )
	{
		cout << "loading existing intrinsics: " << endl;
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			cv::Mat K(3,3, CV_64FC1);
			const Calibration &cal = sources[isc]->GetCalibration();
			for( unsigned rc = 0; rc < 3; ++rc )
				for( unsigned cc = 0; cc < 3; ++cc )
					K.at<double>(rc,cc) = cal.K(rc,cc);
			Ks[isc] =  K;
			ks[isc] = cal.distParams;

			cout << K << endl;
			for( unsigned c = 0; c < ks[ks.size()-1].size(); ++c )
			{
				cout << ks[ks.size()-1][c] << " ";
			}
			cout << endl;
		}
	}
	else
	{
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			cout << "Calibrating source: " << imgDirs[isc] << endl;

			// find all the grids that are visible from this source/camera.
			vector< vector< cv::Point2f > > nonZeroGrids;
			vector<unsigned> nonZeroGridInds;
			for( unsigned gc = 0; gc < grids[isc].size(); ++gc )
			{
				if( grids[isc][gc].size() > 0 )
				{
					vector< cv::Point2f > p2s;
					for( unsigned pc = 0; pc < grids[isc][gc].size(); ++pc )
					{
						cv::Point2f cvp;
						cvp.x = grids[isc][gc][pc].pi(0);
						cvp.y = grids[isc][gc][pc].pi(1);
						p2s.push_back( cvp );

					}
					nonZeroGrids.push_back( p2s );
					nonZeroGridInds.push_back(gc);
				}
			}
			
			

			// decide which grids to use. Using all the grids takes too long,
			// and we can generally do well enough with a random subset of the
			// available grids - especially if we're just debugging and feeling
			// impatient.
			vector< vector< cv::Point2f > > gridsToUse;
			vector< unsigned > gridsToUseInds;
			if( maxGridsForInitial > 0 && maxGridsForInitial < nonZeroGrids.size()-1 )
			{
				// sub-sample the grids.
				std::set<int> s;
				while( s.size() < maxGridsForInitial )
				{
					s.insert( rand()%nonZeroGrids.size() );
				}
				for( auto si = s.begin(); si != s.end(); ++si )
				{
					gridsToUse.push_back( nonZeroGrids[*si] );
					gridsToUseInds.push_back( nonZeroGridInds[*si] );
				}
			}
			else
			{
				// just use all of them.
				gridsToUse = nonZeroGrids;
				for(unsigned gc = 0; gc < nonZeroGridInds.size(); ++gc )
					gridsToUseInds.push_back(nonZeroGridInds[gc]);
			}
			
			

			// To get the right correspondences, we need a set of grid points
			// in object space for every grid observation.
			vector< vector<cv::Point3f> > allObjCorners;
			for( unsigned gc = 0; gc < gridsToUseInds.size(); ++gc )
			{
				vector<cv::Point3f> usedCorners;
				for( unsigned cc = 0; cc < grids[isc][ gridsToUseInds[gc] ].size(); ++cc )
				{
					unsigned r = grids[isc][ gridsToUseInds[gc] ][cc].row;
					unsigned c = grids[isc][ gridsToUseInds[gc] ][cc].col;
					cv::Point3f cv3;
					cv3.x = c * gridCSpacing;
					cv3.y = r * gridRSpacing;
					cv3.z = 0.0f;

					usedCorners.push_back( cv3 );
				}

				allObjCorners.push_back(usedCorners);
			}

			// calibrate the camera intrinsics.
			cv::Mat K;
			std::vector<float> k;
			std::vector<cv::Mat> Rs;
			std::vector<cv::Mat> ts;
			int flags = 0;
			if( cfg.exists("noDistortionOnInitial") && cfg.lookup("noDistortionOnInitial"))
			{
				// distortion parameters are often a flaky prospect, so try calibrating initially
				// without them, then let the bundle adjustment worry about them later.
				flags = cv::CALIB_FIX_PRINCIPAL_POINT | cv::CALIB_FIX_ASPECT_RATIO | cv::CALIB_FIX_K1 | cv::CALIB_FIX_K2 | cv::CALIB_FIX_K3 | cv::CALIB_ZERO_TANGENT_DIST;
			}
			else
			{
				// let OpenCV try a few distortion parameters.
				flags = cv::CALIB_FIX_PRINCIPAL_POINT | cv::CALIB_FIX_ASPECT_RATIO | cv::CALIB_FIX_K3 | cv::CALIB_ZERO_TANGENT_DIST;
			}
			double error = cv::calibrateCamera(allObjCorners, gridsToUse, imgSizes[isc], K, k, Rs, ts, flags);
			
			if( std::isnan(error) )
			{
				throw std::runtime_error(" How the F did this happen? " );
			}

			cout << "calib error: " << error << endl;
			cout << K << endl;
			for (unsigned dc = 0; dc < k.size(); ++dc )
				cout << k[dc] << " ";
			cout << endl;

			Ks[isc] = K;
			ks[isc] = k;
		}
	}
}


void CamNetCalibrator::DetermineGridVisibility()
{
	// fill a matrix where M(a,b) shows how many grid observations
	// are shared between source/camera a and b.
	sharing = Eigen::MatrixXi::Zero(sources.size(), sources.size());
	for( unsigned gc = 0; gc < grids[0].size(); ++gc )
	{
		for( unsigned isc = 0; isc < sources.size(); ++isc )
		{
			if( grids[isc][gc].size() > 0 )
			{
				for(unsigned isc2 = 0; isc2 < sources.size(); ++isc2 )
				{
					if(grids[isc2][gc].size() > 0 )
					{
						++sharing(isc,isc2);
					}
				}
			}
		}
	}

	cout << "camera sharing:" << endl;
	cout << sharing << endl;
}

void CamNetCalibrator::SaveResults()
{
	for(unsigned isc = 0; isc < sources.size(); ++isc )
	{
		sources[isc]->SetCalibration( Ks[isc], ks[isc], Ls[isc] );
		sources[isc]->SaveCalibration();
	}
	
	// we can output the final 3D grids as well...
	std::ofstream p3df( out3DFile );
	
	
	
	std::vector< std::vector< std::vector<float> > > pgpcReprojErrors( gridFrames.size() );
	cout << "gfs: " << gridFrames.size() << endl;
	for( unsigned gc = 0; gc < gridFrames.size(); ++gc )
	{
		// the actual frame number:
		unsigned frameNo = gridFrames[gc];
		
		p3df << "frame: " << frameNo << endl;
		std::vector< std::vector<float> > pcReprojErrors( sources.size() );
		
		// go through the world points looking for points that come
		// from this grid frame.
		for( unsigned wpc = 0; wpc < worldPoints.size(); ++wpc )
		{
			
			if( pc2gc[wpc].gc == gc )
			{
				p3df << "\t" << worldPoints[wpc].transpose() << endl;
				p3df << "\tprojections:" << endl;
				
				for( unsigned cam = 0; cam < sources.size(); ++cam )
				{
					Calibration &c = sources[cam]->GetCalibration();
					hVec2D p = c.Project( worldPoints[wpc] );
					p3df << "\t\t" << cam << ": " << p.transpose() << endl;
				}
				
				
				p3df << "\tobservations:" << endl;
				for( unsigned cam = 0; cam < sources.size(); ++cam )
				{
					if( grids[cam][ gc ].size() > 0)
					{
						hVec2D o;
						o << grids[cam][ gc ][ pc2gc[wpc].pc ].pi(0),
					         grids[cam][ gc ][ pc2gc[wpc].pc ].pi(1), 1.0;
						
						p3df << "\t\t" << cam << ": " << o.transpose() << endl;
					}
				}
				
				for( unsigned cam = 0; cam < sources.size(); ++cam )
				{
					if( grids[cam][ gc ].size() > 0)
					{
						Calibration &c = sources[cam]->GetCalibration();
						hVec2D p = c.Project( worldPoints[wpc] );
						
						hVec2D o;
						o << grids[cam][ gc ][ pc2gc[wpc].pc ].pi(0),
						     grids[cam][ gc ][ pc2gc[wpc].pc ].pi(1), 1.0;
						
						float e = ( o - p ).norm();
						pcReprojErrors[cam].push_back( e );
					}
				}
				
				p3df << endl;
			}
		}
		pgpcReprojErrors[gc] = pcReprojErrors;
		p3df << endl << endl;
	}
	
	
	struct SortableGridErr
	{
		float maxOverallErr;
		int gc;
		int frameNo;
		std::map< unsigned, float > pcMinErr, pcMaxErr, pcMeanErr;
		
		bool operator<( const SortableGridErr &oth) const
		{
			return maxOverallErr < oth.maxOverallErr;
		} 
	};
	
	std::vector< SortableGridErr > gridMaxErrs;
	for( unsigned gc = 0; gc < gridFrames.size(); ++gc )
	{
		SortableGridErr gerr;
		gerr.frameNo = gridFrames[gc];
		gerr.gc = gc;
		
		std::vector<float> maxErrs;
		for( unsigned cam = 0; cam < sources.size(); ++cam )
		{
			if( pgpcReprojErrors[gc][cam].size() > 0 )
			{
				std::sort( pgpcReprojErrors[gc][cam].begin(), pgpcReprojErrors[gc][cam].end() );
				gerr.pcMinErr[cam] = pgpcReprojErrors[gc][cam][0];
				gerr.pcMaxErr[cam] = pgpcReprojErrors[gc][cam].back();
				float sum = std::accumulate( pgpcReprojErrors[gc][cam].begin(), pgpcReprojErrors[gc][cam].end(), 0 );
				gerr.pcMeanErr[cam] = sum / pgpcReprojErrors[gc][cam].size();
				
				maxErrs.push_back( gerr.pcMaxErr[cam] );
			}
		}
		if( maxErrs.size() > 0 )
		{
			std::sort( maxErrs.begin(), maxErrs.end() );
			gerr.maxOverallErr = maxErrs.back();
		}
		else
		{
			gerr.maxOverallErr = 0;
		}
		gridMaxErrs.push_back( gerr );
	}
	
	std::sort( gridMaxErrs.begin(), gridMaxErrs.end() );
	
	std::ofstream errf( outErrsFile );
	errf << "# grid frames in order of worst max reprojection error to best: " << endl;
	errf << "# each block is:" << endl;
	errf << "# <frame No> <overall Max Err>" << endl;
	errf << "#\t<cam idx> : <min> <mean> <max>" << endl;
	
	for( auto i = gridMaxErrs.rbegin(); i != gridMaxErrs.rend(); ++i )
	{
		errf << i->frameNo << " " << i->maxOverallErr << endl;
		for( auto c = i->pcMinErr.begin(); c != i->pcMinErr.end(); ++c )
		{
			errf << "\t" << c->first << " : " << c->second << " " << i->pcMeanErr[ c->first ] << " " << i->pcMaxErr[ c->first ] << endl;
		}
	}
	
}


void CamNetCalibrator::CalibrateExtrinsics()
{
	numGrids = grids[0].size();
	numCams  = grids.size();
	isSetG.assign(numGrids, false);
	isSetC.assign(numCams, false);

	Ls.assign( numCams, transMatrix3D::Identity() );
	Ms.assign( numGrids, transMatrix3D::Identity() );
	Mcams.assign( numGrids, -1);


	// decide which cameras we're going to start with.
	// when we first call PickCameras, it also sets the "root" camera,
	// which is the camera assumed to be on the origin.
	vector<unsigned> fixedCams;
	vector<unsigned> variCams;
	cout << "picking cams. Before - f: ";
	for( unsigned cc = 0; cc < fixedCams.size(); ++cc )
		cout << fixedCams[cc] << " ";
	cout << "      v: ";
	for( unsigned cc = 0; cc < variCams.size(); ++cc )
		cout << variCams[cc] << " ";
	cout << endl;
	
	PickCameras(fixedCams, variCams);
	
	cout << "picking cams. After - f: ";
	for( unsigned cc = 0; cc < fixedCams.size(); ++cc )
		cout << fixedCams[cc] << " ";
	cout << "      v: ";
	for( unsigned cc = 0; cc < variCams.size(); ++cc )
		cout << variCams[cc] << " ";
	cout << endl;

	bool done = false;
	unsigned iterCount = 0;
	while( !done )
	{
		// work out the position of the grids visible from the current camera,
		// but otherwise in unknown positions.
		InitialiseGrids(fixedCams, variCams);

		// if we have auxilliary matches, and more than one fixed camera,
		// we can estimate a 3D position for those matches.
		InitialiseAuxMatches(fixedCams, variCams);
		
		if( visualise == 2 || visualise == 3 )
		{
			cout << "=========================" << endl;
			cout << "  After Init Aux         " << endl;
			cout << "=========================" << endl;
			Visualise();
		}
		
		if( cfg.exists("useSBA") && cfg.lookup("useSBA") )
		{
			// arguably, any new points will help us adjust the known cameras.
			// so, no harm in letting everything move... maybe?
			std::vector<unsigned> noCams;
			if( fixedCams.size() > 1 )
			{
				if( numIntrinsicsToSolve > 0 )
					BundleAdjust(SBA_POINTS, fixedCams, noCams, 4, 5 );
				else
					BundleAdjust(SBA_POINTS, fixedCams, noCams, 5, 5 );

			}
		}
		
		if( visualise == 2 || visualise == 3 )
		{
			cout << "=========================" << endl;
			cout << "  After Init Aux SBA     " << endl;
			cout << "=========================" << endl;
			Visualise();
		}
		

		// estimate the position of any unset camera with good visibility of
		// existing grids.
		InitialiseCams(variCams);
		
		if( visualise == 2 || visualise == 3 )
		{
			cout << "=========================" << endl;
			cout << "  After Init Cams         " << endl;
			cout << "=========================" << endl;
			Visualise();
		}

		cout << "fixed: ";
		for(unsigned fcc = 0; fcc < fixedCams.size(); ++fcc )
			cout << fixedCams[fcc] << " ";
		cout << endl << "vari : ";
		for(unsigned fcc = 0; fcc < variCams.size(); ++fcc )
			cout << variCams[fcc] << " " ;
		cout << endl;

		unsigned numSetGrids = 0;
		for( unsigned gc = 0; gc < isSetG.size(); ++gc )
			if( isSetG[gc] )
				++numSetGrids;
		cout << "num set grids: " << numSetGrids << endl;

		if( visualise == 1 || visualise == 3 )
		{
			cout << "=========================" << endl;
			cout << "  PRE SBA adding cams   " << endl;
			cout << "=========================" << endl;
			Visualise();
		}
		


		// bundle adjust existing cameras
		// and 3D points. Keep things simple by only optimising the points and the newly
		// added cameras, and avoid changing the distortion parameters etc.
		if( cfg.exists("useSBA") && cfg.lookup("useSBA") && variCams.size() > 0 && iterCount > 0)
		{
			if( numIntrinsicsToSolve > 0 )
			{
				BundleAdjust(SBA_CAM, fixedCams, variCams, 4, 5 );
				BundleAdjust(SBA_CAM_AND_POINTS, fixedCams, variCams, 4, 5 );
			}
			else
			{
				BundleAdjust(SBA_CAM, fixedCams, variCams, 5, 5 );
				BundleAdjust(SBA_CAM_AND_POINTS, fixedCams, variCams, 5, 5 );
			}
			
			CheckAndFixScale();
		}

		if( visualise == 2 || visualise == 3 )
		{
			cout << "=========================" << endl;
			cout << "  POST SBA adding cams   " << endl;
			cout << "=========================" << endl;
			Visualise();
		}
		

		// decide which camera to add next,
		// or whether we are actually finished.
		done = PickCameras(fixedCams, variCams) && iterCount > 0;
		CalcReconError();
		
		++iterCount;
	}



	// for the final bundle adjust, we let all cameras move and give it some
	// distortion parameters. Then we give it one more round with more
	// distortions.
	if( cfg.exists("useSBA") && cfg.lookup("useSBA") )
	{
		variCams = fixedCams;
		fixedCams.clear();
		
		if( numIntrinsicsToSolve > 0 )
		{
			cout << "======================" << endl;
			cout << "Final SBA (1,0) camOnly" << endl;
			cout << "======================" << endl;
			BundleAdjust(SBA_CAM, fixedCams, variCams, 4, 5 );
			CheckAndFixScale();
			if( visualise == 2 || visualise == 3 )
				Visualise();
			
			CalcReconError();
			cout << "======================" << endl;
			cout << "Final SBA (1,0) cam and points" << endl;
			cout << "======================" << endl;
			BundleAdjust(SBA_CAM_AND_POINTS, fixedCams, variCams, 4, 5 );
			CheckAndFixScale();
			if( visualise == 2 || visualise == 3 )
				Visualise();
			

			if( numIntrinsicsToSolve > 2 )
			{
				unsigned nis = 2;
				while( nis <= numIntrinsicsToSolve )
				{
					unsigned nds = 0;
					while( nds <= numDistortionToSolve )
					{
						cout << "======================" << endl;
						cout << "Final stage SBA (" << nis << "," << nds <<") cam and points" << endl;
						cout << "======================" << endl;
						BundleAdjust(SBA_CAM_AND_POINTS, fixedCams, variCams, 5-nis, 5-nds);
						CheckAndFixScale();
						if( visualise == 2 || visualise == 3 )
							Visualise();
						
						CalcReconError();
						++nds;
					}
					++nis;
				}
			}
		}
		else if( numDistortionToSolve > 0 )
		{
			unsigned nds = 0;
			while( nds <= numDistortionToSolve )
			{
				cout << "======================" << endl;
				cout << "Final stage SBA (" << 0 << "," << nds <<") cam and points" << endl;
				cout << "======================" << endl;
				BundleAdjust(SBA_CAM_AND_POINTS, fixedCams, variCams, 5, 5-nds);
				CheckAndFixScale();
				if( visualise == 2 || visualise == 3 )
					Visualise();
				
				CalcReconError();
				++nds;
			}
		}

		CheckAndFixScale();
		
	}
	
	CalcReconError();
	cout << "Vis?: " << visualise << endl;
	if( visualise > 0 )
		Visualise();


}

float CamNetCalibrator::CalcReconError()
{
	std::vector< std::vector<float> > gridPointsErrs;
	std::vector< std::vector<float> > auxPointsErrs;
	
	std::ofstream errfi( "/tmp/mc_calib_err" );
	
	gridPointsErrs.resize( numCams );
	for( unsigned wpc = 0; wpc < worldPoints.size(); ++wpc )
	{
		// which grid did it come from?
		unsigned gc = pc2gc[wpc].gc;
		unsigned ipc = pc2gc[wpc].pc;
		
		if( isSetG[gc] )
		{
			for( unsigned cc = 0; cc < numCams; ++cc )
			{
				if( isSetC[cc] )
				{
					Calibration c;
					c.width  = sources[cc]->GetCurrent().cols;
					c.height = sources[cc]->GetCurrent().rows;
					c.L = Ls[cc];
					for( unsigned x = 0; x < 3; ++x )
						for( unsigned y = 0; y < 3; ++y )
							c.K(y,x) = Ks[cc].at<double>(y,x);
					c.distParams = ks[cc];
					
					if( grids[cc][gc].size() > 0 )
					{
						hVec3D p3d = worldPoints[wpc];
						hVec2D p2d;
						p2d << grids[cc][gc][ipc].pi(0), grids[cc][gc][ipc].pi(1), 1.0;
						
						hVec2D r2d;
						r2d = c.Project(p3d);
						
						hVec2D d;
						d = r2d - p2d;
						
						gridPointsErrs[cc].push_back(d.norm());
					}
				}
			}
		}
	}
	
	auxPointsErrs.resize( numCams );
	for( unsigned amc = 0; amc < auxMatches.size(); ++amc )
	{
		PointMatch &m = auxMatches[amc];
		
		if( m.has3D )
		{
			hVec3D p3d = m.p3D;
			
			// what views is the point in?
			for( unsigned cc = 0; cc < numCams; ++cc )
			{
				Calibration c;
				c.width  = sources[cc]->GetCurrent().cols;
				c.height = sources[cc]->GetCurrent().rows;
				c.L = Ls[cc];
				for( unsigned x = 0; x < 3; ++x )
					for( unsigned y = 0; y < 3; ++y )
						c.K(y,x) = Ks[cc].at<double>(y,x);
				c.distParams = ks[cc];
					
				if( m.p2D.find( cc ) != m.p2D.end()  &&  m.proj2D.find( cc ) != m.proj2D.end() )
				{
					hVec2D ev = m.p2D[cc] - m.proj2D[cc];
					auxPointsErrs[cc].push_back( ev.norm() );
				}
				else if( m.p2D.find( cc ) != m.p2D.end() )
				{
					hVec2D proj = c.Project(p3d);
					hVec2D ev = m.p2D[cc] - proj;
					auxPointsErrs[cc].push_back( ev.norm() );
				}
			}
		}
	}
	
	
	cout << endl << endl;
	cout << "====================================================================================================" << endl;
	cout << "======================     Current reconstruction errors                  ==========================" << endl;
	cout << "====================================================================================================" << endl;
	cout << endl << endl;
	
	
	errfi << endl << endl;
	errfi << "====================================================================================================" << endl;
	errfi << "======================     Current reconstruction errors                  ==========================" << endl;
	errfi << "====================================================================================================" << endl;
	errfi << endl << endl;
	
	
	cout << " == grid points == " << endl;
	errfi << " == grid points == " << endl;
	float grandMeanGrids = 0.0f;
	float grandMeanAux   = 0.0f;
	int grandCountGrids = 0;
	int grandCountAux   = 0;
	
	cout  << std::setw(6) << "  cam  " << "|" << std::setw(20) << "means" << " | " << std::setw(20) << "mins" << "|" << std::setw(20) << "maxs" << "|" << std::setw(20) << "meds" << endl;
	errfi << std::setw(6) << "  cam  " << "|" << std::setw(20) << "means" << " | " << std::setw(20) << "mins" << "|" << std::setw(20) << "maxs" << "|" << std::setw(20) << "meds" << endl;
	for( unsigned cc = 0; cc < numCams; cc++ )
	{
		if( isSetC[cc] )
		{
			std::sort( gridPointsErrs[cc].begin(), gridPointsErrs[cc].end() );
			float meanGrids = 0.0f;
			for( unsigned ec = 0; ec < gridPointsErrs[cc].size(); ++ec )
				meanGrids += gridPointsErrs[cc][ec];
			meanGrids /= gridPointsErrs[cc].size();
			
			
			std::sort( auxPointsErrs[cc].begin(), auxPointsErrs[cc].end() );
			float meanAux = 0.0f;
			for( unsigned ec = 0; ec < auxPointsErrs[cc].size(); ++ec )
				meanAux += auxPointsErrs[cc][ec];
			meanAux /= auxPointsErrs[cc].size();
			
			
			
			
			
			
			if( gridPointsErrs[cc].size() > 0 && auxPointsErrs[cc].size() > 0 )
			{
				cout  << std::setw(6)  << cc << "|" 
				      << std::setw(10) << meanGrids << " " << std::setw(9) << meanAux << " | "
				      << std::setw(10) << gridPointsErrs[cc][0] << " " << std::setw(9) << auxPointsErrs[cc][0] << " | "
				      << std::setw(10) << gridPointsErrs[cc].back() << " " << std::setw(9) << auxPointsErrs[cc].back() << " | "
				      << std::setw(10) << gridPointsErrs[cc][ gridPointsErrs[cc].size()/2 ] << " " << std::setw(9) << auxPointsErrs[cc][ auxPointsErrs[cc].size()/2 ] << endl;
				
				errfi << std::setw(6)  << cc << "|" 
				      << std::setw(10) << meanGrids << " " << std::setw(9) << meanAux << " | "
				      << std::setw(10) << gridPointsErrs[cc][0] << " " << std::setw(9) << auxPointsErrs[cc][0] << " | "
				      << std::setw(10) << gridPointsErrs[cc].back() << " " << std::setw(9) << auxPointsErrs[cc].back() << " | "
				      << std::setw(10) << gridPointsErrs[cc][ gridPointsErrs[cc].size()/2 ] << " " << std::setw(9) << auxPointsErrs[cc][ auxPointsErrs[cc].size()/2 ] << endl;
				
				grandMeanGrids += meanGrids;
				grandMeanAux += meanAux;
				++grandCountAux;
				++grandCountGrids;
				
			}
			else if( gridPointsErrs[cc].size() > 0 && auxPointsErrs[cc].size() == 0 )
			{
				cout  << std::setw(6)  << cc << "|" 
				<< std::setw(10) << meanGrids << " " << std::setw(9) << "n/aux" << " | "
				<< std::setw(10) << gridPointsErrs[cc][0] << " " << std::setw(9) << "n/aux" << " | "
				<< std::setw(10) << gridPointsErrs[cc].back() << " " << std::setw(9) << "n/aux" << " | "
				<< std::setw(10) << gridPointsErrs[cc][ gridPointsErrs[cc].size()/2 ] << " " << std::setw(9) << "n/aux" << endl;
				
				errfi << std::setw(6)  << cc << "|" 
				<< std::setw(10) << meanGrids << " " << std::setw(9) << "n/aux" << " | "
				<< std::setw(10) << gridPointsErrs[cc][0] << " " << std::setw(9) << "n/aux" << " | "
				<< std::setw(10) << gridPointsErrs[cc].back() << " " << std::setw(9) << "n/aux" << " | "
				<< std::setw(10) << gridPointsErrs[cc][ gridPointsErrs[cc].size()/2 ] << " " << std::setw(9) << "n/aux" << endl;
				
				grandMeanGrids += meanGrids;
				++grandCountGrids;
			}
			else if( gridPointsErrs[cc].size() == 0 && auxPointsErrs[cc].size() > 0 )
			{
				cout  << std::setw(6)  << cc << "|" 
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << meanAux << " | "
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << auxPointsErrs[cc][0] << " | "
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << auxPointsErrs[cc].back() << " | "
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << auxPointsErrs[cc][ auxPointsErrs[cc].size()/2 ] << endl;
				
				errfi << std::setw(6)  << cc << "|" 
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << meanAux << " | "
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << auxPointsErrs[cc][0] << " | "
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << auxPointsErrs[cc].back() << " | "
				      << std::setw(10) << "n/grid" << " " << std::setw(9) << auxPointsErrs[cc][ auxPointsErrs[cc].size()/2 ] << endl;
				
				
				grandMeanAux += meanAux;
				++grandCountAux;
			}
			else
			{
				cout << cc << " no errs for cam? " << endl;
				errfi << cc << " no errs for cam? " << endl;
			}
			
		}
		else
		{
			cout << "camera " << cc << "     not-set    " << endl;
			errfi << "camera " << cc << "     not-set    " << endl;
		}
	}
	cout << "grand-means: " << std::setw(10) << grandMeanGrids/grandCountGrids << " " << std::setw(10) << grandMeanAux/grandCountAux << endl;
	cout << endl << endl << endl << endl;
	
	errfi << "grand-means: " << std::setw(10) << grandMeanGrids/grandCountGrids << " " << std::setw(10) << grandMeanAux/grandCountAux << endl;
	errfi << endl << endl << endl << endl;
	
	
	errfi.close();
	
	return 0.0f;
}

void CamNetCalibrator::DebugGrid(unsigned cam, unsigned grid, vector<hVec2D> &obs, vector<hVec3D> &p3d)
{
// 	if( dren == NULL )
// 	{
// 		float ar = sources[cam]->GetCurrent().rows / (float) sources[cam]->GetCurrent().cols;
// 		dren = new Renderer( 1280, 1280*ar, "cam net calib debug visualisation" );
// 
// 		dren->Set2DCamera(0,sources[cam]->GetCurrent().cols, sources[cam]->GetCurrent().rows, 0);
// 	}
// 
// 	sources[cam]->JumpToFrame( gridFrames[grid] );
// 
// 
// 	Texture tex = dren->GenTexture();
// 	cv::Mat i = sources[cam]->GetCurrent();
// 	dren->UploadImage( i, tex );
// 	Calibration c;
// 	c.width  = sources[cam]->GetCurrent().cols;
// 	c.height = sources[cam]->GetCurrent().rows;
// 	c.L = Ls[cam];
// 	for( unsigned x = 0; x < 3; ++x )
// 		for( unsigned y = 0; y < 3; ++y )
// 			c.K(y,x) = Ks[cam].at<double>(y,x);
// 	c.distParams = ks[cam];
// 
// 	vector< hVec2D > projections;
// 	for( unsigned pc = 0; pc < p3d.size(); ++pc )
// 	{
// 		hVec2D i = c.Project( p3d[pc] );
// 		projections.push_back(i);
// 	}
// 
// 	bool done = false;
// 	while( !done )
// 	{
// 		hVec2D tl; tl << 0,0,1;
// 		dren->RenderImage2D(tl, c.width, tex );
// 		dren->RenderCircles2D( projections, 10, 1,0,1 );
// 		dren->RenderCircles2D( obs, 14, 0,1,1 );
// 		// for( unsigned oc = 0; oc < observations.size(); ++oc )
// 		// {
// 		// 	float r,g,b;
// 		// 	r = rowCols[oc].first / float(gridRows);
// 		// 	g = rowCols[oc].second / float(gridCols);
// 		// 	b = 1.0f;
// 		// 	dren->RenderCircle2D( observations[oc], 14, r,g,b);
// 		// }
// 		// dren->RenderCircle2D( gridOrigin, 14, 0,1,0 );
// 
// 		dren->Display();
// 
// 		float mx,my; bool l;
// 		done = dren->CheckMouseClick(mx,my, l);
// 	}

}


void CamNetCalibrator::Visualise()
{
	cout << "Visualise!" << endl;
	
	// useful things.
	Eigen::Vector4f magenta;
	magenta << 1.0, 0, 1.0, 1.0;
	Eigen::Vector4f cyan;
	cyan << 0.0, 1.0, 1.0, 1.0;
	Eigen::Vector4f white;
	white << 1.0, 1.0, 1.0, 1.0;
	
	Eigen::Vector4f yellow;
	yellow << 1.0, 1.0, 0.1, 1.0;
	Eigen::Vector4f orange;
	orange << 1.0, 0.5, 0.1, 1.0;
	
	
	hVec3D zero; zero << 0,0,0,1.0f;
	
	if( !dren )
	{
		float ar = sources[0]->GetCurrent().rows / (float) sources[0]->GetCurrent().cols;
		
		
		CommonConfig ccfg;
		int winWidth = ccfg.maxSingleWindowWidth;
		int winHeight = winWidth * ar;
		if( winHeight > ccfg.maxSingleWindowHeight )
		{
			winHeight = ccfg.maxSingleWindowHeight;
			winWidth  = winHeight/ar;
		}
		Rendering::RendererFactory::Create( dren, winWidth, winHeight, "cam net calib debug visualisation");
		
		dren->Get2dBgCamera()->SetOrthoProjection(0, sources[0]->GetCurrent().cols, 0, sources[0]->GetCurrent().rows, -10, 10);
		dren->Get2dFgCamera()->SetOrthoProjection(0, sources[0]->GetCurrent().cols, 0, sources[0]->GetCurrent().rows, -10, 10);
		
		
		// model for showing where a camera is. Simple frustum shape. 
		// We start with a cube, but then change all the vertices.
		camMesh = Rendering::GenerateWireCube(zero, 20);
		
		// we'll actually tweak the vertices slightly to more obviously show
		// which way the camera is facing.
		camMesh->vertices <<  -20, -20,  20,  20,    -50,-50, 50,  50,
		                      -20,  20,  20, -20,    -50, 50, 50, -50,
		                        0,   0,   0,   0,     50, 50, 50,  50,
		                        1,   1,   1,   1,      1,  1,  1,   1;
		camMesh->UploadToRenderer(dren);
		
		
		// nodes to show where each camera is.
		camNodes.resize( Ls.size() );
		for( unsigned cc = 0; cc < Ls.size(); ++cc )
		{
			std::stringstream ss;
			ss << "nodeForCamera_" << cc;
			Rendering::NodeFactory::Create(camNodes[cc], ss.str() );
			camNodes[cc]->SetMesh( camMesh );
			
			if( isSetC[cc] )
				camNodes[cc]->SetBaseColour(cyan);
			else
				camNodes[cc]->SetBaseColour(magenta);
			
			camNodes[cc]->SetTexture( dren->GetBlankTexture() );
			camNodes[cc]->SetTransformation( Ls[cc].inverse() );
			camNodes[cc]->SetShader( dren->GetShaderProg("basicColourShader"));
			
			dren->Get3dRoot()->AddChild( camNodes[cc] );
		}
	}
	
	for( unsigned cc = 0; cc < Ls.size(); ++cc )
	{
		camNodes[cc]->SetTransformation( Ls[cc].inverse() );
		
		if( isSetC[cc] )
			camNodes[cc]->SetBaseColour(cyan);
	}
	
	
	// find the first frame where there is a set grid.
	int gc = 0;
	while( gc < isSetG.size() && !isSetG[gc]  )
	{
		++gc;
	}
	
	if( gc == isSetG.size() )
		gc = -1;
	
	// now make sure all image sources are at the appropriate frame.
	int viewFrame = 0;
	if( gridFrames.size() > 0 )
		viewFrame = gridFrames[gc]; 
	for( unsigned isc = 0; isc < sources.size(); ++isc )
	{
		sources[isc]->JumpToFrame( viewFrame );
	}
	
	// decide which camera we're viewing
	unsigned camID = 0;
	bool camChanged = true;
	
	// set the background image on the renderer.
	dren->SetBGImage( sources[ camID ]->GetCurrent() );
	
	
	if( isSetG.size() > 0 )
	{
		cout << "grid set?:" << isSetG[gc] << endl;
		for( unsigned cc = 0; cc < numCams; ++cc )
			cout << grids[cc][gc].size() << " ";
	}
	else
		cout << "grid set?: no grids to set." << endl;
	cout << "cams set?:" << endl;
	hVec3D o; o << 0,0,0,1;
	for( unsigned cc = 0; cc < numCams; ++cc )
	{
		cout << isSetC[cc] << " ";
		if( isSetC[cc] )
		{
			cout << " | " << (Ls[cc].inverse() * o).transpose() << endl;
		}
		else
		{
			cout << " | centre pending init " << endl;
		}
	}
	cout << endl;
	
	cout << "cam intrinsics: " << endl;
	for( unsigned cc = 0; cc < numCams; ++cc )
	{
		cout << cc << endl;
		cout << Ks[cc] << endl << "-" << endl;
		
	}
	cout << "frame: " << viewFrame << endl;
	
	
	
	bool done = false;
	while( !done )
	{
		if( camChanged )
		{
			// now make sure all image sources are at the appropriate frame.
			sources[camID]->JumpToFrame( viewFrame );
			
			// get camera's bg image
			cv::Mat img = sources[ camID ]->GetCurrent().clone();
			
			// set image as current background.
			dren->SetBGImage( img );
			
			// set camera's calibration for current viewpoint.
			Calibration c;
			c.width  = sources[camID]->GetCurrent().cols;
			c.height = sources[camID]->GetCurrent().rows;
			c.L = Ls[camID];
			for( unsigned x = 0; x < 3; ++x )
				for( unsigned y = 0; y < 3; ++y )
					c.K(y,x) = Ks[camID].at<double>(y,x);
			c.distParams = ks[camID];
			
			dren->Get3dCamera()->SetFromCalibration( c, 100, 20000 );
			
			
			
			dren->Get2dFgRoot()->Clean();
			
			// grid circles...
			for( unsigned wpc = 0; wpc < worldPoints.size(); ++wpc )
			{
				if( pc2gc[wpc].gc == gc )
				{
					// draw the projection of the point.
					hVec2D p;
					p = c.Project( worldPoints[wpc] );
					
					std::shared_ptr<Rendering::MeshNode> pnode;
					std::stringstream id; id << "proj_" << wpc;
					pnode = GenerateCircleNode2D( p, 6, 3, id.str(), dren );
					pnode->SetBaseColour( magenta );
					dren->Get2dFgRoot()->AddChild( pnode );
					
					// do we have an observation of this point?
					if( grids[camID][ gc ].size() > 0)
					{
						hVec2D o;
						o << grids[camID][ gc ][ pc2gc[wpc].pc ].pi(0),
						     grids[camID][ gc ][ pc2gc[wpc].pc ].pi(1), 
						     1.0;
						
						std::shared_ptr<Rendering::MeshNode> onode;
						std::stringstream id; id << "obs_" << wpc;
						onode = GenerateCircleNode2D( o, 8, 4, id.str(), dren );
						onode->SetBaseColour( cyan );
						dren->Get2dFgRoot()->AddChild( onode );
						
						if( grids[camID][ gc ][ pc2gc[wpc].pc ].row == 0 && grids[camID][ gc ][ pc2gc[wpc].pc ].col == 0 )
						{
							onode->SetBaseColour( white );
						}
						
						// do we want to also draw a line from projection to observation?
					}
				}
			}
			
			// auxMatches
			for( unsigned mc = 0; mc < auxMatches.size(); ++mc )
			{
				PointMatch &m = auxMatches[mc];
				
				// is it visible in this camera?
				auto obs = m.p2D.find(camID);
				if( obs == m.p2D.end() )
					continue;
				
				auto o = obs->second;
				
				std::shared_ptr<Rendering::MeshNode> onode;
				std::stringstream id; id << "auxObs_" << mc;
				onode = GenerateCircleNode2D( o, 6, 3, id.str(), dren );
				dren->Get2dFgRoot()->AddChild( onode );
				
				if( !m.has3D )
				{
					onode->SetBaseColour( orange );
				}
				else
				{
					onode->SetBaseColour( yellow );
					auto p = c.Project( m.p3D );
					
					std::shared_ptr<Rendering::MeshNode> pnode;
					std::stringstream id; id << "auxProj_" << mc;
					pnode = GenerateCircleNode2D( p, 6, 3, id.str(), dren );
					dren->Get2dFgRoot()->AddChild( pnode );
					
					pnode->SetBaseColour( white );
				}
			}
			camChanged = false;
		}
		
		int cChange = 0;
		int gChange = 0;
		done = dren->Step( cChange, gChange );
		
		if( cChange != 0 )
		{
			if( (int)camID + cChange >= 0 && (int)camID + cChange < Ls.size() )
			{
				camID = camID + cChange;
				camChanged = true;
			}
		}
		if( gChange != 0 )
		{
			if( (int)gc + gChange >= 0 && (int)gc + gChange < gridFrames.size() )
			{
				gc = gc + gChange;
				camChanged = true;
			}
		}
	}

}


// given a camera which has a known location,
// work out the position of any grids visible
// from that camera (we know the grid size).
void CamNetCalibrator::InitialiseGrids(std::vector<unsigned> &fixedCams, std::vector<unsigned> &variCams)
{
	for( unsigned gc = 0; gc < numGrids; ++gc )
	{
		if( isSetG[gc] )
			continue;	// no need to do this grid, it is already done!

		// to set this grid's position, it must be visible in one
		// fixed camera, or the root camera.
		int curCam = -1;
        bool rootChecked = false;
        unsigned numFixed = 0;
		for( unsigned fcc = 0; fcc < fixedCams.size(); ++fcc )
		{
            if( fixedCams[fcc] == rootCam )
                rootChecked = true;
			if( grids[ fixedCams[fcc] ][gc].size() > 0 )
			{
                if( curCam < 0 )
                    curCam = fixedCams[fcc];
                ++numFixed;
			}
		}

        if( rootChecked == false )
        {
            if( grids[ rootCam ][gc].size() > 0 )
            {
                // TODO: decide which camera will give the best estimate for the grid.
                // prefer rootCam for now if it can be used.
                curCam = rootCam;
            }
        }

		if( curCam < 0)
			continue;	// we can't do this grid.

		unsigned visible = 0;
		for( unsigned vcc = 0; vcc < variCams.size(); ++vcc )
			if( grids[ variCams[vcc] ][gc].size() > 0 )
				++visible;

		// I only want points generated for the grid if it is visible in
		// more than one camera however.
		if( visible >= 1 || numFixed > 1 )
		{

			vector<cv::Point3f> p3D;
			vector<cv::Point2f> p2D;
			for( unsigned cc = 0; cc < grids[curCam][gc].size(); ++cc )
			{
				unsigned r = grids[curCam][gc][cc].row;
				unsigned c = grids[curCam][gc][cc].col;
				cv::Point3f cv3;
				cv3.x = c * gridCSpacing;
				cv3.y = r * gridRSpacing;
				cv3.z = 0.0f;
				p3D.push_back( cv3 );

				cv::Point2f cv2;
				cv2.x = grids[curCam][gc][cc].pi(0);
				cv2.y = grids[curCam][gc][cc].pi(1);
				p2D.push_back( cv2 );
			}

			cv::Mat rvec,t;
			cv::solvePnP(p3D, p2D, Ks[curCam], ks[curCam], rvec, t);

			// rvec and tvec bring "from model to camera",
			// presumably as:
			// p' = Rp + t
			// I want R from rvec...
			cv::Mat R;
			cv::Rodrigues(rvec, R);

			transMatrix3D M;
			M << R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), t.at<double>(0,0),
			     R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), t.at<double>(1,0),
			     R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), t.at<double>(2,0),
			     0,0,0,1;

			// M takes a point from model to camera.
			// meanwhile, L takes a point from world to camera
			// so I need to go model to camera to world...
			Ms[gc] = Ls[curCam].inverse() * M;


			// so the 3D grid points which are in p3D local to the grid origin,
			// can now be moved to be relative to the world...
			vector< hVec3D > grid3D;
			for( unsigned pc = 0; pc < p3D.size(); ++pc )
			{
				// hVec3D po;
				// po << objCorners[pc].x, objCorners[pc].y, objCorners[pc].z, 1.0;

				hVec3D po;
				po << p3D[pc].x, p3D[pc].y, p3D[pc].z, 1.0f;

				grid3D.push_back( Ms[gc] * po );
			}

			// how well did OpenCV do?
			Calibration c;
			c.width  = sources[curCam]->GetCurrent().cols;
			c.height = sources[curCam]->GetCurrent().rows;
			c.L = Ls[curCam];
			for( unsigned x = 0; x < 3; ++x )
				for( unsigned y = 0; y < 3; ++y )
					c.K(y,x) = Ks[curCam].at<double>(y,x);
			c.distParams = ks[curCam];

			float mnerr = 0;
			float mxerr = 0;
			vector< hVec2D > obs;
			for( unsigned pc = 0; pc < grid3D.size(); ++pc )
			{
				auto i = c.Project( grid3D[pc] );

				hVec2D o; o << grids[curCam][gc][pc].pi(0), grids[curCam][gc][pc].pi(1), 1.0;
				auto d = i-o;
				mnerr += d.norm();
				mxerr = std::max( d.norm(), mxerr );

				obs.push_back(o);
			}
			mnerr /= grid3D.size();

			if( mxerr > 20.0f )
			{
				cout << curCam << " gave a very poor estimate of grid " << gc << endl;
				cout << "Max  err: " << mxerr << endl;
				cout << "Mean err: " << mnerr << endl;
				continue;
			}
			else if( mnerr > 10.0f )
			{
				// not good enough...
				cout << curCam << " gave a poor estimate of grid " << gc << endl;
				cout << "Max  err: " << mxerr << endl;
				cout << "Mean err: " << mnerr << endl;
				continue;
			}
			// else
			// {
			// 	cout << curCam << " gave a decent estimate of grid " << gc << endl;
			// 	cout << "Max  err: " << mxerr << endl;
			// 	cout << "Mean err: " << mnerr << endl;
			// }
			// DebugGrid(curCam, gc, obs, grid3D);



			for( unsigned pc = 0; pc < grid3D.size(); ++pc )
			{
				worldPoints.push_back( grid3D[pc] );
				ptOrigin pto;
				pto.gc = gc;
				pto.pc = pc;
				pc2gc.push_back(pto);	// so we know which grid the point came from.
			}
			isSetG[gc] = true;
			Mcams[gc] = curCam;
		}
	}
}



void CamNetCalibrator::InitialiseAuxMatches(std::vector<unsigned> &fixedCams, std::vector<unsigned> &variCams)
{
	if( auxMatches.size() == 0 || fixedCams.size() < 2 )
		return;

	for( unsigned mc = 0; mc < auxMatches.size(); ++mc )
	{
		PointMatch &m = auxMatches[mc];

		if(m.has3D)
			continue;

		// for each of the fixedCams this point is visible from,
		// back-project to get a set of rays in 3D.
		std::vector< hVec3D > rayDirs;
		std::vector< hVec3D > rayStarts;

		for( unsigned fc = 0; fc < fixedCams.size(); ++fc )
		{
			unsigned cc = fixedCams[fc];
			
			if( m.p2D.find(cc) == m.p2D.end() )
				continue;
			
			Calibration c;
			c.width  = sources[cc]->GetCurrent().cols;
			c.height = sources[cc]->GetCurrent().rows;
			c.L = Ls[cc];
			for( unsigned x = 0; x < 3; ++x )
				for( unsigned y = 0; y < 3; ++y )
					c.K(y,x) = Ks[cc].at<double>(y,x);
			c.distParams = ks[cc];

			hVec3D r = c.Unproject( m.p2D[cc] );
			rayDirs.push_back(r);
			rayStarts.push_back( c.GetCameraCentre() );
		}

		if( rayDirs.size() < 2 )
			continue;

		// now we need to intersect the rays to get a 3D point.
		m.p3D = IntersectRays( rayStarts, rayDirs );
		m.has3D = true;
	}

}

void CamNetCalibrator::InitialiseCams( std::vector<unsigned> &variCams )
{
	// we know that the variCams all share grids with the fixed cams or the
	// root cam.

	// for each vari cam, find the set grids and estimate the camera's
	// position relative to those grids.
	for( unsigned vcc = 0; vcc < variCams.size(); ++vcc )
	{
		// root cam is initialised to origin, and grids relative to it,
		// so it makes no sense to initialise root cam.
		if( isSetC[ variCams[vcc] ]  )
			continue;
		
		bool res = EstimateCamPos( variCams[vcc] );
		
		if( !res )
		{
			res = EstimateCamPosFromF( variCams[vcc] ); 
		}
		
		isSetC[ variCams[ vcc ] ] = res;
	}
}

bool CamNetCalibrator::EstimateCamPos(unsigned camID)
{
	// find the grids we're going to be able to use.
	vector< unsigned > gridsToUse;
	for(unsigned gc = 0; gc < grids[camID].size(); ++gc )
	{
		if( isSetG[gc] && grids[camID][gc].size() > 0 )
		{
			gridsToUse.push_back( gc );
		}
	}

	//
	// If we've not got all that many grids available, we can choose to ignore 
	// the grids and instead use auxilliary points instead.
	//
	if( gridsToUse.size() < minGridsToInitialiseCam )
	{
		if( auxMatches.size() > 4 )
			gridsToUse.clear();
		else
			return false;
	}

	// construct vectors of the 2D and 3D points that we're going to use.
	// this first set come from the grids....
	vector< cv::Point2f > p2D;
	vector< cv::Point3f > p3D;
	for( unsigned wpc = 0; wpc < worldPoints.size(); ++wpc )
	{
		unsigned gc  = pc2gc[wpc].gc;
		unsigned ipc = pc2gc[wpc].pc;

		bool usable = false;
		for( unsigned gcuc = 0; gcuc < gridsToUse.size() && !usable; ++gcuc)
		{
			if( gc == gridsToUse[gcuc] )
				usable = true;
		}

		if( grids[camID][gc].size() > 0 )	// is this point visible in this camera?
		{
			cv::Point2f cv2;
			cv2.x = grids[camID][gc][ipc].pi(0);
			cv2.y = grids[camID][gc][ipc].pi(1);
			p2D.push_back( cv2 );

			cv::Point3f cvwp;
			hVec3D &wp = worldPoints[wpc];
			cvwp.x = wp(0);
			cvwp.y = wp(1);
			cvwp.z = wp(2);

			p3D.push_back( cvwp );
		}
	}
	
	// we can also add points from the auxMatches
	for( unsigned ac = 0; ac < auxMatches.size(); ++ac )
	{
		PointMatch &m = auxMatches[ac];
		
		if( m.has3D )
		{
			if( m.p2D.find( camID ) != m.p2D.end() )
			{
				cv::Point2f cv2;
				cv2.x = m.p2D[camID](0);
				cv2.y = m.p2D[camID](1);
				p2D.push_back( cv2 );
				
				cv::Point3f cv3;
				cv3.x = m.p3D(0);
				cv3.y = m.p3D(1);
				cv3.z = m.p3D(2);
				p3D.push_back( cv3 );
			}
		}
	}
	
	if( p3D.size() < 4 )
		return false;
	
	cv::Mat rvec,t;
	cv::solvePnPRansac(p3D, p2D, Ks[camID], ks[camID], rvec, t);

	// vector<cv::Point2f> cvp2d;
	// cv::projectPoints(p3D, rvec, t, Ks[camID], ks[camID], cvp2d );
	// if( camID != rootCam )
	// {
	// 	for( unsigned pc = 0; pc < cvp2d.size(); ++pc )
	// 	{
	// 		cout << p2D[pc].x << " " << p2D[pc].y << " " << endl;
	// 		cout << cvp2d[pc].x << " " << cvp2d[pc].y << " " << endl;
	// 		cout << endl;
	// 	}
	// }


	// rvec and tvec bring "from model to camera",
	// presumably as:
	// p' = Rp + t
	// I want R from rvec...
	cv::Mat R;
	cv::Rodrigues(rvec, R);

	// now, model in this case, is the same as rootCam space.
	// That means this transformation takes a point in rootCam space
	// and moves it into other camera space. Which is exactly what
	// I want... (remember, I use p_c = L.p_w) and here, p_w(orld) == p_root

	transMatrix3D L;
	L << R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), t.at<double>(0,0),
		 R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), t.at<double>(1,0),
		 R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), t.at<double>(2,0),
		 0,0,0,1;

	// which finishes things off nicely..
	Ls[camID] = L;

	// cout << "root cam: " << rootCam << endl;
	//
	// cout << "Li for camID: " << camID << endl;
	// cout << L.inverse() << endl;

	Calibration c;

	c.width  = sources[camID]->GetCurrent().cols;
	c.height = sources[camID]->GetCurrent().rows;

	c.L = Ls[camID];
	for( unsigned x = 0; x < 3; ++x )
		for( unsigned y = 0; y < 3; ++y )
			c.K(y,x) = Ks[camID].at<double>(y,x);
	c.distParams = ks[camID];

	// if( camID != rootCam )
	// {
	// 	for( unsigned wpc = 0; wpc < worldPoints.size(); ++wpc )
	// 	{
	// 		unsigned gc  = pc2gc[wpc].gc;
	// 		unsigned ipc = pc2gc[wpc].pc;
	//
	// 		if( grids[camID][gc].size() > 0 )	// is this point visible in this camera?
	// 		{
	// 			hVec2D &obs = grids[camID][gc][ipc].pi;
	//
	// 			hVec3D &wp = worldPoints[wpc];
	//
	// 			hVec2D proj = c.Project(wp);
	//
	// 			// cout << obs.transpose() << endl;
	// 			// cout << proj.transpose() << endl;
	// 			// cout << endl;
	// 		}
	// 	}
	//
	// 	exit(1);
	// }

	return true;

}

#include "math/distances.h"
int PickM( std::vector< transMatrix3D > Ms, cv::Mat othK, cv::Mat camK, std::vector< cv::Point2d > othPoints, std::vector< cv::Point2d > camPoints )
{
	std::vector< bool > allInfront(4, true);
	std::vector< int  > numInfront(4, 0 );
	unsigned solutionToUse = 0;
	for( unsigned sc = 0; sc < Ms.size(); ++sc )
	{
		cout << "------------------------" << endl;
		Calibration c,o;
		c.width  = 1000; // doesn't matter
		c.height = 1000; // doesn't matter
		c.L = Ms[sc].inverse();                 // deffo!
		for( unsigned x = 0; x < 3; ++x )
			for( unsigned y = 0; y < 3; ++y )
				c.K(y,x) = camK.at<double>(y,x);
		
		
		o.width  = 1000;
		o.height = 1000;
		o.L = transMatrix3D::Identity();
		for( unsigned x = 0; x < 3; ++x )
			for( unsigned y = 0; y < 3; ++y )
				o.K(y,x) = othK.at<double>(y,x);
		
		std::vector< hVec3D > centres(2);
		centres[0] = c.GetCameraCentre();    // should be t of the solution.
		centres[1] = o.GetCameraCentre();    // should be origin.
		for( unsigned pc = 0; pc < camPoints.size(); ++pc )
		{
			cout << "  *--* " << endl;
			std::vector< hVec3D > rays(2);
			hVec2D cp, op;
			cp << camPoints[pc].x, camPoints[pc].y, 1.0;
			op << othPoints[pc].x, othPoints[pc].y, 1.0;
			rays[0] = c.Unproject( cp );
			rays[1] = o.Unproject( op );
			
			
			
			hVec3D p3d = IntersectRays( centres, rays );
			hVec2D cpc, opc;
			cpc = c.Project( p3d );
			opc = o.Project( p3d );
			
			float dstr = DistanceBetweenRays( centres[0], rays[0], centres[1], rays[1] );
			float dst0 = PointRayDistance3D( p3d, centres[0], rays[0] );
			float dst1 = PointRayDistance3D( p3d, centres[1], rays[1] );
			cout << p3d.transpose() << " : " << dst0 << " | " << dst1 << " || " << (cpc - cp).norm() << " <> " << (opc - op).norm() << endl;
			if( p3d(2) > 0 )
			{
				// so we're infront of the other camera, but what about this camera?
				p3d = c.TransformToCamera( p3d );
				if( p3d(2) > 0 )
				{
					++numInfront[sc];
				}
				else
					allInfront[sc] = false;
			}
			else
				allInfront[sc] = false;
		}
		cout << sc << " : " << numInfront[sc] << endl;
		if( (allInfront[sc] && numInfront[sc] >= numInfront[ solutionToUse ]) ||
			(!allInfront[sc] && numInfront[sc] >= numInfront[ solutionToUse] && !allInfront[ solutionToUse ] ) )
		{
			solutionToUse = sc;
		}
	}
	exit(0);
	return solutionToUse;
}


bool CamNetCalibrator::EstimateCamPosFromF(unsigned camID)
{
	// if there are not that many grids on view, or whatever other reason we can think of,
	// we might find it better to try and initialise this camera from epipolar geometry.
	// This, of course, is only useful if we have a set of auxilliary matches
	
	// first off, we need to find all the points between this camera and a suitable
	// second camera.
	// so parse the auxMatches and find out what is shared with each camera.
	std::vector<unsigned> sharedAux( Ls.size(), 0 );
	for( unsigned ac = 0; ac < auxMatches.size(); ++ac )
	{
		PointMatch &m = auxMatches[ac];
		for( auto i = m.p2D.begin(); i != m.p2D.end(); ++i )
		{
			++sharedAux[ i->first ];
		}
	}
	
	// pick the sorted camera with the best share.
	unsigned othCam = rootCam;
	for( unsigned cc = 0; cc < Ls.size(); ++cc )
	{
		if( isSetC[cc] && sharedAux[cc] > sharedAux[othCam] && cc != camID )
			othCam = cc;
	}
	
	// right then, now we need 2D points for each camera in a way
	// that lets us calculate F using OpenCV
	std::vector< cv::Point2d > camPoints, othPoints;
	std::vector< unsigned > usedAux;
	for( unsigned ac = 0; ac < auxMatches.size(); ++ac )
	{
		PointMatch &m = auxMatches[ac];
		
		auto i0 = m.p2D.find( camID );
		auto i1 = m.p2D.find( othCam );
		
		if( i0 != m.p2D.end() && i1 != m.p2D.end() )
		{
			cv::Point camPoint;
			camPoint.x = i0->second(0);
			camPoint.y = i0->second(1);
			
			cv::Point othPoint;
			othPoint.x = i1->second(0);
			othPoint.y = i1->second(1);
			
			camPoints.push_back(camPoint);
			othPoints.push_back(othPoint);
			usedAux.push_back( ac );
		}
	}
	
	if( camPoints.size() > 8 && othPoints.size() == camPoints.size() )
	{
		cv::Mat E, R, t;
		cout << camPoints.size() << " " << othPoints.size() << endl;
		cout << Ks[othCam].type() << " " << Ks[camID].type() << endl;
		cv::Mat ko( ks[othCam].size(), 1, CV_64F );
		cv::Mat kc( ks[othCam].size(), 1, CV_64F );
		for( unsigned c = 0; c < ks[othCam].size(); ++c )
		{
			ko.at<double>(c,0) = ks[othCam][c];
			kc.at<double>(c,0) = ks[camID][c];
		}
		cv::recoverPose( othPoints, camPoints, Ks[othCam], ko, Ks[camID], kc, E, R, t, cv::RANSAC, 0.99999999, 1.0 );
		
		transMatrix3D M;
		M << R.at<double>(0,0), R.at<double>(0,1), R.at<double>(0,2), t.at<double>(0,0),
		     R.at<double>(1,0), R.at<double>(1,1), R.at<double>(1,2), t.at<double>(1,0),
		     R.at<double>(2,0), R.at<double>(2,1), R.at<double>(2,2), t.at<double>(2,0),
		     0,0,0,1;
		
		
		
		// so in theory we can now position camID relative to othCam using the chosen R and t.
		Ls[camID] = M * Ls[ othCam ];
		
		
		// then we have an option. If any of the auxMatches we've been using have 3D points
		// associated to them, we can reconstruct that point from the new camera
		
		// useful to have calibration classes for each camera.
		Calibration c,o;
		
		c.width  = sources[camID]->GetCurrent().cols;
		c.height = sources[camID]->GetCurrent().rows;
		c.L = Ls[camID];
		for( unsigned x = 0; x < 3; ++x )
			for( unsigned y = 0; y < 3; ++y )
				c.K(y,x) = Ks[camID].at<double>(y,x);
		c.distParams = ks[camID];
		
		o.width  = sources[othCam]->GetCurrent().cols;
		o.height = sources[othCam]->GetCurrent().rows;
		o.L = Ls[othCam];
		for( unsigned x = 0; x < 3; ++x )
			for( unsigned y = 0; y < 3; ++y )
				o.K(y,x) = Ks[othCam].at<double>(y,x);
		o.distParams = ks[othCam];
		
		// so, now we need a point.
		// let's try a grid point first.
		bool shared = false;
		unsigned gc = 0;
		while( gc < grids[camID].size() && !shared )
		{
			if( isSetG[gc] && grids[camID][gc].size() > 0 && grids[othCam][gc].size() > 0 )
			{
				shared = true;
			}
			else
			{
				++gc;
			}
		}
		
		std::vector< hVec2D > ps(2);
		std::vector< hVec3D > cs(2);
		std::vector< hVec3D > rs(2);
		if( shared )
		{
			ps[0] = grids[camID][gc][0].pi;
			ps[1] = grids[othCam][gc][0].pi;
			
			rs[0] = c.Unproject( ps[0] );
			rs[1] = o.Unproject( ps[1] );
			
			cs[0] = c.GetCameraCentre();
			cs[1] = o.GetCameraCentre();
			
			hVec3D guess3D = IntersectRays( cs, rs );
			
			// but where is the point on the grid at proper scale?
			int wpc = 0;
			bool found = false;
			while (wpc < pc2gc.size() && !found )
			{
				if( pc2gc[ wpc ].gc == gc && pc2gc[ wpc ].pc == 0 )
					found = true;
				else
					++wpc;
			}
			if( !found )
			{
				throw std::runtime_error("I'm not sure this should happen. Grid point not in worldpoints?" );
			}
			hVec3D real3D  = worldPoints[ wpc ];
			
			float guessD = (guess3D - cs[1]).norm();
			float realD  = (real3D - cs[1]).norm();
			float scale = realD / guessD;
			
			cout << "scaling: " << guessD << " " << realD << " " << scale << endl;
			
			Ls[camID](0,3) *= scale;
			Ls[camID](1,3) *= scale;
			Ls[camID](2,3) *= scale;
			
			// because obviously that will work...
			cout << camID << " L from F...: " << endl;
			cout << Ls[camID] << endl;
			
			// can initialise the auxMatches too now...
			cout << "Initialising aux after L from F..." << endl;
			c.L = Ls[camID];
			hVec3D ccent = c.GetCameraCentre();
			hVec3D ocent = o.GetCameraCentre();
			for( unsigned uac = 0; uac < usedAux.size(); ++uac )
			{
				PointMatch &aux = auxMatches[ usedAux[uac] ];
				
				auto i0 = aux.p2D.find( camID );
				auto i1 = aux.p2D.find( othCam );
			
				if( i0 != aux.p2D.end() && i1 != aux.p2D.end() )
				{
					std::vector< hVec3D > centres(2), rays(2);
					centres[0] = ccent;
					centres[1] = ocent;
					
					rays[0] = c.Unproject( i0->second );
					rays[1] = c.Unproject( i1->second );
					
					aux.p3D = IntersectRays( centres, rays );
					aux.has3D = true;
					
					cout << uac << " => " << aux.p3D.transpose() << endl;
				}
			}
			
			return true;
		}
		else
		{
			return false;
// 			throw std::runtime_error( "I've not done this yet.");
			
		}
	}
	else
	{
		// not enough point matches for getting F.
		return false;
	}
}

cv::Mat CamNetCalibrator::GetEssentialFromF( cv::Mat F, cv::Mat &K1, cv::Mat &K2 )
{
	// in theory, it is as simple as this...
	cv::Mat E = K2.t() * F * K1;
	
	// in practice, we need to make sure the E we get back meets the requirements
	// of an E matrix - i.e. that it has two equal singular values.
	cv::SVD SVD;
	cv::Mat w, U, VT;
	SVD.compute( E, w, U, VT );
	
	// the docs don't tell me if W is a matrix or vector of singular values.
	// I'll assume a vector? But what fucking type? I hate OpenCV!!!
	// yup, vectors, and all are doubles.
	cout << "types: " << E.type() << " " << w.type() << " " << w.rows << " " << w.cols << " " << endl;
	
	float s =  0.5 * (w.at<double>(0,0) + w.at<double>(1,0) );
	cv::Mat S(3,3, CV_64FC1, cv::Scalar(0) );
	S.at<double>(0,0) = S.at<double>(1,1) = s;
	
	cv::Mat Enew = U * S * VT;
	
	return Enew;
}

void CamNetCalibrator::DecomposeE_Internal( cv::Mat &E, std::vector<cv::Mat> &Rs, std::vector<cv::Mat> &ts)
{
	// start with SVD of E
	cv::SVD SVD;
	cv::Mat w, U, VT;
	SVD.compute( E, w, U, VT );
	
	// note that in previous work, I needed to avoid improper rotations by trying various
	// fixes on E. We're not doing that here...
	cv::Mat W(3,3, CV_64FC1, cv::Scalar(0) );
	cv::Mat Z(3,3, CV_64FC1, cv::Scalar(0) );
	W.at<double>(0,1) = -1.0;
	W.at<double>(1,0) =  1.0;
	W.at<double>(2,2) =  1.0;
	Z.at<double>(0,1) =  1.0;
	Z.at<double>(1,0) = -1.0;
	
	std::vector<cv::Mat> Ss(2);
	ts.resize(2);
	Rs.resize(2);
	
	Ss[0] = -U * Z * U.t();
	Ss[1] =  U * Z * U.t();
	
	Rs[0] = U * W.t() * VT;
	Rs[1] = U * W     * VT;
	
	// translation comes from last row of VT (last column of V).
	ts[0] = cv::Mat(3,1,CV_64FC1);
	ts[0].at<double>(0,0) = VT.at<double>(2,0);
	ts[0].at<double>(1,0) = VT.at<double>(2,1);
	ts[0].at<double>(2,0) = VT.at<double>(2,2);
	
	ts[1] = -ts[0];
}
void CamNetCalibrator::DecomposeE( cv::Mat &E, std::vector<cv::Mat> &Rs, std::vector<cv::Mat> &ts)
{
	DecomposeE_Internal( E, Rs, ts );
	cout << " detsA: " << cv::determinant( Rs[0] ) << " " << cv::determinant( Rs[1] ) << endl;
	if( cv::determinant( Rs[0] ) < 0 && cv::determinant( Rs[1] ) < 0 )
	{
		E = E * -1;
		DecomposeE_Internal( E, Rs, ts );
	}
	cout << " detsB: " << cv::determinant( Rs[0] ) << " " << cv::determinant( Rs[1] ) << endl;
	
	assert( cv::determinant( Rs[0] ) > 0.99 &&  cv::determinant( Rs[0] ) < 1.01);
	assert( cv::determinant( Rs[1] ) > 0.99 &&  cv::determinant( Rs[1] ) < 1.01);
}


bool CamNetCalibrator::PickCameras(vector<unsigned> &fixedCams, vector<unsigned> &variCams)
{
	if( fixedCams.size() == 0 && variCams.size() == 0)
	{
		// it makes no sense if minSharedGrids is a larger value than the largest number of shared grids
		// between cameras.
		int maxShare = 0;
		for( unsigned cc0 = 0; cc0 < Ls.size(); ++cc0 )
		{
			for( unsigned cc1 = 0; cc1 < Ls.size(); ++cc1 )
			{
				if( cc0 == cc1 )
					continue;
				maxShare = std::max( maxShare, sharing(cc0,cc1) );
			}
		}
		if( maxShare < minSharedGrids )
			minSharedGrids = maxShare-1;
		
		
		
		//
		// Unless the config file is overriding us, we will choose the camera 
		// with the most other cameras as our root camera.
		//
		if( rootCam >= Ls.size() )
		{
			// find the camera which shares overlap with the most other cameras.
			unsigned bestSharing = 0;
			for( unsigned cc0 = 0; cc0 < Ls.size(); ++cc0 )
			{
				unsigned shareCount = 0;
				for( unsigned cc1 = 0; cc1 < Ls.size(); ++cc1 )
				{
					if( sharing(cc0, cc1) > minSharedGrids )
						++shareCount;
				}
				if( shareCount > bestSharing )
				{
					bestSharing = shareCount;
					rootCam = cc0;
				}
			}
		}
		
		//
		// Have we _still_ not found a root cam? Probably that akward situation where we have _no_ grids at all.
		//
		if( rootCam >= Ls.size() )
		{
			if( auxMatches.size() > 8 )
				rootCam = 0;
			else
				throw( std::runtime_error( "Couldn't pick root cam - no grids, no aux matches." ) );
		}
		
		
		variCams.push_back(rootCam);
		
		isSetC[ rootCam ] = true;
	}
	else
	{
		// move vari cams to fixed cams.
		fixedCams.insert( fixedCams.end(), variCams.begin(), variCams.end() );
		variCams.clear();
		for(unsigned fcc = 0; fcc < fixedCams.size(); ++fcc )
		{
			isSetC[ fixedCams[fcc] ] = true;
		}
	}

	// find cameras that are unset that share grids with the root
	// camera or the fixed cameras.
	unsigned maybeCam = Ls.size();
	unsigned maybeShareCount = 0;
	unsigned numUnset = 0;
	for( unsigned cc = 0; cc < Ls.size(); ++cc )
	{
		if( !isSetC[cc] )
		{
			++numUnset;

			// check how many grids we share with the fixed or root camera
			bool rootChecked = false;
			unsigned shareCnt = 0;
			for( unsigned fcc = 0; fcc < fixedCams.size(); ++fcc )
			{
				if( fixedCams[fcc] == rootCam )
					rootChecked = true;
				shareCnt += sharing(cc, fixedCams[fcc] );
			}

			if( !rootChecked && rootCam < sharing.cols() )
			{
				shareCnt += sharing(cc, rootCam );
			}

			if( shareCnt > minSharedGrids )
			{
				variCams.push_back( cc );
			}

			if( shareCnt > maybeShareCount )
			{
				maybeCam = cc;
				maybeShareCount = shareCnt;
			}
		}
	}
	
	if( maybeCam == Ls.size() || maybeShareCount < 2)
	{
		// we have an awkward situation now where a camera doesn't just have insufficient shares
		// to be considered, but quite simply, has _no_shares_at_all_
		// We could just fail, but we have a special use case where we might not have many grids,
		// but use point matches instead. In such a case, we need to select the maybeCam from
		// shared point matches.
		// The complication is that those point matches might not be initialised as 3D points yet....
		
		
		// find auxMatches that can be seen by atleast 2 cams.
		std::vector<unsigned> maybeUsefulAux;
		for( unsigned mc = 0; mc < auxMatches.size(); ++mc )
		{
			PointMatch &m = auxMatches[mc];
			unsigned cnt = 0;
			for( unsigned cc = 0; cc < Ls.size(); ++cc )
			{
				//unsigned cc = fixedCams[fc];
				
				if( m.p2D.find(cc) != m.p2D.end() )
					++cnt;
			}
			if( cnt > 1 )
				maybeUsefulAux.push_back( mc );
		}
		
		// now, of those maybe useful auxes, how many are seen by each camera?
		// mostly, just interested in unset cameras.
		std::vector<unsigned> numSeen( Ls.size(), 0 );
		for( unsigned cc = 0; cc < Ls.size(); ++cc )
		{
			if( !isSetC[cc] )
			{
				for( unsigned ac = 0; ac < maybeUsefulAux.size(); ++ac )
				{
					unsigned mc = maybeUsefulAux[ac];
					
					PointMatch &m = auxMatches[mc];
					
					if( m.p2D.find(cc) != m.p2D.end() )
						++numSeen[cc];
					
				}
			}
		}
		
		// now we can set maybeCam to the camera that has the best numSeen.
		unsigned bestCam = 0;
		for( unsigned cc = 0; cc < numSeen.size(); ++cc )
		{
			if( numSeen[cc] > numSeen[bestCam] )
				bestCam = cc;
		}
		
		// to estimate a camera position from a set of points, we'll need at least 5 points.
		if( numSeen[ bestCam ] > 5 )
		{
			maybeCam = bestCam;
		}
		
		// if that fails, there really is nothing we can do.
	}
	

	// force it to add just one camera at a time... the one
	// with the most current shares (maybe cam)
	if( forceOneCam && maybeCam < Ls.size() )
	{
		variCams.clear();

		// if this is the first iteration, then we also need the rootCam in variCams,
		// otherwise we'll be trying to do thing with just one camera!
		if( fixedCams.size() == 0 )
		{
			variCams.push_back( rootCam );
		}
		variCams.push_back( maybeCam );
		return false;
	}

	// we have the root cam added, but nothing passed the grid sharing limits.
	if(variCams.size() == 1 && variCams[0] == rootCam && maybeCam < Ls.size() )
	{
		variCams.push_back( maybeCam );
		return false;
	}
	else if( variCams.size() > 0 )
		return false;

	if( variCams.size() == 0 && numUnset > 0 && maybeCam < Ls.size() )
	{
		// we have a camera that had insufficient shares, but we have to add it, so try to add it now.
		variCams.push_back( maybeCam );
		return false;
	}
	
	// otherwise, there's nothing else that we can do, either because all cameras are fixed, or 
	// because there were no suitable shared grids to add another camera.

    return true;
}


void CamNetCalibrator::BundleAdjust(sbaMode_t mode, vector<unsigned> fixedCams, vector<unsigned> variCams, unsigned numFixedIntrinsics, unsigned numFixedDists)
{
	cout << "BA: " << endl;
	cout << "\tfixed: ";
	for(unsigned fc = 0; fc < fixedCams.size(); ++fc )
		cout << fixedCams[fc] << " ";
	cout << endl;
	cout << "\tvari: ";
	for(unsigned vc = 0; vc < variCams.size(); ++vc )
		cout << variCams[vc] << " ";
	cout << endl;
	
	
	// NULL : squared loss
	ceres::SoftLOneLoss *lossFunc = new ceres::SoftLOneLoss( 2.0f );  // smooth like square near 0, linear with distance.
	//new ceres::HuberLoss(s)                                         // kind of the same but even lower gradient with distance.
	// ceres::CauchyLoss *lossFunc = new ceres::CauchyLoss( 2.0f );   // like a log loss, less and less gradient with distance.
	
	
	// for the Ceres based bundle adjustor, we need to do some prep.
	// first off, collect together the cameras that we're going to use
	// in the right order. We want to specify any fixed cameras first.
	std::vector< transMatrix2D >             CKs;
	std::vector< transMatrix3D >             CLs;
	std::vector< Eigen::Matrix<float,5,1> >  Cks;
	std::vector< hVec3D >                    Cp3ds;
	std::vector< std::vector< hVec2D > >     Cp2ds;
	
	vector< unsigned > cameraMap;
	for( unsigned fc = 0; fc < fixedCams.size(); ++fc )
	{
		cameraMap.push_back( fixedCams[fc] );
		transMatrix2D CK;
		for( unsigned rc = 0; rc < 3; ++rc )
			for( unsigned cc = 0; cc < 3; ++cc )
			{
				CK(rc,cc) = Ks[ fixedCams[fc] ].at<double>(rc,cc);
			}
		transMatrix3D CL = Ls[ fixedCams[fc] ];
		Eigen::Matrix<float,5,1> Ck(5);
		for( unsigned c = 0; c < 5; ++c )
			Ck(c) = ks[ fixedCams[fc] ][c];
		
		CKs.push_back( CK );
		CLs.push_back( CL );
		Cks.push_back( Ck );
	}

	for( unsigned vc = 0; vc < variCams.size(); ++vc )
	{
		cameraMap.push_back( variCams[vc] );
		transMatrix2D CK;
		for( unsigned rc = 0; rc < 3; ++rc )
			for( unsigned cc = 0; cc < 3; ++cc )
			{
				CK(rc,cc) = Ks[ variCams[vc] ].at<double>(rc,cc);
			}
		transMatrix3D CL = Ls[ variCams[vc] ];
		Eigen::Matrix<float,5,1> Ck(5);
		for( unsigned c = 0; c < 5; ++c )
			Ck(c) = ks[ variCams[vc] ][c];
		
		CKs.push_back( CK );
		CLs.push_back( CL );
		Cks.push_back( Ck );
	}
	
	// now we need any 3D points that we have.
	// First off, we'll add the 3D points from the grids,
	// and then later, we'll add auxMatches.
	vector< unsigned > gridPointMap;
	for( unsigned wpc = 0; wpc < worldPoints.size(); ++wpc )
	{
		// which grid is this from?
		unsigned gc = pc2gc[wpc].gc;

		// check visibility in active cameras.
		unsigned cnt = 0;
		for( unsigned acc = 0; acc < cameraMap.size(); ++acc )
		{
			unsigned cc = cameraMap[acc];
			if( grids[cc][gc].size() > 0 )
			{
				++cnt;
			}
		}
		if( cnt > 1 )
		{
			hVec3D p3d = worldPoints[wpc];
			Cp3ds.push_back( p3d );
			gridPointMap.push_back( wpc );
		}
	}
	

	// 2D observations of these grid points.
	Cp2ds.resize( gridPointMap.size() );
	for( unsigned awpc = 0; awpc < gridPointMap.size(); ++awpc )
	{
		unsigned wpc = gridPointMap[ awpc ];

		unsigned gc = pc2gc[wpc].gc;
		unsigned ipc = pc2gc[wpc].pc;

		Cp2ds[awpc].resize( cameraMap.size() );

		hVec2D i;
		for( unsigned cmc = 0; cmc < cameraMap.size(); ++cmc )
		{
			unsigned cc = cameraMap[cmc];
			if( grids[cc][gc].size() > 0 )
			{
				i << grids[cc][gc][ipc].pi(0), grids[cc][gc][ipc].pi(1), 1.0;
			}
			else
			{
				i << 0,0,0;	// the bundle adjustor knows that this means "not visible"
			}
			Cp2ds[awpc][cmc] = i;
		}
	}
	
	// now we can add the auxMatches to this....
	// auxMap[ic] says which auxMatch a3D[ic] came from. 
	std::map< unsigned, unsigned > auxMap;
	for( unsigned amc = 0; amc < auxMatches.size(); ++amc )
	{
		PointMatch &m = auxMatches[amc];
		
		if( m.has3D )
		{
			auxMap[ Cp3ds.size() ] = amc;
			Cp3ds.push_back( m.p3D );
			
			std::vector< hVec2D > ips;
			hVec2D i;
			for( unsigned cmc = 0; cmc < cameraMap.size(); ++cmc )
			{
				if( m.p2D.find( cameraMap[cmc] ) != m.p2D.end() )
				{
					i  = m.p2D[ cameraMap[cmc] ];
				}
				else
				{
					i << 0,0,0;
				}
				
				ips.push_back(i);
			}
			
			Cp2ds.push_back( ips );
		}
	}
	
	
	
	
	
	
	
	
	// ===============================================================
	// now we should be able to create the Ceres based bundle adjustor.
	// ===============================================================
	
	ceres::Problem problem;
	std::vector< std::vector<double> > params;
	std::vector< std::vector<double> > points;
	std::vector< std::vector<int> > paramID;
	
	params.resize( CKs.size() );
	paramID.resize( CKs.size() );
	points.resize( Cp3ds.size() );
	
	// as we build the problem we also set the camera parameters,
	// so we can avoid doing it too often.
	std::vector< bool > setParams( CKs.size(), false );
	
	if( mode == SBA_CAM )
	{
		// for now, this is just one option, camera extrinsics and focal length are solved for.
		// we now need to build the problem.
		for( unsigned pc = 0; pc < Cp3ds.size(); ++pc )
		{
			for( unsigned cc = 0; cc < Cp2ds[pc].size(); ++cc )
			{
				if( Cp2ds[pc][cc](2) == 1.0 )
				{
					if( numFixedIntrinsics == 5 )
					{
						// extrinsics only.
						ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedPExtOnly_BA, 2, 6> *cef;
						
						CalibCeres::CeresFunctor_FixedPExtOnly_BA *errf;
						errf = new CalibCeres::CeresFunctor_FixedPExtOnly_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
						
						
						cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedPExtOnly_BA, 2, 6>( errf );
						
						if( !setParams[cc] )
						{
							errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
							setParams[cc] = true;
						}
						
						
						problem.AddResidualBlock(
						                          cef,
						                          lossFunc /* squared loss */,
						                          &params[cc][0]
						                        );
					}
					else if( numFixedIntrinsics == 4 )
					{
						ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedPFocal_BA, 2, 7>  *cef;
						
						CalibCeres::CeresFunctor_FixedPFocal_BA *errf;
						errf = new CalibCeres::CeresFunctor_FixedPFocal_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
						
						cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedPFocal_BA, 2, 7> ( errf );
						
						if( !setParams[cc] )
						{
							errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
							setParams[cc] = true;
						}
						
						
						problem.AddResidualBlock(
						                          cef,
						                          lossFunc /* squared loss */,
						                          &params[cc][0]
						                        );
					}
				}
			}
		}
	}
	else if( mode == SBA_CAM_AND_POINTS )
	{
		// various complicated and horrific things can happen in here...
		for( unsigned pc = 0; pc < Cp3ds.size(); ++pc )
		{
			std::vector<double> p(3);
			p[0] = Cp3ds[pc](0);
			p[1] = Cp3ds[pc](1);
			p[2] = Cp3ds[pc](2);
			points[pc] = p;
// 			cout << pc << " " << pc > gridPointMap.size() << " " << p[0] << " " << p[1] << " " << p[2] << endl;
			for( unsigned cc = 0; cc < Cp2ds[pc].size(); ++cc )
			{
				if( Cp2ds[pc][cc](2) == 1)
				{
					// is this a fixed camera?
					if( cc < fixedCams.size() )
					{
						// then we need a points only error
						ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedCamera_BA, 2, 3>  *cef;
						
						CalibCeres::CeresFunctor_FixedCamera_BA *errf;
						errf = new CalibCeres::CeresFunctor_FixedCamera_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
						
						cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedCamera_BA, 2, 3> ( errf );
						
						problem.AddResidualBlock(
						                         cef,
						                         lossFunc /* squared loss */,
						                         &points[pc][0]
						                        );
					}
					else
					{
						// we need one of the cam and points functors, but which one?
						if( numFixedIntrinsics == 5 )
						{
							// extrinsics only.
							ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_ExtrinsicOnly_BA, 2, 6, 3> *cef;
							
							CalibCeres::CeresFunctor_ExtrinsicOnly_BA *errf;
							errf = new CalibCeres::CeresFunctor_ExtrinsicOnly_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
							
							
							cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_ExtrinsicOnly_BA, 2, 6, 3>( errf );
							
							if( !setParams[cc] )
							{
								errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
								setParams[cc] = true;
							}
							
							
							problem.AddResidualBlock(
							                          cef,
							                          lossFunc /* squared loss */,
							                          &params[cc][0],
							                          &points[pc][0]
							                        );
						}
						if( numFixedIntrinsics == 4 )
						{
							// F only
							// no distortion options right now.
							ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_Focal_BA, 2, 7, 3> *cef;
							
							CalibCeres::CeresFunctor_Focal_BA *errf;
							errf = new CalibCeres::CeresFunctor_Focal_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
							
							
							cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_Focal_BA, 2, 7, 3>( errf );
							
							if( !setParams[cc] )
							{
								errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
								setParams[cc] = true;
							}
							
							
							problem.AddResidualBlock(cef,
													lossFunc /* squared loss */,
													&params[cc][0],
													&points[pc][0]
													);
						}
						else if( numFixedIntrinsics == 2 )
						{
							// F and cx, cy
							if( numFixedDists == 5 )
							{
								ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple_BA, 2, 9, 3> *cef;
								
								CalibCeres::CeresFunctor_FocalPrinciple_BA *errf;
								errf = new CalibCeres::CeresFunctor_FocalPrinciple_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
								
								
								cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple_BA, 2, 9, 3>( errf );
								
								if( !setParams[cc] )
								{
									errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
									setParams[cc] = true;
								}
								
								
								problem.AddResidualBlock(cef,
														lossFunc /* squared loss */,
														&params[cc][0],
														&points[pc][0]
														);
							}
							else if( numFixedDists == 4 )
							{
								ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple1Dist_BA, 2, 10, 3> *cef;
								
								CalibCeres::CeresFunctor_FocalPrinciple1Dist_BA *errf;
								errf = new CalibCeres::CeresFunctor_FocalPrinciple1Dist_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
								
								
								cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple1Dist_BA, 2, 10, 3>( errf );
								
								if( !setParams[cc] )
								{
									errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
									setParams[cc] = true;
								}
								
								
								problem.AddResidualBlock(cef,
														lossFunc /* squared loss */,
														&params[cc][0],
														&points[pc][0]
														);
							}
							else if( numFixedDists == 3 )
							{
								ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple2Dist_BA, 2, 11, 3> *cef;
								
								CalibCeres::CeresFunctor_FocalPrinciple2Dist_BA *errf;
								errf = new CalibCeres::CeresFunctor_FocalPrinciple2Dist_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
								
								
								cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple2Dist_BA, 2, 11, 3>( errf );
								
								if( !setParams[cc] )
								{
									errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
									setParams[cc] = true;
								}
								
								
								problem.AddResidualBlock(cef,
														lossFunc /* squared loss */,
														&params[cc][0],
														&points[pc][0]
														);
							}
							else if( numFixedDists == 1 )
							{
								ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple4Dist_BA, 2, 13, 3> *cef;
								
								CalibCeres::CeresFunctor_FocalPrinciple4Dist_BA *errf;
								errf = new CalibCeres::CeresFunctor_FocalPrinciple4Dist_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
								
								
								cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FocalPrinciple4Dist_BA, 2, 13, 3>( errf );
								
								if( !setParams[cc] )
								{
									errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
									setParams[cc] = true;
								}
								
								
								problem.AddResidualBlock(cef,
														lossFunc /* squared loss */,
														&params[cc][0],
														&points[pc][0]
														);
							}
							else if( numFixedDists == 0 )
							{
								throw std::runtime_error(" Currently don't have 2 fixed intrinsics and 0 fixed distortions " );
							}
						}
						else if( numFixedIntrinsics == 0 )
						{
							// Full K
							if( numFixedDists == 5 )
							{
								ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FullK_BA, 2, 11, 3> *cef;
								
								CalibCeres::CeresFunctor_FullK_BA *errf;
								errf = new CalibCeres::CeresFunctor_FullK_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
								
								
								cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FullK_BA, 2, 11, 3>( errf );
								
								if( !setParams[cc] )
								{
									errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
									setParams[cc] = true;
								}
								
								// TODO: Options for distortion params
								
								
								
								problem.AddResidualBlock(cef,
														lossFunc /* squared loss */,
														&params.back()[0],
														&points.back()[0]
														);
							}
							else if( numFixedDists == 0 )
							{
								ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FullKk_BA, 2, 16, 3> *cef;
								
								CalibCeres::CeresFunctor_FullKk_BA *errf;
								errf = new CalibCeres::CeresFunctor_FullKk_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
								
								
								cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FullKk_BA, 2, 16, 3>( errf );
								
								if( !setParams[cc] )
								{
									errf->ThingsToParams( CKs[cc], CLs[cc], Cks[cc], params[cc], paramID[cc] );
									setParams[cc] = true;
								}
								
								// TODO: Options for distortion params
								
								
								
								problem.AddResidualBlock(cef,
														lossFunc /* squared loss */,
														&params.back()[0],
														&points.back()[0]
														);
							}
							else
							{
								throw std::runtime_error("Full K only available with 0 or 5 fixed distortion params");
							}
						}
					}
				}
			}
		}
	}
	else if( mode == SBA_POINTS )
	{
		// nice and simple again here.
		for( unsigned pc = 0; pc < Cp3ds.size(); ++pc )
		{
			std::vector<double> p(3);
			p[0] = Cp3ds[pc](0);
			p[1] = Cp3ds[pc](1);
			p[2] = Cp3ds[pc](2);
			points[pc] = p;
			for( unsigned cc = 0; cc < Cp2ds[pc].size(); ++cc )
			{
				// then we need a points only error
				if( Cp2ds[pc][cc](2) == 1)
				{
					ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedCamera_BA, 2, 3> *cef;
							
					CalibCeres::CeresFunctor_FixedCamera_BA *errf;
					errf = new CalibCeres::CeresFunctor_FixedCamera_BA( Cp2ds[pc][cc], CKs[cc], CLs[cc], Cks[cc], Cp3ds[pc] );
					
					
					cef = new ceres::AutoDiffCostFunction<CalibCeres::CeresFunctor_FixedCamera_BA, 2, 3>( errf );
					
					
					problem.AddResidualBlock(cef,
											lossFunc /* squared loss */,
											&(points[pc][0])
											);
				}
			}
		}
	}
	
	// OK then, so if all of the above is vaguely correct, then
	// then Ceres will do its fitting nice and simply...
	
	// we also want to set the options of the solver.
	// TODO: better options for BA (like sparsity and stuff)
	ceres::Solver::Options options;
	options.minimizer_progress_to_stdout = true;
	options.linear_solver_type = ceres::SPARSE_SCHUR;
	
	// and now we can finally run the solver.
	ceres::Solver::Summary summary;
	ceres::Solve( options, &problem, &summary );
	
	cout << summary.BriefReport() << endl;

	if( !summary.IsSolutionUsable() || summary.termination_type == ceres::NO_CONVERGENCE)
	{
		// fitting has failed, so don't bother extracting the results out, they are a nonsense,
		// just return and nothing will have changed.
		return;
	}
	
	// ===============================================================
	// now extract the results, and update our data.
	// ===============================================================
	
	for( unsigned cmc = 0; cmc < cameraMap.size(); ++cmc )
	{
		if( cmc >= fixedCams.size() )
		{
			double f,cx,cy,a,s;
			f  = CKs[cmc](0,0);
			cx = CKs[cmc](0,2);
			cy = CKs[cmc](1,2);
			a  = CKs[cmc](1,1) / CKs[cmc](0,0);
			s  = CKs[cmc](0,1);
			
			double ax[3];
			std::vector<double> R(9);
			unsigned i = 0;
			for( unsigned c = 0; c < 3; ++c )
				for( unsigned r = 0; r < 3; ++r )
				{
					R[i] = CLs[cmc](r,c);
					++i;
				}
			
			ceres::RotationMatrixToAngleAxis(&R[0], ax);
			
			cout << "paramid: " << endl;
			for( unsigned pc = 0; pc < paramID[cmc].size(); ++pc )
			{
				cout << paramID[cmc][pc] << " . ";
			}
			cout << endl;

			for( unsigned pc = 0; pc < paramID[cmc].size(); ++pc )
			{
				switch( paramID[cmc][pc] )
				{
					case BA_PARAMID_f:
						f = params[cmc][pc]; break;
					case BA_PARAMID_cx:
						cx = params[cmc][pc]; break;
					case BA_PARAMID_cy:
						cy = params[cmc][pc]; break;
					case BA_PARAMID_a:
						a = params[cmc][pc]; break;
					case BA_PARAMID_s:
						s = params[cmc][pc]; break;
					
					case BA_PARAMID_raxx:
						ax[0] = params[cmc][pc]; break;
					case BA_PARAMID_raxy:
						ax[1] = params[cmc][pc]; break;
					case BA_PARAMID_raxz:
						ax[2] = params[cmc][pc]; break;
						
					case BA_PARAMID_tx:
						CLs[cmc](0,3) = params[cmc][pc]; break;
					case BA_PARAMID_ty:
						CLs[cmc](1,3) = params[cmc][pc]; break;
					case BA_PARAMID_tz:
						CLs[cmc](2,3) = params[cmc][pc]; break;
						
					case BA_PARAMID_d0:
						Cks[cmc](0) = params[cmc][pc]; break;
					case BA_PARAMID_d1:
						Cks[cmc](1) = params[cmc][pc]; break;
					case BA_PARAMID_d2:
						Cks[cmc](2) = params[cmc][pc]; break;
					case BA_PARAMID_d3:
						Cks[cmc](3) = params[cmc][pc]; break;
					case BA_PARAMID_d4:
						Cks[cmc](4) = params[cmc][pc]; break;
				}
			}
			
			CKs[cmc] << f,s,cx,     0, f*a,cy,   0,0,1;
			
			ceres::AngleAxisToRotationMatrix( &ax[0], &R[0] );
			i = 0;
			for( unsigned c = 0; c < 3; ++c )
				for( unsigned r = 0; r < 3; ++r )
				{
					CLs[cmc](r,c) = R[i];
					++i;
				}
		
		}
		
		
		unsigned camID = cameraMap[cmc];
		for( unsigned rc = 0; rc < 3; ++rc )
			for( unsigned cc = 0; cc < 3; ++cc )
				Ks[ camID ].at<double>(rc,cc) = CKs[cmc](rc,cc);
		
		Ls[camID] = CLs[cmc].cast<float>();
		
		for( unsigned c = 0; c < 5; ++c )
			ks[camID][c] = Cks[cmc](c);
	}
	
	
	unsigned pc = 0;
	while(pc < gridPointMap.size())
	{
		if( points[pc].size() == 3 )
		{
			Cp3ds[pc] << points[pc][0], points[pc][1], points[pc][2], 1.0f;
			
			unsigned wpc = gridPointMap[pc];
			worldPoints[wpc] = Cp3ds[pc].cast<float>();
		}
		++pc;
	}
	
	while( pc < Cp3ds.size() )
	{
		if( points[pc].size() == 3 )
		{
			Cp3ds[pc] << points[pc][0], points[pc][1], points[pc][2], 1.0f;
			
			unsigned apc = auxMap[pc];
			auxMatches[apc].p3D = Cp3ds[pc].cast<float>();
		}
		++pc;
	}
	
	//delete lossFunc;  // actually, don't do this, ceres apparently takes ownership of the pointer.
	
	// it all seems to simple, until you run it...
}


void CamNetCalibrator::CheckAndFixScale()
{
	// as soon as we do bundle adjustment that lets the cameras move,
	// we risk it fucking over the scale of the space. Ideally, we would have our own
	// bundle adjustor and it would have a constraint that made sure the distances between
	// grid points were carefully controlled (and indeed, that they were planar!).
	// We don't have that just now, so we'll do the next best thing, and force the scale back
	// into place.
	
	// the way I've laid out the data means that it is not actually all that easy to 
	// find each grid point, or more specifically, its neighbours, so we're going to have
	// to do this kind of the hard way...
	
	std::vector<      std::vector< std::vector< hVec3D > >    > worldInGrid;
	
	if( grids.size() > 0 )
	{
		worldInGrid.resize( gridFrames.size() );
		for( unsigned gc = 0; gc < gridFrames.size(); ++gc )
		{
			if( isSetG[gc] )
			{
				worldInGrid[gc].resize( gridRows );
				for( unsigned rc = 0; rc < gridRows; ++rc )
				{
					worldInGrid[gc][rc].assign( gridCols, hVec3D::Zero() );
				}
			}
		}
		
		for( unsigned pc = 0; pc < worldPoints.size(); ++pc )
		{
			unsigned gc = pc2gc[pc].gc;
			CircleGridDetector::GridPoint &gp = grids[ Mcams[gc] ][gc][pc2gc[pc].pc];
			unsigned r  = gp.row;
			unsigned c  = gp.col;
			
// 			cout << r << " " << c << " " << gc << " " << gridFrames[gc] << " " << Mcams[gc] << " " << grids.size() << " " << grids[ Mcams[gc]].size() << endl;
			
			worldInGrid[gc][r][c] = worldPoints[pc];
		}
		
		float meanRspacing = 0.0f;
		float meanCspacing = 0.0f;
		unsigned count = 0;
		for( unsigned gc = 0; gc < worldInGrid.size(); ++gc )
		{
			if( worldInGrid[gc].size() == gridRows )
			{
				for( unsigned rc = 0; rc < gridRows - 1; ++rc )
				{
					for(unsigned cc = 0; cc < gridCols - 1; ++cc )
					{
						meanRspacing += (worldInGrid[gc][rc+1][cc] - worldInGrid[gc][rc][cc] ).norm();
						meanCspacing += (worldInGrid[gc][rc][cc+1] - worldInGrid[gc][rc][cc] ).norm();
						++count;
					}
				}
			}
		}
		
		meanRspacing /= count;
		meanCspacing /= count;
		
		if( count > 0 )
		{
			cout << "mean spacing: " << meanRspacing << " " << meanCspacing << endl;
			
			float rscale = gridRSpacing / meanRspacing;
			float cscale = gridCSpacing / meanCspacing;
			float scale  = (rscale + cscale) / 2.0f;
			cout << "fixing scales: " << rscale << " " << cscale << endl;
			
			cout << " note, if fixing scales are very different, that's probably a hint that the calibration has gone awry... " << endl;
		
			// now we can just scale all camera translations.... honest...
			for( unsigned cc = 0; cc < Ls.size(); ++cc )
			{
				Ls[cc](0,3) *= scale;
				Ls[cc](1,3) *= scale;
				Ls[cc](2,3) *= scale;
			}
			
			// and of course, all points! (Duh!)
			for( unsigned pc = 0; pc < worldPoints.size(); ++pc )
			{
				worldPoints[pc](0) *= scale;
				worldPoints[pc](1) *= scale;
				worldPoints[pc](2) *= scale;
			}
			
			// and obviously, who could possibly forget, I mean come on, honestly,
			// really, _who_ could forget them, they are _utterly_ unforgettable...
			// the auxPoints...
			for( unsigned ac = 0; ac < auxMatches.size(); ++ac )
			{
				auxMatches[ac].p3D(0) *= scale;
				auxMatches[ac].p3D(1) *= scale;
				auxMatches[ac].p3D(2) *= scale;
			}
		}
		else
		{
			cout << "no grids to do scale/spacing check" << endl;
		}
	}
}
