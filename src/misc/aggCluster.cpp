#include "misc/aggCluster.h"
#include <nanoflann.hpp>
#include <algorithm>

#include <iostream>
using std::cout;
using std::endl;

#include <chrono>

#include "renderer2/basicRenderer.h"

void AggCluster( genMatrix &D, std::vector< std::vector< std::vector<int> > > &allClusters, std::vector<float> &mergeCosts, float maxDistance )
{
	cout << "clustering..." << endl;
	assert( D.rows() == D.cols() && D.rows() > 0 );
	
	int Mr, Mc;
	float DMax = D.maxCoeff(&Mr, &Mc);
	if( maxDistance < 0 )
		maxDistance = DMax + 1.0f;
	std::vector<int> clustInds( D.rows() );
	std::vector< std::vector<int> > clusters( D.rows() );
	clusters.resize( D.rows() );
	for( unsigned c = 0; c < D.rows(); ++c )
	{
		clustInds[c] = c;
		clusters[c].push_back(c);
	}
	
	allClusters.push_back( clusters );
	mergeCosts.push_back(-1.0);
	
	float bestDist = 0.0f;
	int mr, mc;
	
	
	while( bestDist < maxDistance )
	{
		bestDist = D.minCoeff(&mr, &mc);
		cout << "bestDist: " << bestDist << " " << DMax << " " << mr << " " << mc << endl;
		if( clustInds[mr] == clustInds[mc] )
		{
			// already in the same cluster.
			D(mr,mc) = DMax + 10.0f;
		}
		else
		{
			// put all of the second cluster into the first cluster.
			int c0 = clustInds[mr];
			int c1 = clustInds[mc];
			for( unsigned c = 0; c < clustInds.size(); ++c )
				if( clustInds[c] == c1 )
					clustInds[c] = c0;
			
// 			allClusters.push_back( clusters );
			mergeCosts.push_back( bestDist );
			
			D(mr,mc) = DMax + 10.0f;
		}
	}
	
}




typedef nanoflann::KDTreeEigenMatrixAdaptor< genMatrix > tcTree_t;

struct SEdge
{
	unsigned a,b;  // indices of vertices at either end of this edge.
	float dist;    // euclidean distance between vertices.
	float mrDist;  // "mutual reachability" distance between vertices.
	
	// when this edge was added to our minSpanningTree, which vertices 
	// were already clustered with vertices a and b?
	std::vector< unsigned > vertsA, vertsB;
};
bool SEdgeCompare(const SEdge &a, const SEdge &b)
{
	return a.mrDist < b.mrDist;
}

// which "cluster" is element c currently in?
unsigned DJSetFind( unsigned c, std::vector<unsigned> &parent )
{
	unsigned r = c;
	while( parent[r] != r )
		r = parent[r];
	return r;
}


void AggCluster( genMatrix &points, unsigned k, float maxDist, std::vector<unsigned> &clustIDs )
{
	cout << "Starting clustering: " << endl;
	cout << "\tnum points: " << points.cols() << endl;
	auto t0 = std::chrono::steady_clock::now();
	
	//
	// The first thing we'll do is build a kd-tree of the points.
	//
	genMatrix pt = points.transpose();       // nanoflann wants the points as rows of the matrix.
	tcTree_t tree( pt.cols(), pt, 10);     // '10' is the number of leaf nodes in the tree - kind of a performance tuning thing.
	tree.index->buildIndex();
	
	auto t1 = std::chrono::steady_clock::now();
	
	// Find some number of edges.
	std::vector<float> q( points.rows() );
	std::vector< SEdge > edges;
	for( unsigned c0 = 0; c0 < points.cols(); ++c0 )
	{
		std::vector<size_t> ret_indexes(k);
		std::vector<float>  out_dists_sqr(k);
		nanoflann::KNNResultSet<float> resultSet(k);
		resultSet.init(&ret_indexes[0], &out_dists_sqr[0] );
		
		q[0] = points(0,c0);
		q[1] = points(1,c0);
		tree.index->findNeighbors(resultSet, &q[0], nanoflann::SearchParams(10));
		
		for( unsigned c = 0; c < k; ++c )
		{
			SEdge e;
			e.a = c0;
			e.b = ret_indexes[c];
			e.dist = (points.col(e.a) - points.col(e.b)).norm();
			e.mrDist = e.dist;
			
			if( e.dist < maxDist )
			{
				edges.push_back(e);
			}
		}
	}
	
	auto t2 = std::chrono::steady_clock::now();
	
	
	// then we sort them from shortest to longest.
	std::sort( edges.begin(), edges.end(), SEdgeCompare );
	
	auto t3 = std::chrono::steady_clock::now();
	
	
	std::vector<unsigned> parents( points.cols() );
	for( unsigned c = 0; c < points.cols(); ++c )
	{
		parents[c] = c;
	}
	
	for( unsigned ec = 0; ec < edges.size(); ++ec )
	{
		
		// does this edge connect two clusters?
		int c0 = DJSetFind( edges[ec].a, parents );
		int c1 = DJSetFind( edges[ec].b, parents );
		if( c0 != c1 )
		{
			parents[c1] = c0;
		}
	}
	
	auto t4 = std::chrono::steady_clock::now();
	
	clustIDs.resize( points.cols() );
	for( unsigned pc = 0; pc < points.cols(); ++pc )
	{
		clustIDs[pc] = DJSetFind( pc, parents );
	}
	
	
}

