// We've started to encounter problems with using SBA. It seems really really late in the
// day for these problems to appear, consider the same wrappers and calls have been quite
// happily functional for more than a year now. But digging down, SBA is the root of some
// problems I'm having.
//
// It should not be that hard, with the help of Ceres, to write a Bundle Adjustment class,
// however, that means we need versions of the calibration class methods that will work
// with Ceres. i.e. templated.
//
// I don't like having multiple bits of code to do the same thing, but in this instance,
// I'd rather leave the existing calibration code alone until I've tried out this test.
//
// We can worry about neater integration later.

#ifndef ME_CALIBRATION_C
#define ME_CALIBRATION_C

#include "math/mathTypes.h"

#ifndef HAVE_CERES
namespace ceres
{
	template <class T>
	void AngleAxisToRotationMatrix(T *a, T *b)
	{
		throw std::runtime_error("Not compiled with ceres, don't try to run calibration stuff.");
	}

	template <class T>
	void RotationMatrixToAngleAxis(T *a, T *b)
	{
		throw std::runtime_error("Not compiled with ceres, don't try to run calibration stuff.");
	}
}
#else
	#include <ceres/ceres.h>
	#include <ceres/rotation.h>
#endif
#include <vector>


// this is hardly good, but I feel forced into this shit.
#define BA_PARAMID_f  0 
#define BA_PARAMID_cx 1
#define BA_PARAMID_cy 2
#define BA_PARAMID_a  3
#define BA_PARAMID_s  4

#define BA_PARAMID_raxx 5
#define BA_PARAMID_raxy 6
#define BA_PARAMID_raxz 7

#define BA_PARAMID_tx 8
#define BA_PARAMID_ty 9
#define BA_PARAMID_tz 10

#define BA_PARAMID_d0 11
#define BA_PARAMID_d1 12
#define BA_PARAMID_d2 13
#define BA_PARAMID_d3 14
#define BA_PARAMID_d4 15



// basically, all we really need are some helpers, and some error functions
namespace CalibCeres
{

// This is the basic projection from 3D point to 2D point
// given intrinsic matrix (K)
// extrinsic matrix (L)
// distortion params (k)
// and the 3D point (p3d)
// This is basically a copy of what is in the calibration class,
// all merged into one function.
template <typename T>
void Project(
           Eigen::Matrix<T,3,3> &K,
           Eigen::Matrix<T,4,4> &L,
           Eigen::Matrix<T,5,1> &k,
           Eigen::Matrix<T,4,1> &p3d,
           Eigen::Matrix<T,3,1> &p2d            
         )
{
	// first, project to normalised coords
	Eigen::Matrix<T,3,4> P;
	P << T(1), T(0), T(0), T(0),
         T(0), T(1), T(0), T(0),
         T(0), T(0), T(1), T(0);
	
	Eigen::Matrix<T,3,1> pn;
	pn = P * L * p3d;
	pn = pn / pn(2);

	// handle distortion
	Eigen::Matrix<T,3,1> dc;
	dc << T(0), T(0), T(1);

	Eigen::Matrix<T,3,1> rx;
	rx = pn - dc;

	T r2;
	r2 = rx(0)*rx(0) + rx(1)*rx(1);

	T v;
	v = T(1) + k(0)*r2 + k(1)*(r2*r2) + k(4)*(r2*r2*r2);

	Eigen::Matrix<T,3,1> dx;
	dx << T(0), T(0), T(0);
	dx(0) = T(2)*k(2)*rx(0)*rx(1) + k(3)*(r2 + T(2)*rx(0)*rx(0));
	dx(1) = k(2)*(r2 + T(2)*rx(1)*rx(1)) + T(2)*k(3)*rx(0)*rx(1);

	p2d = dc + v*rx + dx;
	
	p2d = K * p2d;
}


// To get the most out of Ceres, we need various different cost functions.
// This is annoying, but using the dynamicAutoDiff was not getting the sparsity
// advantage and was extremely slow.

// the base functor, because they all need the same constructor.
class CeresFunctor_BA_base
{
public:
	CeresFunctor_BA_base(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    )
	{
		obs2d = in_obs2d;
		initK = in_initK;
		initL = in_initL;
		for( unsigned c = 0; c < 5; ++c )
		{
			initk(c) = in_initk(c);
		}
		initp3d = in_initp3d;
	}
	
