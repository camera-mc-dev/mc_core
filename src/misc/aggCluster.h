#ifndef MC_DEV_AGGCLUSTER_H
#define MC_DEV_AGGCLUSTER_H

#include "math/mathTypes.h"
#include <unordered_map>
//
// Functions to perform simple and probably very naive agglomerative clustering.
//
// Given a set of n "things", you must provide an (n,n) matrix where D(a,b) is the distance 
// between "thing" a and "thing" b.
// 
// Clustering starts with n clusters each containing a single "thing". 
// At each iteration, the smallest distance in D is located, and the pair (a,b) is used 
// to merge together two clusters. D(a,b) is then set to a large number to allow discovery of 
// the next smallest distance.
//



//
// This first function returns the whole clustering sequence, allowing you to 
// analyse the mergeCosts and decide which set of clusters will meet your needs.
//
// If the maxDistance is >= 0 then clustering will stop once there are no possible mergers smaller than maxDistance.
// This is stupidly slow.
//
void AggCluster( genMatrix &D, std::vector< std::vector< std::vector<int> > > &allClusters, std::vector<float> &mergeCosts, float maxDistance = -1.0 );


void AggCluster( genMatrix &points, unsigned k, float maxDist, std::vector<unsigned> &clustIDs );



//
// An attempt to implement the HDBSCAN clustering algorithm. Wish me luck!
// Input is a set of n-d point
// the clustering parameters are:
//      1) k : as in find the distance to the kth nearest neighbour.
//
// arguments:
//       points   : (d,n) matrix of n point with d dimensionality
//       k        : as in "distance to the kth nearest neighbour"
//       clusters : vector of vector of point indices
//
void HDBSCAN( genMatrix &points, unsigned k, unsigned minClusterSize, std::vector<unsigned> &clustIDs );






#endif


