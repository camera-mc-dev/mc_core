#ifndef CALIB_CIRCLE_GRID_TOOLS_H_MC
#define CALIB_CIRCLE_GRID_TOOLS_H_MC

#include "imgio/imagesource.h"
#include "renderer2/basicRenderer.h"
#include "renderer2/geomTools.h"
#include "renderer2/sdfText.h"

#include <vector>
#include <opencv2/core.hpp>
using std::vector;

// given an image source, find the circle centres
// for a circle-grid calibration board in the images.
// specify the number of rows and number of columns in the grid.
//
// Get grids for all images in the source.
void GetCircleGrids(ImageSource *imgSrc, vector< vector<cv::Point2f> > &centers, unsigned rows, unsigned cols, bool visualise=false);

// Get grid for the current image in the source.
// Returns false if grid not detected.
bool GetCircleGrids(ImageSource *imgSrc, vector<cv::Point2f> &centers, unsigned rows, unsigned cols, bool visualise=false);

// Build the "object" points for a grid of a specified size
// in terms of the number of rows and columns, and the spacing
// between rows and columns.
void BuildGrid(vector< cv::Point3f > &gridCorners, unsigned rows, unsigned cols, float rowSpace, float colSpace);



// debugging renderer for circle grid detector - fwd declaration so next class knows about it.
class CGDRenderer;


//
// MSER is too slow for detection, so we've developed something a wee bit faster.
//
// minMagThresh: magnitude of image gradients is normalised 0->1, then thresholded.
//               smaller values means we have more edges to process, means slower.
//               set it too high and we miss circles.
//               0.05 -> 0.3 have been OK depending.
//               better lighting makes for stronger black/white circle contrast allowing larger threshold.
//
// detThresh   : Votes for circles are normalised 0->1.
//               threshold the normalised votes to hopefully get good circles.
//               too low you get too much noise. Too high you miss circles.
//               0.3 wasn't a bad lower limit in dev testing.
//
void GridCircleFinder( cv::Mat grey, float minMagThresh, float detThresh, float rescale, std::vector< cv::KeyPoint > &kps );


// This replaces the unreliable OpenCV circle grid finder.
// We use MSER for detecing blobs (grid circles) in the image,
// and then try to work out the grid from the detected blobs.
class CircleGridDetector
{
public:
	enum blobDetector_t {MSER_t, SURF_t, CIRCD_t};
	
	CircleGridDetector( unsigned w, unsigned h, bool useHypothesis, bool visualise = false, blobDetector_t in_bdt = MSER_t, hVec2D down = {0,1,0});
	CircleGridDetector( unsigned w, unsigned h, libconfig::Setting &cfg, hVec2D down = {0,1,0} );
	
	std::shared_ptr<CGDRenderer> ren;
	
	
	
	struct GridPoint
	{
		// position in grid
		unsigned row, col;
		
		// position in image.
		hVec2D pi;
		
		// hypothesis in image.
		hVec2D ph;
		
		// mostly only used for debugging.
		float blobRadius;
		
		bool operator<( const GridPoint &oth) const
		{
			if( row < oth.row )
				return true;
			else if( row == oth.row )
			{
				return col < oth.col;
			}
			return false;
		}
		
	};
	
	
	void TestKeypoints( cv::Mat img, bool isGridLightOnDark, vector< cv::KeyPoint > &kps, std::vector<float> &score );
	
	bool FindGrid( cv::Mat img, unsigned rows, unsigned cols, bool hasAlignmentDots, bool isGridLightOnDark, std::vector< GridPoint > &gridPoints );
	
	// occasionally useful. Only valid after call to FindGrid.
	void GetAlignmentPoints( vector< cv::KeyPoint > &out_akps )
	{
		out_akps = akps;
	}
	
	
	float maxHypPointDist;
	
	struct kpLine
	{
		unsigned a, b;
		float err;
		vector<unsigned> closestVerts;
		
		hVec2D d;	// line direction
		hVec2D p0;	// start point
		
		bool operator<(const kpLine &oth) const
		{
			return err < oth.err;
		}
	};

