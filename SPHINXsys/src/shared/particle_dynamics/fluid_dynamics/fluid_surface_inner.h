/* -------------------------------------------------------------------------*
 *								SPHinXsys									*
 * --------------------------------------------------------------------------*
 * SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle	*
 * Hydrodynamics for industrial compleX systems. It provides C++ APIs for	*
 * physical accurate simulation and aims to model coupled industrial dynamic *
 * systems including fluid, solid, multi-body dynamics and beyond with SPH	*
 * (smoothed particle hydrodynamics), a meshless computational method using	*
 * particle discretization.													*
 *																			*
 * SPHinXsys is partially funded by German Research Foundation				*
 * (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1				*
 * and HU1527/12-1.															*
 *                                                                           *
 * Portions copyright (c) 2017-2020 Technical University of Munich and		*
 * the authors' affiliations.												*
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may   *
 * not use this file except in compliance with the License. You may obtain a *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.        *
 *                                                                           *
 * --------------------------------------------------------------------------*/
/**
 * @file 	fluid_surface_inner.h
 * @brief 	Here, we define the algorithm classes for fluid surfaces.
 * @details 	Fluid indicators are mainly used here to classify different region in a fluid.
 * @author	Chi Zhang and Xiangyu Hu
 */

#ifndef FLUID_SURFACE_INNER_H
#define FLUID_SURFACE_INNER_H

#include "fluid_dynamics_inner.hpp"

namespace SPH
{
	namespace fluid_dynamics
	{
		/**
		 * @class FreeSurfaceIndicationInner
		 * @brief  indicate the particles near the free surface of a fluid body.
		 * Note that, SPHinXsys does not require this function for simulating general free surface flow problems.
		 * However, some other applications may use this function, such as transport velocity formulation,
		 * for masking some function which is only applicable for the bulk of the fluid body.
		 */
		class FreeSurfaceIndicationInner : public LocalDynamics, public FluidDataInner
		{
		public:
			explicit FreeSurfaceIndicationInner(BaseInnerRelation &inner_relation, Real threshold = 0.75);
			virtual ~FreeSurfaceIndicationInner(){};
			void interaction(size_t index_i, Real dt = 0.0);
			void update(size_t index_i, Real dt = 0.0);

		protected:
			Real threshold_by_dimensions_;
			StdLargeVec<int> &surface_indicator_;
			StdLargeVec<Real> pos_div_;
			Real smoothing_length_;
		};

		/**
		 * @class SpatialTemporalFreeSurfaceIdentification
		 * @brief using the spatial-temporal method to indicate the surface particles to avoid mis-judgement.
		 */
		template <class FreeSurfaceIdentification>
		class SpatialTemporalFreeSurfaceIdentification : public FreeSurfaceIdentification
		{
		public:
			template <typename... ConstructorArgs>
			explicit SpatialTemporalFreeSurfaceIdentification(ConstructorArgs &&...args);
			virtual ~SpatialTemporalFreeSurfaceIdentification(){};
			void interaction(size_t index_i, Real dt = 0.0);
			void update(size_t index_i, Real dt = 0.0);

		protected:
			StdLargeVec<int> previous_surface_indicator_;
			void checkNearPreviousFreeSurface(size_t index_i);
		};
		using SpatialTemporalFreeSurfaceIdentificationInner =
			SpatialTemporalFreeSurfaceIdentification<FreeSurfaceIndicationInner>;

		/**
		 * @class DensitySummationFreeSurfaceInner
		 * @brief computing density by summation with a re-normalization for free surface flows
		 */
		class DensitySummationFreeSurfaceInner : public DensitySummationInner
		{
		public:
			explicit DensitySummationFreeSurfaceInner(BaseInnerRelation &inner_relation)
				: DensitySummationInner(inner_relation){};
			virtual ~DensitySummationFreeSurfaceInner(){};

		protected:
			virtual Real ReinitializedDensity(Real rho_sum, Real rho_0, Real rho_n) override
			{
				return rho_sum + SMAX(0.0, (rho_n - rho_sum)) * rho_0 / rho_n;
			};
		};

		/**
		 * @class DensitySummationFreeStreamInner
		 * @brief The density is smoothed if the particle is near fluid surface.
		 */
		class DensitySummationFreeStreamInner : public DensitySummationFreeSurfaceInner
		{
		public:
			explicit DensitySummationFreeStreamInner(BaseInnerRelation &inner_relation)
				: DensitySummationFreeSurfaceInner(inner_relation),
				  surface_indicator_(*particles_->getVariableByName<int>("SurfaceIndicator")){};
			virtual ~DensitySummationFreeStreamInner(){};
			void update(size_t index_i, Real dt = 0.0);