void HDBSCAN( genMatrix &points, unsigned k, unsigned minClusterSize, std::vector<unsigned> &clustIDs )
{
	
// 	std::shared_ptr< Rendering::BasicRenderer > ren;
// 	Rendering::RendererFactory::Create( ren, 1280, 720, "agg tst" );
// 	cv::Mat bgImg(1080,1920,CV_32FC3,cv::Scalar(0,0,0) );
// 	ren->Get2dBgCamera()->SetOrthoProjection( 0, 1920, 0, 1080, -10, 10 );
	
// 	cout << "Starting clustering: " << endl;
// 	cout << "\tnum points: " << points.cols() << endl;
	auto t0 = std::chrono::steady_clock::now();
	
	//
	// The first thing we'll do is build a kd-tree of the points.
	//
	genMatrix pt = points.transpose();       // nanoflann wants the points as rows of the matrix.
	tcTree_t tree( pt.cols(), pt, 10);     // '10' is the number of leaf nodes in the tree - kind of a performance tuning thing.
	tree.index->buildIndex();
	
	auto t1 = std::chrono::steady_clock::now();
	
	
	//
	// Next up, we want to compute the "core distance" of each input point. 
	//
	// We do this by computing the distance to the kth nearest neighbour.
	// 
	Eigen::VectorXf coreDistances( points.cols() );
	
	std::vector<float> q( points.rows() );
	for( unsigned c0 = 0; c0 < points.cols(); ++c0 )
	{
		std::vector<size_t> ret_indexes(k);
		std::vector<float>  out_dists_sqr(k);
		nanoflann::KNNResultSet<float> resultSet(k);
		resultSet.init(&ret_indexes[0], &out_dists_sqr[0] );
		
		q[0] = points(0,c0);
		q[1] = points(1,c0);
		tree.index->findNeighbors(resultSet, &q[0], nanoflann::SearchParams(10));
		
		coreDistances(c0) = sqrt( out_dists_sqr.back() );
	}
	
	auto t2 = std::chrono::steady_clock::now();
	
	//
	// We'll construct a minimum spanning tree of the edges - but in doing so, we'll
	// use mutual reachability for the edge length, rather than actual edge length.
	//
	
	// First we construct all edges.
	std::vector< SEdge > edges;
	for( unsigned c0 = 0; c0 < points.cols(); ++c0 )
	{
		for( unsigned c1 = c0+1; c1 < points.cols(); ++c1 )
		{
			SEdge edge;
			edge.a = c0;
			edge.b = c1;
			edge.dist = (points.col(c0) - points.col(c1)).norm();
			edge.mrDist = std::max( std::max( coreDistances( edge.a ), coreDistances( edge.b ) ), edge.dist );
			
			edges.push_back( edge );
		}
	}
	
	auto t3 = std::chrono::steady_clock::now();
	
	// then we sort them from shortest to longest.
	std::sort( edges.begin(), edges.end(), SEdgeCompare );
	
	auto t4 = std::chrono::steady_clock::now();
	
	// We'll basically use Kruskal's algorithm to build a minimum spanning tree using
	// our sorted edges, but there's something of a complication.
	// Basically, we can do a disjoint set - so all we need is a list of parents for 
	// each point.
	// When parent[c] == c, that means there is no parent - but there's another way of thinking 
	// about this. It means we've reached the ID of the cluster.
	//
	//
	std::vector<unsigned> parents( points.cols() );
	for( unsigned c = 0; c < points.cols(); ++c )
	{
		parents[c] = c;
	}
	
	// keep track of how many points are beneath a representative element,
	// and build up the stability of the clusters. The stability is
	// the sum of the difference of mrDist for each point in the cluster,
	// between the cluster's birth and death. Man words make this seem
	// more difficult than it is...
	std::vector<unsigned>  sizes( points.cols(), 1    );
	std::vector<float>     minDists( points.cols(), 0.0f );
	std::vector<float>     maxDists( points.cols(), 0.0f );
	std::vector<float>     stability( points.cols(), 0.0f );
	std::vector<bool>      selected(  points.cols(), true );
	std::vector<unsigned>  childa( points.cols(), 9999999 );
	std::vector<unsigned>  childb( points.cols(), 9999999 );
	
	
	//
	// OK, so... the clustering process is based on building 
	// a minimum spanning tree...
	//
	unsigned sa,sb,np;
	for( unsigned ec = 0; ec < edges.size(); ++ec )
	{
		
		// does this edge connect two clusters?
		int c0 = DJSetFind( edges[ec].a, parents );
		int c1 = DJSetFind( edges[ec].b, parents );
		if( c0 != c1 )
		{
// 			cout << ec << "/" << edges.size() << " : " << edges[ec].a << " -> " << edges[ec].b << endl;
// 			for( unsigned c = 0; c < points.cols(); ++c )
// 			{
// 				cv::circle( bgImg, cv::Point( points(0,c), points(1,c) ), 1, cv::Scalar( 1.0, DJSetFind( c, parents ) / (float) parents.size(), 0.0f ), -1 );
// 			}
// 			cv::line( bgImg, cv::Point( points(0, edges[ec].a ), points(1, edges[ec].a) ), cv::Point( points(0, edges[ec].b ), points(1, edges[ec].b) ), cv::Scalar(0,0,1), 2 );
// 			ren->SetBGImage(bgImg);
// 			ren->StepEventLoop();
			
			// The basic rule is that we're going to merge the smaller 
			// cluster into the larger cluster.
			// The larger cluster will be a, the smaller one b.
			unsigned ca, cb;
			if( sizes[c0] >= sizes[c1] )
			{
				sa = sizes[c0];
				sb = sizes[c1];
				ca = c0;
				cb = c1;
			}
			else
			{
				sa = sizes[c1];
				sb = sizes[c0];
				ca = c1;
				cb = c0;
			}
			
			// So we now know which is a and which is b.
			// The next question we have is about the size of 
			// a and b. If a and b are large clusters, then 
			// we need to make a new cluster c by combining them.
			// Otherwise, we need to merge b into a.
			if( sa > minClusterSize && sb > minClusterSize )
			{
				//
				// So we need to make a new cluster. Which really just means 
				// that we need to add an element to our various arrays.
				//
				
				unsigned cc = parents.size(); // clusterID for the new cluster.
				parents.push_back( cc );      // new cluster is currently its own parent.
				sizes.push_back( sa + sb );   // size of new cluster is size of a and b
				
				// and the sizes. We don't know the maxDist yet, but we start
				// accumulating the minDist.
				minDists.push_back( edges[ec].mrDist * (sa+sb) );
				maxDists.push_back( edges[ec].mrDist * (sa+sb) );
				
				
				// and we want a record of which clusters formed us,
				// and point them to us too.
				childa.push_back( ca );
				childb.push_back( cb );
				parents[ca] = cc;
				parents[cb] = cc;
				
				// and a couple other place-holders.
				stability.push_back( -1.0f );
				selected.push_back(false);
				
				//
				// But we also need to complete the previous clusters.
				//
				
				// first off, we can compute the stability of ca...
				maxDists[ca]  = edges[ec].mrDist * sizes[ca];
				stability[ca] = maxDists[ca] - minDists[ca];
				
				// ... and of cb ...
				maxDists[cb]  = edges[ec].mrDist * sizes[cb];
				stability[cb] = maxDists[cb] - minDists[cb];
				
				// and the final thing is to decide whether ca and/or cb
				// are selected. To do that, we need to know where ca and cb
				// came from.
				
				// ca came from caa and cab
				unsigned caa = childa[ca];
				unsigned cab = childb[ca];
				
				
				// cb came from cba and cbb
				unsigned cba = childa[cb];
				unsigned cbb = childb[cb];
				
				
				if( caa < stability.size() && cab < stability.size() )
				{
					if( stability[ca] < (stability[caa] + stability[cab] ) )
					{
						// ca is not more stabe than its children, so caa
						// and cab remain selected.
						stability[ca] = stability[caa] + stability[cab];
						selected[ca]   = false;
					}
					else
					{
						selected[ca] = true;
						selected[caa] = false;
						selected[cab] = false;
					}
				}
				
				if( cba < stability.size()  && cbb < stability.size()  )
				{
					if( stability[cb] < (stability[cba] + stability[cbb] ) )
					{
						// caa is not more stable than its children, so caa
						// and cab remain selected.
						stability[cb] = stability[cba] + stability[cbb];
						selected[cb]   = false;
					}
					else
					{
						selected[cb] = true;
						selected[cba] = false;
						selected[cbb] = false;
					}
				}
			}
			else
			{
				// cb was too small to be a cluster.
				// ca continues, just larger.
				sizes[ ca ]    += sb;
				maxDists[ ca ]  = edges[ec].mrDist * sizes[ ca ];
				minDists[ ca ] += edges[ec].mrDist * sizes[ cb ];
				selected[cb]    = false;
				
				parents[cb] = ca;
			}
		}
	}
	
	//
	// We need to finish any unfinished clusters. How do we do that?
	// Have I got a way of saying it has been finished? Yeah,
	// the stability value (the setting of which is how we deem ourselves to be finished.)
	// 
	for( unsigned clc = 0; clc < stability.size(); ++clc )
	{
		if( stability[clc] < 0.0f )
		{
			unsigned ca = childa[clc];
			unsigned cb = childb[clc];
			
			stability[clc] = maxDists[clc] - minDists[clc];
			if( stability[clc] < (stability[ca] + stability[cb] ) )
			{
				// caa is not more stable than its children, so caa
				// and cab remain selected.
				stability[clc] = stability[ca] + stability[cb];
				selected[clc]  = false;
			}
			else
			{
				selected[clc] = true;
				selected[ca] = false;
				selected[cb] = false;
			}
			
		}
	}
	
	
	auto t5 = std::chrono::steady_clock::now();
	
	//
	// The final thing we need to do is return our flat clusters
	//
	for( unsigned pc = 0; pc < parents.size(); ++pc )
	{
// 		cout << pc << " : " << selected[pc] << " " << sizes[pc] << " " << parents[pc];
		if( selected[pc] )
		{
			bool anyParentSelected = false;
			unsigned r = pc;
			while( parents[r] != r && !anyParentSelected )
			{
				r = parents[r];
				anyParentSelected = selected[r];
			}
			
// 			cout << " " << anyParentSelected << " ";
			
			// If something above us is selected
			if( anyParentSelected )
			{
				selected[pc] = false;
			}
			else
			{
				parents[pc] = pc;
			}
		}
// 		cout << endl;
	}
	clustIDs.resize( points.cols(), points.cols()+1 );
	for( unsigned pc = 0; pc < points.cols(); ++pc )
	{
		clustIDs[pc] = DJSetFind( pc, parents );
	}
	
	auto t6 = std::chrono::steady_clock::now();
// 	cout << "\t tree  : " << std::chrono::duration <double, std::milli>(t1 - t0).count() << " ms" << endl;
// 	cout << "\t coreD : " << std::chrono::duration <double, std::milli>(t2 - t1).count() << " ms" << endl;
// 	cout << "\t edges : " << std::chrono::duration <double, std::milli>(t3 - t2).count() << " ms" << endl;
// 	cout << "\t sort  : " << std::chrono::duration <double, std::milli>(t4 - t3).count() << " ms" << endl;
// 	cout << "\t MST   : " << std::chrono::duration <double, std::milli>(t5 - t4).count() << " ms" << endl;
	
}


