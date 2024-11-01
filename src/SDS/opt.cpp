// Basic class for SDS optimisation
// (requires customised Agents)
// Murray Evans 2009
//
//
// 2014: Updated slightly to aid my work at SLE.

#include "opt.h"
#include <cmath>
#include "float.h"
#include <iostream>
#include <cstdlib>
#include <random>
using std::cout;
using std::endl;

SDS::Optimiser::Optimiser()
{
}

SDS::Optimiser::~Optimiser()
{
}

SDS::Agent::Agent()
{
}

SDS::Agent::~Agent()
{
}

double SDS::Rand01()
{
	return (double)rand()/(double)RAND_MAX;
}


void SDS::Optimiser::InitialiseOpt(vector<double> &initPos, vector<double> &initRanges, vector<Agent*> &agents, double rangeThresh, int maxIterations)
{
	std::random_device rd;
	std::default_random_engine g(rd());
	//g.seed(0);

	termRangeThresh = rangeThresh * rangeThresh;
	termMaxIters    = maxIterations;

	bestError = DBL_MAX;
	bestAgent = 0;
	iterCount = 0;

	// distribute the agents relative to the initial position
	vector<Agent*>::iterator ai;
	for( ai = agents.begin(); ai < agents.end(); ++ai)
	{
		Agent *cur;
		cur = *ai;

		cur->position.clear();
		cur->position.insert( cur->position.begin(), initPos.begin(), initPos.end());

		// add randomness to all but one agent.
		if( ai != agents.begin() )
		{
// 			for( int p = 0; p < cur->position.size(); ++p)
// 			{
// 				cur->position[p] += (SDS::Rand01() * initRanges[p]) - (initRanges[p]/2.0);
// 			}
			for( unsigned p = 0; p < cur->position.size(); ++p )
			{
				std::normal_distribution<double> ndist(0.0f, initRanges[p] );
				cur->position[p] += ndist(g);
			}
		}
		cur->moved = true;
		cur->error = -1.0;
	}

	agentsPtr = &agents;
	searchDim = initPos.size();
}

int SDS::Optimiser::StepOpt()
{
	vector<Agent*> &agents = *agentsPtr;

	// get the agent's errors
	#pragma omp parallel for
	for( unsigned ac = 0; ac < agents.size(); ++ac)
	{
		if( agents[ac]->moved )
			agents[ac]->EvaluatePosition();
	}

	for(unsigned ac = 0; ac < agents.size(); ++ac )
	{
		if( agents[ac]->error < bestError )
		{
			bestAgent = ac;
			bestError = agents[ac]->error;
		}
		agents[ac]->moved = false;
	}


	// distribute the agents
	range.clear();
	range.assign( searchDim, 0.0 );
	for( unsigned ac = 0; ac < agents.size(); ++ac)
	{
		if( ac == bestAgent)
			continue;

		// pick a random other agent that hasn't already moved
		int oc = rand()%agents.size();
		while( agents[oc]->moved == true )
			oc = rand()%agents.size();

		// move nearer to it if appropriate.
		if( agents[oc]->error < agents[ac]->error )
		{
			for( unsigned pc = 0; pc < searchDim; ++pc)
			{
				double d = fabs(agents[ac]->position[pc] - agents[oc]->position[pc]);
				double m = (SDS::Rand01() * d * 2.0 ) - d;
				agents[ac]->position[pc] = agents[oc]->position[pc] + m;
				range[pc] = std::max(d, range[pc] );
			}
			agents[ac]->moved = true;
		}
	}

	++iterCount;
	return bestAgent;
}

bool SDS::Optimiser::CheckTerm()
{
	// check for convergence
	bool converged = false;

	rangeNorm = 0.0;
	for( unsigned rc = 0; rc < range.size(); ++rc)
	{
		rangeNorm += range[rc]*range[rc];
	}
	if( rangeNorm < termRangeThresh )
		converged = true;
	else
		converged = false;

	// quit after too many iterations, even if we've not converged.
	if( termMaxIters > 0 && iterCount >= termMaxIters )
		converged = true;

	return converged;
}

int SDS::Optimiser::Optimise(vector<double> &initPos, vector<double> &initRanges, vector<Agent*> &agents, double rangeThresh )
{
	return Optimise( initPos, initRanges, agents, rangeThresh, -1 );
}

int SDS::Optimiser::Optimise(vector<double> &initPos, vector<double> &initRanges, vector<Agent*> &agents, double rangeThresh, int maxIterations )
{
	InitialiseOpt(initPos, initRanges, agents, rangeThresh, maxIterations);

	// start the optimisation loop
	bool converged = false;
	while(!converged )
	{
		StepOpt();
		converged = CheckTerm();

// 		cout << "err: " << bestError << " " << sqrt(rangeNorm) << " " << iterCount << endl;
// 		cout << "ranges: " << endl;
// 		for( int i = 0; i < range.size(); ++i)
// 		{
// 			cout << "\t" << range[i] << endl;
// 		}
// 		cout << endl;

		if(converged)
		{
			for( unsigned i = 0; i < range.size(); ++i)
			{
				initRanges[i] = sqrt(range[i]);
			}
		}
	}
	return bestAgent;

}
