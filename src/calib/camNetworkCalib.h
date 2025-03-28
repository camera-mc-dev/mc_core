#ifndef CAM_NETWORK_CALIB_H_MC
#define CAM_NETWORK_CALIB_H_MC

#include <iostream>
#include <fstream>
using std::cout;
using std::endl;
#include <mutex>
#include <vector>
#include <string>

#include "calib/calibration.h"
#include "math/mathTypes.h"
#include "imgio/imagesource.h"
#include "calib/circleGridTools.h"

#include "renderer2/basicRenderer.h"

#include "libconfig.h++"

enum sbaMode_t {SBA_CAM_AND_POINTS, SBA_CAM, SBA_POINTS};

class CamNetCalibrator
{
public:

	CamNetCalibrator(std::string configFile)
	{
		cfgFile = configFile;
		ReadConfig();

		dren = NULL;
	}

	void Calibrate();

	static void ReadGridsFile( std::ifstream &infi, vector< vector< CircleGridDetector::GridPoint > > &grids );
	static void WriteGridsFile( std::ofstream &outfi, vector< vector< CircleGridDetector::GridPoint > > &grids );


private:

	struct ptOrigin
	{
		unsigned gc;	// grid it came from.
		unsigned pc;	// index in grid.
	};

	std::string cfgFile;
	libconfig::Config cfg;
	void ReadConfig();

	// image sources and configuration of
	// for now assumed to be image directories
	std::vector< ImageSource* > sources;
	std::vector< hVec2D > downHints;
	std::vector<bool> isDirectorySource;
	std::string dataRoot;
	std::string testRoot;
	vector< std::string > imgDirs;
	std::map< std::string, unsigned > srcId2Indx;

	// grids.
	std::vector< int > gridProg;
	std::vector< int > gridFound;
	void FindGridThread(ImageSource *dir, unsigned isc,omp_lock_t &coutLock, hVec2D downHint);
	void GetGrids();
	unsigned maxGridsForInitial;
	unsigned gridRows;
	unsigned gridCols;
	float gridRSpacing, gridCSpacing;
	bool isGridLightOnDark;
	bool gridHasAlignmentDots;
	bool useHypothesis;
	unsigned minSharedGrids;
	bool forceOneCam;
	bool onlyExtrinsicsForBA;
	unsigned minGridsToInitialiseCam;
	// std::vector< std::vector< std::vector< cv::Point2f > > > grids;
	std::vector< std::vector< std::vector< CircleGridDetector::GridPoint > > > grids;

	vector<unsigned> gridFrames;

	// auxiliary points matched between camera views.
	struct PointMatch
	{
		unsigned id;
		std::map< unsigned, hVec2D > p2D;       // the point as originally seen in 2D
		std::map< unsigned, hVec2D > proj2D;    // the point as currently projected.
		hVec3D p3D;
		bool has3D;
	};
	std::vector<PointMatch> auxMatches;
	std::string auxMatchesFile;
	void LoadAuxMatches();
	void InitialiseAuxMatches(std::vector<unsigned> &fixedCams, std::vector<unsigned> &variCams);


	// intrinsic calibration
	void CalibrateIntrinsics();
	vector<cv::Mat> Ks;
	vector< vector<float> > ks;
	vector< cv::Point3f > objCorners;
	bool intrinsicsOnly;


	// extrinsics
	void CheckAndFixScale();
	void CalibrateExtrinsics();
	void DetermineGridVisibility();
	void InitialiseGrids(std::vector<unsigned> &fixedCams, std::vector<unsigned> &variCams);
	void InitialiseCams( std::vector<unsigned> &variCams );
	bool EstimateCamPos(unsigned camID);
	bool EstimateCamPosFromF(unsigned camID);
		cv::Mat GetEssentialFromF( cv::Mat F, cv::Mat &K1, cv::Mat &K2 );
		void DecomposeE( cv::Mat &E, std::vector<cv::Mat> &Rs, std::vector<cv::Mat> &ts);
		void DecomposeE_Internal( cv::Mat &E, std::vector<cv::Mat> &Rs, std::vector<cv::Mat> &ts);
		// transMatrix3D DecomposeE( cv::Mat E
	void BundleAdjust(sbaMode_t mode, vector<unsigned> fixedCams, vector<unsigned> variCams, unsigned numFixedIntrinsics, unsigned numFixedDists);
	float CalcReconError();
	bool PickCameras(vector<unsigned> &fixedCams, vector<unsigned> &variCams);
	Eigen::MatrixXi sharing;		// which cameras can see which grids?
	vector< hVec3D > worldPoints;	// all 3D grid points.
	vector<ptOrigin> pc2gc;			// which grid did each point come from?
	vector<transMatrix3D> Ms;		// initial transformation of grids relative to world.
	vector<int>      Mcams;			// which camera was used to initialise this grid?
	vector<transMatrix3D> Ls;		// camera L matrices (take point in world space and move to camera space)
	std::vector< bool    > isSetG;	// do grids have a computed M?
	std::vector< bool    > isSetC;	// do cameras have a computed L?
	unsigned numGrids;
	unsigned numCams;
	unsigned rootCam;
	unsigned numIntrinsicsToSolve;
	unsigned numDistortionToSolve;


	// and done!
	void SaveResults();
	std::string out3DFile;
	std::string outErrsFile;

	// rendering stuff
	class CalibRenderer : public Rendering::BasicRenderer
	{
		friend class Rendering::RendererFactory;	
		// The constructor should be private or protected so that we are forced 
		// to use the factory...
	protected:
		// the constructor creates the renderer with a window of the specified
		// size, and with the specified title.
		CalibRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title) {}
	public:
		bool Step(int &camChange, int &gridChange)
		{
			win.setActive();
			Render();
			
			camChange = 0;
			
			sf::Event ev;
			while( win.pollEvent(ev) )
			{
				if( ev.type == sf::Event::Closed )
					return true;
				if( ev.type == sf::Event::KeyReleased )
				{
					if (ev.key.code == sf::Keyboard::Space || ev.key.code == sf::Keyboard::Return || ev.key.code == sf::Keyboard::Escape )
					{
						return true;
					}
					if (ev.key.code == sf::Keyboard::Up )
					{
						gridChange = 1;
					}
					if (ev.key.code == sf::Keyboard::Down )
					{
						gridChange = -1;
					}
					if (ev.key.code == sf::Keyboard::Left )
					{
						camChange = -1;
					}
					if (ev.key.code == sf::Keyboard::Right )
					{
						camChange = 1;
					}
				}
			}
			return false;
		}
	};
	
	
	
	std::shared_ptr<CalibRenderer> dren;
	std::shared_ptr<Rendering::Mesh> camMesh;
	std::vector< std::shared_ptr<Rendering::MeshNode> > camNodes;
	
	
	void Visualise();
	unsigned visualise;

	void DebugGrid(unsigned source, unsigned grid, vector<hVec2D> &obs, vector<hVec3D> &p3d);

	unsigned sbaVerbosity;




};


#endif
