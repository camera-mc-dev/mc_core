#include <Eigen/Dense>
#include <opencv2/core/eigen.hpp>

//#include "math/mathTypes.h"
#include "math/matrixGenerators.h"

float rigidTransform3D (Eigen::Matrix<float, Eigen::Dynamic, 4> A, Eigen::Matrix<float,Eigen::Dynamic,4> B, Eigen::Matrix<float,1,4> &t, Eigen::Matrix<float,4,4> &R, bool calculateError /* = false */)
{
	if (A.rows() != B.rows())
	{
		cout << "ERROR: Sizes of sets does not match" << endl;
		return -1.0;
	}
	
	//cout << A << endl;
	//cout << B << endl;
	
	unsigned int N = A.rows();

	Eigen::Matrix<float,1,4> centroid_A = A.colwise().mean();
	Eigen::Matrix<float,1,4> centroid_B = B.colwise().mean();

	Eigen::Matrix<float, Eigen::Dynamic, 4> centered_A = A;
	Eigen::Matrix<float, Eigen::Dynamic, 4> centered_B = B;
	
	// centre the points
	for (unsigned int i = 0; i < N; i++)
	{
		centered_A.row(i) -= centroid_A;
		centered_B.row(i) -= centroid_B;
	}

	//cout << centered_A << endl;
	//cout << centered_B << endl;
	
	// Calculate H matrix
	Eigen::Matrix<float, 4, Eigen::Dynamic> H = centered_A.transpose() * centered_B;

	// Single Value Decomposition
	Eigen::JacobiSVD<Eigen::MatrixXf> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
	
	// Calculate Rotation Matrix
	R = svd.matrixV() * svd.matrixU().transpose();
	
	// special reflection case
	if (R.determinant() < 0)
	{
		cout << "Reflection detected" << endl;
		Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> V = svd.matrixV();
		V.col(2) *= -1;
		R = V * svd.matrixU().transpose();
	}
	
	//Calculate final translation vector
	t = -1 * R * centroid_A.transpose() + centroid_B.transpose();

	// Calculate Error
	float error = 0.0;

	if (calculateError)
	{
		Eigen::Matrix<float, Eigen::Dynamic, 4> newA;
		newA = A;

		for (unsigned int i = 0; i < A.rows(); i++)
		{
			newA.row(i) = A.row(i) * R + t;
		}
		newA -= B;

		//cout << A << endl;
		//cout << B << endl;
		//cout << newA << endl;


		newA = newA.array().square();
		error = newA.sum();
		error = sqrt(error);
	}

	return error;
}


float rigidRotation3D (Eigen::Matrix<float, Eigen::Dynamic, 4> A, Eigen::Matrix<float,Eigen::Dynamic,4> B, Eigen::Matrix<float,4,4> &R, bool calculateError /* = false */)
{
	if (A.rows() != B.rows())
	{
		cout << "ERROR: Sizes of sets does not match" << endl;
		return -1.0;
	}
	
	//cout << A << endl;
	//cout << B << endl;
	
	unsigned int N = A.rows();

	// Calculate H matrix
	Eigen::Matrix<float, 4, Eigen::Dynamic> H = A.transpose() * B;

	// Single Value Decomposition
	Eigen::JacobiSVD<Eigen::MatrixXf> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
	
	// Calculate Rotation Matrix
	R = svd.matrixV() * svd.matrixU().transpose();
	
	// special reflection case
	if (R.determinant() < 0)
	{
		cout << "Reflection detected" << endl;
		Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> V = svd.matrixV();
		V.col(2) *= -1;
		R = V * svd.matrixU().transpose();
	}
	
	// Calculate Error
	float error = 0.0;

	if (calculateError)
	{
		Eigen::Matrix<float, Eigen::Dynamic, 4> newA;
		newA = A;

		for (unsigned int i = 0; i < A.rows(); i++)
		{
			newA.row(i) = A.row(i) * R;
		}
		newA -= B;

		//cout << A << endl;
		//cout << B << endl;
		//cout << newA << endl;


		newA = newA.array().square();
		error = newA.sum();
		error = sqrt(error);
	}

	return error;
}