	hVec2D obs2d;
	transMatrix2D initK;
	transMatrix3D initL;
	Eigen::Matrix<double,5,1> initk;

	
	// if the points are fixed, then we also need to know
	// the point this functor is working on.
	hVec3D initp3d;
	
};


// So, first off, we need a cost function for a fixed camera, changing point.
class CeresFunctor_FixedCamera_BA : CeresFunctor_BA_base
{
public:
	
	CeresFunctor_FixedCamera_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	
	template <typename T>
	bool operator()( T const* point, T* errors ) const
	{
		// this is the easiest one as we just use the already input
		// camera paramters.
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		LT = initL.cast<T>();
		kT = initk.cast<T>();
		
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}
	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID      )
	{
		// not worth the function really...
		params.clear();
		paramID.clear();
		return;
	}
};


// we also need one or two for fixed points, changing cameras. If the points are 
// fixed, let us assume we're going to adjust extrinsics and focal length.
// focal length
class CeresFunctor_FixedPExtOnly_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FixedPExtOnly_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	
	template <typename T>
	bool operator()( T const* params, T* errors ) const
	{
		// most of the stuff still comes from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		unsigned pc = 0;
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		
		// and the rest is easy.
		p3d = initp3d.cast<T>();
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}

	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		return;
	}
	
};


// we also need one or two for fixed points, changing cameras. If the points are 
// fixed, let us assume we're going to adjust extrinsics and focal length.
// focal length
class CeresFunctor_FixedPFocal_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FixedPFocal_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	
	template <typename T>
	bool operator()( T const* params, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		unsigned pc = 0;
		T a = KT(1,1) / KT(0,0);
		
		KT(0,0) = params[pc++];
		KT(1,1) = a * KT(0,0);
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		
		// and the rest is easy.
		p3d = initp3d.cast<T>();
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}

	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );       
		
		paramID.push_back( BA_PARAMID_f );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		return;
	}
	
};


// now there are various different cost functions depending on how many 
// camera parameters are allowed to change.


// First off, extrinsic only.
class CeresFunctor_ExtrinsicOnly_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_ExtrinsicOnly_BA(
	                                hVec2D in_obs2d,
	                                transMatrix2D &in_initK,
	                                transMatrix3D &in_initL,
	                                Eigen::Matrix<float,5,1> &in_initk,
	                                hVec3D in_initp3d = hVec3D::Zero()
	                             ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		// but now we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[0];
		ax[1] = params[1];
		ax[2] = params[2];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[3];
		LT(1,3) = params[4];
		LT(2,3) = params[5];
		
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}

	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		return;
	}
	
	
};


// focal length
class CeresFunctor_Focal_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_Focal_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		unsigned pc = 0;
		T a = KT(1,1) / KT(0,0);
		
		KT(0,0) = params[pc++];
		KT(1,1) = a * KT(0,0);
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}

	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		
		paramID.push_back( BA_PARAMID_f );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		return;
	}
	
};


// focal and principal
class CeresFunctor_FocalPrinciple_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FocalPrinciple_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		
		// stuff for K is first.
		unsigned pc = 0;
		T a = KT(1,1) / KT(0,0);
		
		KT(0,0) = params[pc++];
		KT(1,1) = a * KT(0,0);
		KT(0,2) = params[pc++];
		KT(1,2) = params[pc++];
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}
	
	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		params.push_back( K(0,2) );
		params.push_back( K(1,2) );
		
		paramID.push_back( BA_PARAMID_f );
		paramID.push_back( BA_PARAMID_cx );
		paramID.push_back( BA_PARAMID_cy );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		return;
	}
};


// focal, principal, first order distortion
class CeresFunctor_FocalPrinciple1Dist_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FocalPrinciple1Dist_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		
		// stuff for K is first.
		unsigned pc = 0;
		T a = KT(1,1) / KT(0,0);
		
		KT(0,0) = params[pc++];
		KT(1,1) = a * KT(0,0);
		KT(0,2) = params[pc++];
		KT(1,2) = params[pc++];
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		// and after the extrinsics comes the distortions.
		kT(0)   = params[pc++];
		
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}
	
	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		params.push_back( K(0,2) );
		params.push_back( K(1,2) );
		
		paramID.push_back( BA_PARAMID_f );
		paramID.push_back( BA_PARAMID_cx );
		paramID.push_back( BA_PARAMID_cy );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		
		params.push_back( k(0) );
		
		paramID.push_back( BA_PARAMID_d0 );

		
		return;
	}
	
	
};

