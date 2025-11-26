// class to encapsulate SDS optimisation
// Murray Evans, 2009
//
// 2014: Updated slightly to aid my work at SLE.

#ifndef SDS_OPT
#define SDS_OPT

#include <vector>
using std::vector;

#include <cmath>

namespace SDS
{
	double Rand01();

	class Agent
	{
		public:
			Agent();
			virtual ~Agent();

			vector<double> position;
			double error;
			bool moved;

			virtual double EvaluatePosition()=0;
	};

	class Optimiser
	{
		public:
			Optimiser();
			~Optimiser();

			// returns the index of the agent with the best score
			int Optimise(vector<double> &initPos, vector<double> &initRanges, vector<Agent*> &agents, double rangeThresh, int maxIterations);
			int Optimise(vector<double> &initPos, vector<double> &initRanges, vector<Agent*> &agents, double rangeThresh);


			// alternatively, you can step the optimisation...
			void InitialiseOpt(vector<double> &initPos, vector<double> &initRanges, vector<Agent*> &agents, double rangeThresh, int maxIterations);
			int StepOpt();
			bool CheckTerm();

			double GetError()
			{
				return bestError;
			}
			double GetRange()
			{
				return sqrt(rangeNorm);
			}
			unsigned GetIterCount()
			{
				return iterCount;
			}

		private:

			double termRangeThresh;
			double termMaxIters;

			double bestError;
			unsigned bestAgent;
			unsigned iterCount;
			vector<double> range;
			double rangeNorm;

			vector<Agent*> *agentsPtr;

			unsigned searchDim;

	};

};

#endif