		protected:
			StdLargeVec<int> &surface_indicator_;
			bool isNearSurface(size_t index_i);
		};

		/**
		 * @class FreeStreamBoundaryVelocityCorrection
		 * @brief this function is applied to freestream flows TODO: revise for general freestream condition
		 * @brief modify the velocity of free surface particles with far-field velocity
		 */
		class FreeStreamBoundaryVelocityCorrection : public LocalDynamics, public FluidDataSimple
		{
		protected:
			Real u_ref_, t_ref_, rho_ref_;
			StdLargeVec<Real> &rho_sum;
			StdLargeVec<Vecd> &vel_;
			StdLargeVec<int> &surface_indicator_;

		public:
			explicit FreeStreamBoundaryVelocityCorrection(SPHBody &sph_body);
			virtual ~FreeStreamBoundaryVelocityCorrection(){};

			void update(size_t index_i, Real dt = 0.0);
		};

		/**
		 * @class FreeSurfaceHeight
		 * @brief Probe the free surface profile for a fluid body part by reduced operation.
		 * TODO: this class can be generalized
		 */
		class FreeSurfaceHeight : public LocalDynamicsReduce<Real, ReduceMax>,
								  public FluidDataSimple
		{
		protected:
			StdLargeVec<Vecd> &pos_;

		public:
			FreeSurfaceHeight(BodyPartByCell &body_part)
				: LocalDynamicsReduce<Real, ReduceMax>(body_part.getSPHBody(), Real(MinRealNumber)),
				  FluidDataSimple(sph_body_), pos_(particles_->pos_)
			{
				quantity_name_ = "FreeSurfaceHeight";
			}
			virtual ~FreeSurfaceHeight(){};

			Real reduce(size_t index_i, Real dt = 0.0) { return pos_[index_i][1]; };
		};
		/**
		 * @class ColorFunctionGradientInner
		 * @brief  indicate the particles near the interface of a fluid-fluid interaction and computing norm
		 */
		class ColorFunctionGradientInner : public LocalDynamics, public FluidDataInner
		{
		public:
			explicit ColorFunctionGradientInner(BaseInnerRelation &inner_relation);
			virtual ~ColorFunctionGradientInner(){};
			void interaction(size_t index_i, Real dt = 0.0);

		protected:
			Real threshold_by_dimensions_;
			StdLargeVec<int> &surface_indicator_;
			StdLargeVec<Vecd> color_grad_;
			StdLargeVec<Vecd> surface_norm_;
			StdLargeVec<Real> &pos_div_;
		};

		/**
		 * @class ColorFunctionGradientInterpolationInner
		 * @brief
		 */
		class ColorFunctionGradientInterpolationInner : public LocalDynamics, public FluidDataInner
		{
		public:
			explicit ColorFunctionGradientInterpolationInner(BaseInnerRelation &inner_relation);
			virtual ~ColorFunctionGradientInterpolationInner(){};
			void interaction(size_t index_i, Real dt = 0.0);

		protected:
			Real threshold_by_dimensions_;
			StdLargeVec<Real> &Vol_;
			StdLargeVec<int> &surface_indicator_;
			StdLargeVec<Vecd> &color_grad_;
			StdLargeVec<Vecd> &surface_norm_;
			StdLargeVec<Real> &pos_div_;
		};

		/**
		 * @class SurfaceTensionAccelerationInner
		 * @brief  the surface force induced acceleration
		 */
		class SurfaceTensionAccelerationInner : public LocalDynamics, public FluidDataInner
		{
		public:
			SurfaceTensionAccelerationInner(BaseInnerRelation &inner_relation, Real gamma);
			explicit SurfaceTensionAccelerationInner(BaseInnerRelation &inner_relation);
			virtual ~SurfaceTensionAccelerationInner(){};
			void interaction(size_t index_i, Real dt = 0.0);

		protected:
			Real gamma_;
			StdLargeVec<Real> &Vol_, &mass_;
			StdLargeVec<Vecd> &acc_prior_;
			StdLargeVec<int> &surface_indicator_;
			StdLargeVec<Vecd> &color_grad_;
			StdLargeVec<Vecd> &surface_norm_;
		};
	}
}
#endif // FLUID_SURFACE_INNER_H