// focal, principal, first & second order distortion
class CeresFunctor_FocalPrinciple2Dist_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FocalPrinciple2Dist_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		
		// stuff for K is first.
		unsigned pc = 0;
		T a = KT(1,1) / KT(0,0);
		
		KT(0,0) = params[pc++];
		KT(1,1) = a * KT(0,0);
		KT(0,2) = params[pc++];
		KT(1,2) = params[pc++];
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		// and after the extrinsics comes the distortions.
		kT(0)   = params[pc++];
		kT(1)   = params[pc++];
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}
	
	
	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		params.push_back( K(0,2) );
		params.push_back( K(1,2) );
		
		paramID.push_back( BA_PARAMID_f );
		paramID.push_back( BA_PARAMID_cx );
		paramID.push_back( BA_PARAMID_cy );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		
		params.push_back( k(0) );
		params.push_back( k(1) );
		
		paramID.push_back( BA_PARAMID_d0 );
		paramID.push_back( BA_PARAMID_d1 );

		
		return;
	}
	
	
	
	
};


// focal, principal, first & second order + tangential distortion
class CeresFunctor_FocalPrinciple4Dist_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FocalPrinciple4Dist_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		
		// stuff for K is first.
		unsigned pc = 0;
		T a = KT(1,1) / KT(0,0);
		
		KT(0,0) = params[pc++];
		KT(1,1) = a * KT(0,0);
		KT(0,2) = params[pc++];
		KT(1,2) = params[pc++];
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		// and after the extrinsics comes the distortions.
		kT(0)   = params[pc++];
		kT(1)   = params[pc++];
		kT(2)   = params[pc++];
		kT(3)   = params[pc++];
		
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}

	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		params.push_back( K(0,2) );
		params.push_back( K(1,2) );
		
		paramID.push_back( BA_PARAMID_f );
		paramID.push_back( BA_PARAMID_cx );
		paramID.push_back( BA_PARAMID_cy );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		
		params.push_back( k(0) );
		params.push_back( k(1) );
		params.push_back( k(2) );
		params.push_back( k(3) );
		
		paramID.push_back( BA_PARAMID_d0 );
		paramID.push_back( BA_PARAMID_d1 );
		paramID.push_back( BA_PARAMID_d2 );
		paramID.push_back( BA_PARAMID_d3 );
		
		return;
	}
	
};


// full K
class CeresFunctor_FullK_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FullK_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		// most of the stuff still come from internal...
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		
		// stuff for K is first.
		unsigned pc = 0;
		T f  = params[pc++];
		T cx = params[pc++];
		T cy = params[pc++];
		T a  = params[pc++];
		T s  = params[pc++];
		
		KT << f, s, cx,        T(0), a*f, cy,         T(0),T(0),T(1);
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}

	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		params.push_back( K(0,2) );
		params.push_back( K(1,2) );
		params.push_back( K(1,1) / K(0,0) );
		params.push_back( K(0,1) );
		
		paramID.push_back( BA_PARAMID_f );
		paramID.push_back( BA_PARAMID_cx );
		paramID.push_back( BA_PARAMID_cy );
		paramID.push_back( BA_PARAMID_a );
		paramID.push_back( BA_PARAMID_s );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		return;
	}
	
	
	
};

