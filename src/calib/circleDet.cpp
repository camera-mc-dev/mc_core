#include "calib/circleGridTools.h"
#include "math/intersections.h"

void GridCircleFinder( cv::Mat in_grey, float minMagThresh, float detThresh, float rescale, std::vector< cv::KeyPoint > &kps )
{
	auto t0 = std::chrono::steady_clock::now();
	
	//
	// We can obviously gain a big speed advantage by scaling the image.
	//
	cv::Mat grey;
	if( rescale != 1.0f )
	{
		cv::resize( in_grey, grey, cv::Size(), rescale, rescale );
	}
	else
	{
		grey = in_grey;
	}
	
	auto t1 = std::chrono::steady_clock::now();
	
	//
	// and we need to allocate our voting blob
	//
	int fs = 5;
	cv::Mat k(fs,fs, CV_32FC1, cv::Scalar(0));
	hVec2D c; c << fs/2, fs/2, 1.0f;
	for( unsigned rc = 0; rc < fs; ++rc )
	{
		for( unsigned cc = 0; cc < fs; ++cc )
		{
			hVec2D p; p << cc,rc,1.0f;
			float d = (p-c).norm();
			k.at<float>(rc,cc) = exp(-(d*d)/(2*fs));
		}
	}
	
	
	auto t2 = std::chrono::steady_clock::now();
	
	//
	// First thing is simple image gradient.
	//
	cv::Mat gradx, grady, greySm;
	cv::GaussianBlur( grey, greySm, cv::Size(7,7), 0 );
	cv::Scharr( greySm, gradx, CV_32F, 1, 0, 3);
	cv::Scharr( greySm, grady, CV_32F, 0, 1, 3);
	
	auto t3 = std::chrono::steady_clock::now();
	
	
	//
	// Now get the magnitude of the gradient, and threshold it.
	// This uses the first of our thresholds.
	//
	double mx, Mx;
	cv::minMaxIdx( gradx, &mx, &Mx );
	double my, My;
	cv::minMaxIdx( grady, &my, &My );
	double m, M;
	m = std::min( mx, my );
	M = std::max( Mx, My );
	M = std::max( std::abs(m), M );
	gradx = gradx/M;
	grady = grady/M;
	cv::Mat gx2; cv::multiply( gradx, gradx, gx2 );
	cv::Mat gy2; cv::multiply( grady, grady, gy2 );
	cv::Mat mag; cv::sqrt(gx2+gy2, mag);
	
	// We used the inverse threshold because various incarnations of this
	// algorithm then did a distance transform of the result.
	// That may yet be of value because the true centre of each grid circle
	// is always at a local max of the distance map, and that can make the 
	// circle centres much much more robust to various board angles.
	// But whether it is worth the cost is another question entirely...
	cv::Mat mthr( mag.rows, mag.cols, CV_8UC1, cv::Scalar(0) );
	cv::threshold( mag, mthr, minMagThresh, 255, cv::THRESH_BINARY_INV );
	mthr.convertTo( mthr, CV_8UC1 );
	
	
	auto t4 = std::chrono::steady_clock::now();
	
	//
	// Now we do a voting procedure to find the centres of our circles.
	// This isn't quite as sophisticated as a Hough transform, which is somewhat 
	// out of the necessity of speed.
	//
	cv::Mat vote( grey.rows, grey.cols, CV_32FC1, cv::Scalar(0) );
	
	//
	// Cycle over the rows of the image...
	//
	std::vector< hVec2D > pts, dirs;
	std::vector<float> mags;
	for( unsigned rc = 10; rc < grey.rows-10; ++rc )
	{
		//
		// Find all the pixels on this row where the magnitude of the 
		// gradient was larger than our threshold.
		//
		pts.clear();
		dirs.clear();
		mags.clear();
		for( unsigned cc = 10; cc < grey.cols-10; ++cc )
		{
			unsigned char &m = mthr.at<unsigned char>(rc,cc);
			if( m == 0 ) // because inverse thresh!
			{
				hVec2D p0, d0;
				float gx = gradx.at<float>(rc,cc);
				float gy = grady.at<float>(rc,cc);
				d0 << gx,gy,0.0f; 
				d0 /= d0.norm();
				
				p0 << cc,rc,1.0f;
				
				pts.push_back( p0  );
				dirs.push_back( d0 );
				mags.push_back( m  );
			}
		}
		
		
		//
		// Now we look at pairs of edge points.
		// The idea is that one edge should enter a grid circle, the 
		// other should exit it. Now, we allow for the entry and exit
		// edge to not be neighbours just to add some robustness, and to allow
		// for the alignment dots being rings.
		//
		
		for( unsigned c1 = 0; c1 < pts.size(); ++c1 )
		{
			for( unsigned c2 = c1+1; c2 < c1+7 && c2 < pts.size(); ++c2 )
			{
				hVec2D p0 = pts[c1]; hVec2D d0 = dirs[c1];
				hVec2D p1 = pts[c2]; hVec2D d1 = dirs[c2];
				
				//
				// Check correctness of these two edge points.
				//
				
				// first are a couple of distance thresholds
				if( (p0-p1).norm() > vote.cols / 20 )
					continue; // too far apart.
				
				if( (p0-p1).norm() < 5 )
					continue; // too close.
				
				// Next up we look at the gradients of those two edges.
				// Imagine lines going from those points in the direction of the gradients,
				// thus perpendicular to the circle's edge.
				// We first demand that both move up the image, or both move down the image.
				if( (d0(1) > 0 && d1(1) >0) || (d0(1) < 0 && d1(1) < 0) )
				{
					// for dark-on-light grid circles, these direction lines will point 
					// outwards - the x-grads will point away from each other.
					// for light-on-dark grid circles, the direction lines point inwards,
					// which means they point towards each other.
					// Although this does slightly discriminate between alignment circles
					// and grid circles, it doesn't robustly work across scale, and indeed,
					// with increasing distance, alignment dots start to respond like grid dots.
					// we're more robust by just accepting either case at once.
					if( (d0(0) < 0 && d1(0) > 0) || (d0(0) > 0 && d1(0) < 0) )
					{
						//
						// Intersect the two point+grad directions.
						//
						float s0 = IntersectRays( p0, d0, p1, d1 );
// 						float s1 = IntersectRays( p1, d1, p0, d0 );
						hVec2D i = p0 + s0 * d0;
						
						//
						//  Check the validity of the solution.
						//
						bool valid = ( i(0) > fs && i(0) < grey.cols-fs-1 && i(1) > fs && i(1) < grey.rows-fs-1);
						if( valid )
						{
							//
							// To vote, we draw a broad gaussian shaped feature at the location
							// of the intersection point, thus spreading the vote over an area.
							//
							
							cv::Rect r(i(0)-fs/2, i(1)-fs/2, fs, fs );
							vote( r ) += k;
						}
					}
					else
					{
						continue;
					} // grad check 2
				}
				else
				{
					continue;
				} // grad check 1
			} // c2
		} // c1
	} // rc
	
	auto t5 = std::chrono::steady_clock::now();
	
	//
	// Finally, we process the vote map by normalising it,
	// thresholding it, and running connected components to extract the features.
	//
	
	cv::minMaxIdx( vote, &m, &M );
	cout << m << " " << M << endl;
	vote = (vote-m)/(M-m);
	
	cv::threshold(vote,vote,0.35, 1.0, cv::THRESH_BINARY );
	cv::Mat vth;
	vote.convertTo( vth, CV_8UC1, 255.0f );
	
	cv::Mat labels, stats, centroids;
	cv::connectedComponentsWithStats(vth, labels, stats, centroids);
	
	kps.clear();
	for( unsigned pc = 1; pc < centroids.rows; ++pc )
	{
		cv::KeyPoint k;
		k.size = 10; // who cares.
		cv::Point a( centroids.at<double>(pc,0), centroids.at<double>(pc,1) );
		a.x /= rescale;
		a.y /= rescale;
		k.pt = a;
		kps.push_back(k);
	}
	
	auto t6 = std::chrono::steady_clock::now();
	
	cout << "scale  : " << std::chrono::duration <double, std::milli>(t1-t0).count() << " ms " << endl;
	cout << "init-k : " << std::chrono::duration <double, std::milli>(t2-t1).count() << " ms " << endl;
	cout << "grad   : " << std::chrono::duration <double, std::milli>(t3-t2).count() << " ms " << endl;
	cout << "mag    : " << std::chrono::duration <double, std::milli>(t4-t3).count() << " ms " << endl;
	cout << "vote   : " << std::chrono::duration <double, std::milli>(t5-t4).count() << " ms " << endl;
	cout << "det    : " << std::chrono::duration <double, std::milli>(t6-t5).count() << " ms " << endl;
	cout << "tot    : " << std::chrono::duration <double, std::milli>(t6-t0).count() << " ms " << endl;
	
	
}