	struct kpLineDist
	{
		unsigned kc;
		float err;

		bool operator<(const kpLineDist &oth) const
		{
			return err < oth.err;
		}
	};

	struct lineInter
	{
		unsigned lineIdx;
		float scalar;

		bool operator<( const lineInter &oth) const
		{
			return scalar < oth.scalar;
		}
	};
	
	
	int potentialLinesNumNearest;       // 8
	float parallelLineAngleThresh;	      // 5  (degrees)
	float parallelLineLengthRatioThresh;  // 0.7
	
	float gridLinesParallelThresh;        // 25 (degrees)
	float gridLinesPerpendicularThresh;   // 60 (degrees)
	
	float gapThresh;                      // 0.8
	
	float alignDotDistanceThresh;         // 10 (pixels)
	float alignDotSizeDiffThresh;         // 25 (pixels?)
	
	
	
	int MSER_delta;    // 3
	int MSER_minArea;  // 50
	int MSER_maxArea;  // 200*200
	double MSER_maxVariation; // 1.0
	
	double SURF_thresh; // 5000 has been good.
	
	
	
	float maxGridlineError;
	
	
	float cd_minMagThresh, cd_detThresh, cd_rescale;

protected:

	cv::Mat grey;
	
	blobDetector_t blobDetector;
	
	unsigned rows, cols;
	hVec2D down;
	
	
	bool showVisualiser;
	
	
	int imgWidth, imgHeight;
	
	
	
	void RoughClassifyKeypoints( cv::Mat grey, bool isGridLightOnDark, vector< cv::KeyPoint > &filtkps );
	void InitKeypoints(cv::Mat &grey, std::vector<cv::KeyPoint> &filtkps);
	void FindKeypoints(bool isLightOnDark );

	
	vector< cv::KeyPoint > kps, ikps, akps;
	vector<bool> couldBeGrid;
	float keyPointSize;

	void GetInitialLines();
	vector< kpLine > initLines;

	bool GetGridLines();
	vector< kpLine > gridLinesA, gridLinesB;

	bool SelectRowsCols(bool hasAlignmentDots );
		void FilterLines(vector< kpLine > &lines, vector< lineInter > &inters );
		int CheckAlignKP(bool isRow, int se );
	vector< kpLine > rowLines, colLines;
	vector< kpLine > alignErrLines;

	void GetGridVerts(vector< GridPoint > &finalGridPoints );

	
	bool useGridPointHypothesis;

};


class CGDRenderer : public Rendering::BasicRenderer
{
public:
    friend class Rendering::RendererFactory;		
protected:
	CGDRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title)
	{
		
	}
public:
	bool Step();
	
	
	void RenderLines( cv::Mat img, const vector< CircleGridDetector::kpLine > &lines, const vector< cv::KeyPoint > &kps, Eigen::Vector4f colour );
	void RenderLines( cv::Mat img, const vector< CircleGridDetector::kpLine > &lines, Eigen::Vector4f colour );
	void RenderLines( cv::Mat img, const vector< CircleGridDetector::kpLine > &lines, const vector< cv::KeyPoint > &kps );
	void RenderGridPoints( cv::Mat img, const vector< CircleGridDetector::GridPoint > &gps, std::string tag = "gp-" );
	void RenderKeyPoints( cv::Mat img, const vector< cv::KeyPoint > &kps, Eigen::Vector4f colour, std::string tag = "kp-" );
	void ClearLinesAndGPs();
	
	std::shared_ptr< Rendering::SceneNode > linesRoot;
	std::shared_ptr< Rendering::SceneNode > gpsRoot;
	
	int luuid;
	
protected:
	virtual void FinishConstructor()
	{
		BasicRenderer::FinishConstructor();
		
		Rendering::NodeFactory::Create(linesRoot, "linesRoot");
		Rendering::NodeFactory::Create(gpsRoot, "gpsRoot");
		
		Get2dFgRoot()->AddChild(linesRoot);
		Get2dFgRoot()->AddChild(gpsRoot);
		
		luuid = 0;
	}
	
};


#endif