// full K + full distortion
class CeresFunctor_FullKk_BA : CeresFunctor_BA_base
{
public:
	CeresFunctor_FullKk_BA(
	                     hVec2D in_obs2d,
	                     transMatrix2D &in_initK,
	                     transMatrix3D &in_initL,
	                     Eigen::Matrix<float,5,1> &in_initk,
	                     hVec3D in_initp3d = hVec3D::Zero()
	                    ) : CeresFunctor_BA_base( in_obs2d, in_initK, in_initL, in_initk, in_initp3d ) {}
	template <typename T>
	bool operator()( T const* params, T const* point, T* errors ) const
	{
		Eigen::Matrix<T,3,3> KT;
		Eigen::Matrix<T,4,4> LT;
		Eigen::Matrix<T,5,1> kT;
		Eigen::Matrix<T,4,1> p3d;
		Eigen::Matrix<T,3,1> p2d;
		
		KT = initK.cast<T>();
		kT = initk.cast<T>();
		
		
		// stuff for K is first.
		unsigned pc = 0;
		T f  = params[pc++];
		T cx = params[pc++];
		T cy = params[pc++];
		T a  = params[pc++];
		T s  = params[pc++];
		
		KT << f, s, cx,        T(0), a*f, cy,         T(0),T(0),T(1);
		
		
		// we need LT
		LT = Eigen::Matrix<T,4,4>::Identity();
		
		T ax[3];
		ax[0] = params[pc++];
		ax[1] = params[pc++];
		ax[2] = params[pc++];
		
		std::vector<T> R33(9);

		Eigen::Matrix<T, 3, 3> R = Eigen::Matrix<T,3,3>::Identity();
		ceres::AngleAxisToRotationMatrix( &ax[0], &R33[0] );
		R << R33[0], R33[3], R33[6],
		     R33[1], R33[4], R33[7],
		     R33[2], R33[5], R33[8];
		
		LT.block(0,0,3,3) = R;
		LT(0,3) = params[pc++];
		LT(1,3) = params[pc++];
		LT(2,3) = params[pc++];
		
		// and after the extrinsics comes the distortions.
		kT(0)   = params[pc++];
		kT(1)   = params[pc++];
		kT(2)   = params[pc++];
		kT(3)   = params[pc++];
		kT(4)   = params[pc++];
		
		
		// and the rest is easy.
		p3d << point[0], point[1], point[2], T(1.0);
		
		Project( KT, LT, kT, p3d, p2d );
		
		Eigen::Matrix<T,3,1> obs2dT;
		obs2dT = obs2d.cast<T>();
		
		Eigen::Matrix<T,3,1> e;
		e = p2d - obs2dT;
		
		errors[0] = e(0);
		errors[1] = e(1);
		
		return true;
	}
	
	
	void ThingsToParams( Eigen::Matrix<float,3,3> &K,
	                     Eigen::Matrix<float,4,4> &L,
	                     Eigen::Matrix<float,5,1> &k,
	                     std::vector<double> &params,
	                     std::vector<int>    &paramID       )
	{
		params.clear();
		
		params.push_back( K(0,0) );
		params.push_back( K(0,2) );
		params.push_back( K(1,2) );
		params.push_back( K(1,1) / K(0,0) );
		params.push_back( K(0,1) );
		
		paramID.push_back( BA_PARAMID_f );
		paramID.push_back( BA_PARAMID_cx );
		paramID.push_back( BA_PARAMID_cy );
		paramID.push_back( BA_PARAMID_a );
		paramID.push_back( BA_PARAMID_s );
		
		std::vector<double> R(9);
		unsigned i = 0;
		for( unsigned c = 0; c < 3; ++c )
			for( unsigned r = 0; r < 3; ++r )
			{
				R[i] = L(r,c);
				++i;
			}
		
		double ax[3];
		ceres::RotationMatrixToAngleAxis(&R[0], ax);
		
		params.push_back(ax[0]);     paramID.push_back( BA_PARAMID_raxx );
		params.push_back(ax[1]);     paramID.push_back( BA_PARAMID_raxy );
		params.push_back(ax[2]);     paramID.push_back( BA_PARAMID_raxz );
		
		params.push_back( L(0,3) );     paramID.push_back( BA_PARAMID_tx );
		params.push_back( L(1,3) );     paramID.push_back( BA_PARAMID_ty );
		params.push_back( L(2,3) );     paramID.push_back( BA_PARAMID_tz );
		
		for( unsigned c = 0; c < 5; ++c )
			params.push_back( k(c) );
		
		paramID.push_back( BA_PARAMID_d0 );
		paramID.push_back( BA_PARAMID_d1 );
		paramID.push_back( BA_PARAMID_d2 );
		paramID.push_back( BA_PARAMID_d3 );
		paramID.push_back( BA_PARAMID_d4 );
		
		return;
	}
	
};
	
};	// namespace




#endif
