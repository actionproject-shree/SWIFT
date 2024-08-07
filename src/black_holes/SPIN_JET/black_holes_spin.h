/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2022 Filip Husko (filip.husko@durham.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#ifndef SWIFT_SPIN_JET_BLACK_HOLES_SPIN_H
#define SWIFT_SPIN_JET_BLACK_HOLES_SPIN_H

/* Standard headers */
#include <float.h>

/* Local includes */
#include "black_holes_properties.h"
#include "black_holes_struct.h"
#include "hydro_properties.h"
#include "inline.h"
#include "physical_constants.h"

/**
 * @brief Compute the gravitational radius of a black hole.
 *
 * @param a Black hole mass.
 * @param constants Physical constants (in internal units).
 */
__attribute__((always_inline)) INLINE static float
black_hole_gravitational_radius(float mass,
                                const struct phys_const* constants) {

  const float r_G =
      mass * constants->const_newton_G /
      (constants->const_speed_light_c * constants->const_speed_light_c);

#ifdef SWIFT_DEBUG_CHECKS
  if (r_G <= 0.f) {
    error(
        "Something went wrong with calculation of R_G of black holes. "
        " R_G is %f instead of R_G > 0.",
        r_G);
  }
#endif

  return r_G;
}

/**
 * @brief Compute the radius of the horizon of a BH particle in gravitational
 * units.
 *
 * @param a Black hole spin, -1 < a < 1.
 */
__attribute__((always_inline)) INLINE static float black_hole_horizon_radius(
    float a) {
  return 1.f + sqrtf((1.f - a) * (1.f + a));
}

/**
 * @brief Compute the radius of the innermost stable circular orbit of a
 * BH particle in gravitational units.
 *
 * The expression is given in Appendix B of Fiacconi et al. (2018) or eqn. 4 in
 * Griffin et al. (2019).
 *
 * @param a Black hole spin, -1 < a < 1.
 */
__attribute__((always_inline)) INLINE static float black_hole_isco_radius(
    float a) {
  const float Z1 = 1.f + (cbrtf((1.f + fabsf(a)) * (1.f - a * a)) +
                          cbrtf((1.f - fabsf(a)) * (1.f - a * a)));
  const float Z2 = sqrtf(3.f * a * a + Z1 * Z1);

  const float R_ISCO =
      3. + Z2 - a / fabsf(a) * sqrtf((3.f - Z1) * (3.f + Z1 + 2.f * Z2));

#ifdef SWIFT_DEBUG_CHECKS
  if (Z1 > 3.f) {
    error(
        "Something went wrong with calculation of Z1 factor for r_isco of"
        " black holes. Z1 is %f instead of Z1 > 3.",
        Z1);
  }

  if ((3.f + Z1 + 2.f * Z2) < 0.f) {
    error(
        "Something went wrong with calculation of (3. + Z1 + 2. * Z2 ) "
        "factor for r_isco of black holes. (3. + Z1 + 2. * Z2 ) is %f instead "
        "of"
        " (3. + Z1 + 2. * Z2 ) > 0.",
        3.f + Z1 + 2.f * Z2);
  }

  if (R_ISCO < 1.f) {
    error(
        "Something went wrong with calculation of R_ISCO of black holes. "
        "R_ISCO is %f instead >= 1.",
        R_ISCO);
  }
#endif

  return R_ISCO;
}

/**
 * @brief Compute the magnitude of the angular momentum of the black hole
 * given its spin.
 *
 * @param a Black hole spin magnitude, 0 < a < 1.
 * @param constants Physical constants (in internal units).
 */
__attribute__((always_inline)) INLINE static float
black_hole_angular_momentum_magnitude(struct bpart* bp,
                                      const struct phys_const* constants) {

  const float J_BH =
      fabs(bp->subgrid_mass * bp->subgrid_mass * bp->spin *
           constants->const_newton_G / constants->const_speed_light_c);

#ifdef SWIFT_DEBUG_CHECKS
  if (J_BH <= 0.f) {
    error(
        "Something went wrong with calculation of j_BH of black holes. "
        " J_BH is %f instead of J_BH > 0.",
        J_BH);
  }
#endif

  return J_BH;
}

/**
 * @brief Compute the warp radius of a black hole particle.
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk). For the thick disk and slim disk, the radius is calculated
 * from Lubow et al. (2002), eqn. 22 with x=1. The result will be different
 * only due to different aspect ratios H/R=h_0.
 *
 * For the thin disk, the result depends on props->TD_region (B - region b from
 * Shakura & Sunyaev 1973, C - region c from Shakura & Sunyaev 1973). The warp
 * radii are taken as eqns. 11 from Griffin et al. (2019) and A8 from Fiacconi
 * et al. (2018), respectively.
 *
 * For the thin disk we also have to include the possibility that the self-
 * gravity radius is smaller than the warp radius. In this case r_warp=r_sg
 * because the disk cannot be larger than the self-gravity radius, and the
 * entire disk is warped. The sg radius is taken as eqns. 16 in Griffin et al.
 * (2019) and A6 in Fiacconi et al. (2018), respectively.
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float black_hole_warp_radius(
    struct bpart* bp, const struct phys_const* constants,
    const struct black_holes_props* props) {

  /* Define placeholder value for the result. We will assign the final result
     to this variable. */
  float Rw = -1.f;

  /* Gravitational radius */
  const float R_G =
      black_hole_gravitational_radius(bp->subgrid_mass, constants);

  /* Start branching depending on which accretion mode the BH is in */
  if (bp->accretion_mode == BH_thick_disc) {

    /* Eqn. 22 from Lubow et al. (2002) with H/R=h_0_ADAF (thick disk) */
    const float base = 15.36f * fabsf(bp->spin) / props->h_0_ADAF_2;
    Rw = R_G * powf(base, 0.4f);
  } else if (bp->accretion_mode == BH_slim_disc) {

    /* Eqn. 22 from Lubow et al. (2002) with H/R=1/gamma_SD (slim disk) */
    const float base = 15.36f * fabsf(bp->spin) * props->gamma_SD;
    Rw = R_G * powf(base, 0.4f);
  } else if (bp->accretion_mode == BH_thin_disc) {

    /* Start branching depending on which region of the thin disk we wish to
       base the model upon (TD_region=B: region b from Shakura & Sunyaev 1973,
       or TD_region=C: region c) */
    if (props->TD_region == TD_region_B) {

      /* Calculate different factors in eqn. 11 (Griffin et al. 2019) for warp
          radius of region b in Shakura & Sunyaev (1973) */
      float mass_factor =
          powf(bp->subgrid_mass / (1e8f * constants->const_solar_mass), 0.2f);
      float edd_factor = powf(bp->eddington_fraction, 0.4f);

      /* Gather the factors and finalize calculation */
      const float base = mass_factor * fabsf(bp->spin) /
                         (props->xi_TD * props->alpha_factor_08 * edd_factor);
      const float rw = 3410.f * 2.f * R_G * powf(base, 0.625f);

      /* Self-gravity radius in region b: eqn. 16 in Griffin et al. */
      mass_factor = powf(
          bp->subgrid_mass / (1e8f * constants->const_solar_mass), -0.961f);
      edd_factor = powf(bp->eddington_fraction, -0.353f);

      const float rs = 4790.f * 2.f * R_G * mass_factor *
                       props->alpha_factor_0549 * edd_factor;

      /* Take the minimum */
      Rw = min(rs, rw);
    }

    if (props->TD_region == TD_region_C) {

      /* Calculate different factors in eqn. A8 (Fiacconi et al. 2018) */
      float mass_factor =
          powf(bp->subgrid_mass / (1e6f * constants->const_solar_mass), 0.2f);
      float edd_factor = powf(bp->eddington_fraction, 0.3f);

      /* Gather the factors and finalize calculation */
      const float base = mass_factor * fabsf(bp->spin) /
                         (props->xi_TD * props->alpha_factor_02 * edd_factor);
      const float rw = 1553.f * 2.f * R_G * powf(base, 0.5714f);

      /* Repeat the same for self-gravity radius - eqn. A6 in F2018 */
      mass_factor = powf(
          bp->subgrid_mass / (1e6f * constants->const_solar_mass), -1.1556f);
      edd_factor = powf(bp->eddington_fraction, -0.48889f);

      const float rs = 1.2f * 100000.f * 2.f * R_G * mass_factor *
                       props->alpha_factor_06222 * edd_factor;

      /* Take the minimum */
      Rw = min(rs, rw);
    }
  }

#ifdef SWIFT_DEBUG_CHECKS
  if (Rw < 0.f) {
    error(
        "Something went wrong with calculation of Rw of black holes. "
        " Rw is %f instead of Rw >= 0.",
        Rw);
  }
#endif

  return Rw;
}

/**
 * @brief Compute the warp mass of a black hole particle.
 *
 * Calculated as the integral of the surface density of the disk up to R_warp.
 * The result again depends on type of accretion mode, both due to different
 * R_warp and different surface densities.
 *
 * The surface densities for the thick and slim disk take the same form
 * (eqn. 2.3 in Narayan & Yi 1995 for rho, and then sigma = rho * 2H =
 * dot(M_BH) / (2pi * R * abs(v_r))). They differ due to different radial
 * radial velocities in the disks: v_r = -alpha * v_0 * v_K (with v_K
 * the Keplerian velocity). These differences are encoded in the numerical
 * constant v_0, which depends on alpha in Narayan & Yi for the thick disk,
 * and is roughly constant for the slim disk (Wang & Zhou 1999).
 *
 * For the thin disk the surface densities are more complex, and again depend
 * on which region of the disk is chosen to be modelled (region b or c from
 * Shakura & Sunyaev 1973). Sigma for region b is given by eqn. 7 in Griffin
 * et al. (2019) and for region c, it is not given explicitly but can be
 * calculated based on Appendix A in Fiacconi et al. (2018).
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static double black_hole_warp_mass(
    struct bpart* bp, const struct phys_const* constants,
    const struct black_holes_props* props) {

  /* Define placeholder value for the result. We will assign the final result
     to this variable. */
  double Mw = -1.;

  /* Gravitational radius */
  const float R_G =
      black_hole_gravitational_radius(bp->subgrid_mass, constants);

  /* Start branching depending on which accretion mode the BH is in */
  if ((bp->accretion_mode == BH_thick_disc) ||
      (bp->accretion_mode == BH_slim_disc)) {

    /* Define v_0, the only factor which differs between thick and slim
       disc */
    float v_0;
    if (bp->accretion_mode == BH_thick_disc) {
      v_0 = props->v_0_ADAF;
    } else {
      v_0 = props->gamma_SD_inv;
    }

    /* Final result based on eqn. 2.3 in Narayan & Yi 1995*/
    Mw = 2. * bp->accretion_rate /
         (3. * props->alpha_acc * v_0 *
          sqrtf(bp->subgrid_mass * constants->const_newton_G)) *
         powf(black_hole_warp_radius(bp, constants, props), 1.5);
  } else {

    /* Start branching depending on which region of the thin disk we wish to
       base the model upon (TD_region=B: region b from Shakura & Sunyaev 1973,
       or TD_region=C: region c) */
    if (props->TD_region == TD_region_B) {

      /* Calculate different factors that appear in result for M_warp */
      const float mass_factor =
          powf(bp->subgrid_mass / (1e8 * constants->const_solar_mass), 2.2);
      const float edd_factor = powf(bp->eddington_fraction, 0.6);
      const float R_factor =
          powf(black_hole_warp_radius(bp, constants, props) / (2. * R_G), 1.4);

      /* Gather factors and finalize calculation */
      Mw = constants->const_solar_mass * 1.35 * mass_factor *
           props->alpha_factor_08_inv * edd_factor * R_factor;
    }
    if (props->TD_region == TD_region_C) {

      /* Same as above but for region c of disk */
      const float mass_factor =
          powf(bp->subgrid_mass / (1e6 * constants->const_solar_mass), 2.2);
      const float edd_factor = powf(bp->eddington_fraction, 0.7);
      const float R_factor =
          powf(black_hole_warp_radius(bp, constants, props) / (2. * R_G), 1.25);

      Mw = constants->const_solar_mass * 0.01 * mass_factor *
           props->alpha_factor_08_inv_10 * edd_factor * R_factor;
    }
  }

#ifdef SWIFT_DEBUG_CHECKS
  if (Mw < 0.) {
    error(
        "Something went wrong with calculation of Mw of black holes. "
        " Mw is %f instead of Mw >= 0.",
        Mw);
  }
#endif

  return Mw;
}

/**
 * @brief Compute the warp angular momentum of a black hole particle.
 *
 * Calculated as the integral of the surface density times the specific
 * angular momentum of the disk up to R_warp. The result depends on type
 * of accretion mode, due to different R_warp, surface densities and spec.
 * ang. momenta of the disks.
 *
 * The surface densities are the same as for M_warp. For the thin disk, the
 * spec. ang. mom. is L(R) = R * v_K(R), because orbits are perfectly circular.
 * For the thick and slim disk, this is replaced by L(R) = Omega_0 * R * v_K(R)
 * , with Omega_0 a numerical constant between 0 and 1 which encodes the fact
 * that rotation is slower in the two disks. The values for Omega_0 are given
 * in Narayan & Yi (1995) and Wang & Zhou (1999) for the thick and slim disk,
 * respectively.
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static double
black_hole_warp_angular_momentum(struct bpart* bp,
                                 const struct phys_const* constants,
                                 const struct black_holes_props* props) {

  /* Define placeholder value for the result. We will assign the final result
     to this variable. */
  double Jw = -1.;

  /* Start branching depending on which accretion mode the BH is in */
  if ((bp->accretion_mode == BH_thick_disc) ||
      (bp->accretion_mode == BH_slim_disc)) {

    /* Get numerical constants for radial and tangential velocities for the
       thick and slim disk, which factor into the surface density and spec.
       ang. mom., respectively */
    float v_0 = 0.;
    float omega_0 = 0.;
    if (bp->accretion_mode == BH_thick_disc) {
      v_0 = props->v_0_ADAF;
      omega_0 = props->omega_0_ADAF;
    } else {
      v_0 = props->gamma_SD_inv;
      omega_0 = props->gamma_SD_inv;
    }

    /* Gather factors for the final result  */
    const double r_warp = black_hole_warp_radius(bp, constants, props);
    Jw = 2. * bp->accretion_rate * omega_0 / (2. * props->alpha_acc * v_0) *
         r_warp * r_warp;
  } else {

    /* Start branching depending on which region of the thin disk we wish to
       base the model upon (TD_region=B: region b from Shakura & Sunyaev 1973,
       or TD_region=C: region c). The warp radius can generally be related to
       the warp mass and radius, if one assumes Keplerian rotation, with the
       following relation: J_warp = (c+2)/(c+5/2) * M_warp * sqrt(M_BH * G *
       R_warp), where c is the slope of the surface density profile: sigma~R^c.
       For region b, c=-3/5 (see Griffin et al. 2019), and for region c, c=-3/4
       (see Fiacconi et al. 2018). */
    if (props->TD_region == TD_region_B) {
      Jw = 0.737 * black_hole_warp_mass(bp, constants, props) *
           sqrtf(bp->subgrid_mass * constants->const_newton_G *
                 black_hole_warp_radius(bp, constants, props));
    }
    if (props->TD_region == TD_region_C) {
      Jw = 0.714 * black_hole_warp_mass(bp, constants, props) *
           sqrtf(bp->subgrid_mass * constants->const_newton_G *
                 black_hole_warp_radius(bp, constants, props));
    }
  }

#ifdef SWIFT_DEBUG_CHECKS
  if (Jw < 0.) {
    error(
        "Something went wrong with calculation of Jw of black holes. "
        " Jw is %f instead of Jw >= 0.",
        Jw);
  }
#endif

  return Jw;
}

/**
 * @brief Compute the spin-dependant radiative efficiency of a BH particle in
 * the radiatively efficient (thin disc) regime.
 *
 * This is eqn. 3 in Griffin et al. (2019), based on Novikov & Thorne (1973).
 *
 * @param a Black hole spin, -1 < a < 1.
 */
__attribute__((always_inline)) INLINE static float eps_Novikov_Thorne(float a) {

#ifdef SWIFT_DEBUG_CHECKS
  if (black_hole_isco_radius(a) <= 0.6667f) {
    error(
        "Something went wrong with calculation of eps_Novikov_Thorn of. "
        "black holes. r_isco is %f instead of r_isco > 1.",
        black_hole_isco_radius(a));
  }
#endif

  return 1. - sqrtf(1. - 2.f / (3.f * black_hole_isco_radius(a)));
}

/**
 * @brief Compute the spin- and accretion rate-dependant radiative efficiency
 * of a BH particle in the super-Eddington (slim disk) regime.
 *
 * This is eqn. 3-6 in Madau et al. (2014), which is based on numerical GR
 * results by Sadowski (2009).
 *
 * @param a Black hole spin, -1 < a < 1.
 * @param m_dot Accretion rate normalized to the Eddington rate.
 */
__attribute__((always_inline)) INLINE static float eps_slim_disc(float a,
                                                                 float mdot) {
  const float B = powf(4.627f - 4.445f * a, -0.5524f);
  const float C = powf(827.3f - 718.1f * a, -0.706f);
  const float A = powf(0.9663f - 0.9292f * a, -0.5693f);

#ifdef SWIFT_DEBUG_CHECKS
  if (mdot <= 0.f) {
    error(
        "The calculation of eps_slim_disc was called even though mdot is %f. "
        " This function should not have been called if the accretion rate is "
        " not > 0.",
        mdot);
  }
#endif

  /* Since we use a definition of the Eddington ratio (mdot) that includes
     the varying (Novikov-Thorne) radiative efficiency, we need to rescale this
     back to a constant one, as the paper provides a formula assuming a
     constant radiative efficiency. They use a value of 1/16, so we redefine
     the Eddington ratio using the ratio of efficiencies. */
  const float constant_rad_efficiency = 1.f / 16.f;

  mdot = mdot * constant_rad_efficiency / eps_Novikov_Thorne(a);

  /* Return radiative efficiency as given by Eqn 3 from Madau et al. (2014).
     Note that the equation provided in the paper is for L / L_Edd, rather than
     for L / (f_Edd * M_Edd * c^2). We thus need to multiply their Eqn 3 by
     L_Edd / (f_Edd * M_Edd * c^2) = eps_rad_constant / mdot. Here we have used
     M_Edd = L_Edd / (eps_rad_constant * c^2). Also note that mdot = f_Edd in
     our notation. */

  return (constant_rad_efficiency / mdot) *
         (0.985f / (B + mdot) + 0.015f / (C + mdot)) * A;
}

/**
 * @brief Decide which regime (mode) of accretion the BH particle is in.
 *
 * The possible modes are the thick disk, thin disk and slim disk, in
 * order of increasing accretion rate. The transition from thick to thin disk
 * is currently governed by a free parameter, props->mdot_crit_ADAF (of order
 * 0.01. The transition between the thin and slim disc is assumed to take place
 * at mdot = 1, i.e. for super-Eddington accretion. Note that this assumption
 * only works if we define mdot by using the spin-dependent radiative
 * efficiency, which we do.
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static void
black_hole_select_accretion_mode(struct bpart* bp,
                                 const struct black_holes_props* props) {

  /* For deciding the accretion mode, we want to use the Eddington fraction
   * calculated using the raw, unsuppressed accretion rate. This means that
   * if the disc is currently thick, its current Eddington fraction, which is
   * already suppressed, needs to be unsuppressed (increased) to retrieve the
   * raw Bondi-based Eddington ratio. */
  float eddington_fraction_Bondi = bp->eddington_fraction;
  eddington_fraction_Bondi *= 1.f / bp->accretion_efficiency;

  if (eddington_fraction_Bondi < props->mdot_crit_ADAF) {
    bp->accretion_mode = BH_thick_disc;
  } else {

    /* The disc is assumed to be slim (super-Eddington) if the Eddington
     * fraction is above 1. */
    if ((eddington_fraction_Bondi > 1.f) && (props->include_slim_disk)) {
      bp->accretion_mode = BH_slim_disc;
    } else {
      bp->accretion_mode = BH_thin_disc;
    }
  }

  /* If we do not include radiative feedback, then we force the disk to be in
     the thick disk mode always. */
  if (props->turn_off_radiative_feedback) {
    bp->accretion_mode = BH_thick_disc;
  }

  /* similar for if we do not include jets - we force the disk to be thin */
  if (!props->include_jets) {
    bp->accretion_mode = BH_thin_disc;
  }
}

/**
 * @brief Compute the accretion efficiency of a BH particle.
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk). We assume no accretion efficiency (100%) in the thin disk,
 * and allow for options for a non-zero accretion efficiency in the thick
 * and slim disc. For both we allow the option of constant values, and for the
 * thick disc we allow an option for a scaling with Eddington ratio that is
 * motivated by simulations.
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float
black_hole_accretion_efficiency(struct bpart* bp,
                                const struct black_holes_props* props,
                                const struct phys_const* constants,
                                const struct cosmology* cosmo) {

  if (bp->accretion_mode == BH_thick_disc ||
      bp->accretion_mode == BH_slim_disc) {

    if (props->accretion_efficiency_mode == BH_accretion_efficiency_constant) {
      if (bp->accretion_mode == BH_thick_disc) {
        return props->accretion_efficiency_thick;
      } else if (bp->accretion_mode == BH_slim_disc) {
        return props->accretion_efficiency_slim;
      } else {

#ifdef SWIFT_DEBUG_CHECKS
        error(
            "This branch of the function accretion_efficiency() should not"
            " have been reached!");
#endif

        return 1.f;
      }
    } else if (props->accretion_efficiency_mode ==
               BH_accretion_efficiency_variable) {

      if (bp->accretion_mode == BH_thick_disc) {

        /* Compute the transition radius between an outer thin disc and an
         * inner thick disc. This is assumed to happen at 10 R_G at the
         * critical value of the Eddington ratio between the two regimes.
         * The transition radius then increases as 1 / f_Edd^2. Note that
         * we also need to use the raw (unsuppressed) Eddington ratio here,
         * hence the multiplication by accretion efficiencies. Note that the
         * units of the transition radius here are in R_G. */
        float R_tr = props->ADIOS_R_in * props->mdot_crit_ADAF *
                     props->mdot_crit_ADAF * bp->accretion_efficiency *
                     bp->accretion_efficiency /
                     (bp->eddington_fraction * bp->eddington_fraction);

        /* We need to also compute the Bondi radius (in units of R_G), which
         * can be expressed in terms of the ratio between speed of light and
         * sound speed. */
        const double c = constants->const_speed_light_c;
        float gas_c_phys2 = bp->sound_speed_gas * cosmo->a_factor_sound_speed;
        gas_c_phys2 = gas_c_phys2 * gas_c_phys2;
        float R_B = c * c / gas_c_phys2;

        /* Limit the transition radius to no larger than R_B and no smaller
         * than 10 R_G. */
        R_tr = fminf(R_B, R_tr);
        R_tr = fmaxf(10.f, R_tr);

        /* Implement the actual scaling of accretion efficiency with transition
         * radius as found by GRMHD simulations. */
        float suppr_factor = powf(10.f / R_tr, props->ADIOS_s);
        return suppr_factor;
      } else if (bp->accretion_mode == BH_slim_disc) {
        return props->accretion_efficiency_slim;
      } else {

#ifdef SWIFT_DEBUG_CHECKS
        error(
            "This branch of the function accretion_efficiency() should not"
            " have been reached!");
#endif

        return 1.f;
      }
    } else {

#ifdef SWIFT_DEBUG_CHECKS
      error(
          "This branch of the function accretion_efficiency() should not"
          " have been reached!");
#endif

      return 1.f;
    }
  } else {
    return 1.f;
  }
}

/**
 * @brief Compute the jet efficiency of a BH particle.
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk).
 *
 * The equation implemented is eqn. 9 from Tchekhovskoy et al. (2010), with the
 * dimensionless magnetic flux phi taken as eqn. 9 from Narayan et al. (2022),
 * and an additional modification from Ricarte et al. (2023).
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float black_hole_jet_efficiency(
    struct bpart* bp, const struct black_holes_props* props) {

  /* Define placeholder value for the result. We will assign the final result
     to this variable. */
  float jet_eff = -1.f;
  if (props->fix_jet_efficiency) {
    jet_eff = props->jet_efficiency;
  } else {

    /* Numerical prefactor that appears in the jet power formula, related to
       the geometry of the magnetic field. */
    const float kappa = 0.05f;

    /* Definition of angular velocity at the BH event horizon */
    const float horizon_ang_vel =
        bp->spin / (2.f * (1.f + sqrtf(1.f - bp->spin * bp->spin)));

    /* Dimensionless magnetic flux as a function of BH spin, using Eqn. (15)
       from Narayan et al. (2022). */
    float phi = -20.2f * bp->spin * bp->spin * bp->spin -
                14.9f * bp->spin * bp->spin + 34.f * bp->spin + 52.6f;

    float Eddington_ratio = bp->eddington_fraction;

    /* Suppress the magnetic flux if we are in the thin or slim disc,
     * according to results from Ricarte et al. (2023). */
    if ((bp->accretion_mode == BH_slim_disc) ||
        (props->use_jets_in_thin_disc && bp->accretion_mode == BH_thin_disc)) {
      phi *= powf(Eddington_ratio / 1.88f, 1.29f) /
             (1.f + powf(Eddington_ratio / 1.88f, 1.29f));
    }

    /* Full jet efficiency formula as in Tchekhovskoy et al. (2010). */
    jet_eff = kappa * 0.25f * M_1_PI * phi * phi * horizon_ang_vel *
              horizon_ang_vel *
              (1.f + 1.38f * horizon_ang_vel * horizon_ang_vel -
               9.2f * horizon_ang_vel * horizon_ang_vel * horizon_ang_vel *
                   horizon_ang_vel);
  }

  /* Turn off jet feedback if we want to do that */
  if (!props->include_jets) {
    jet_eff = 0.f;
  }

  /* Turn off jets in thin disk mode if we want to do that */
  if ((bp->accretion_mode == BH_thin_disc) && (!props->use_jets_in_thin_disc)) {
    jet_eff = 0.f;
  }

#ifdef SWIFT_DEBUG_CHECKS
  if (jet_eff < 0.f) {
    error(
        "Something went wrong with calculation of jet efficiency of black "
        "holes. jet_eff is %f instead of jet_eff >= 0.",
        jet_eff);
  }
#endif

  return jet_eff;
}

/**
 * @brief Compute the radiative efficiency of a BH particle.
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk), since all modes have different radiative physics.
 *
 * For the thin disk, we assume the Novikov-Thorne (1973) radiative efficiency
 * based on general relativity. For the slim disk, we take the fit from Madau
 * et al. (2014), which is based on numerical GR results by Sadowski (2009).
 * For the thick disk, we assume radiative efficiencies from Mahadevan et al.
 * (1997).
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float
black_hole_radiative_efficiency(struct bpart* bp,
                                const struct black_holes_props* props) {

  /* Calculate Novikov-Thorne efficiency, which will be needed twice. */
  const float eps_TD = eps_Novikov_Thorne(bp->spin);

  /* Define placeholder value for the result. We will assign the final result
     to this variable. */
  float rad_eff = -1.f;

  if (props->fix_radiative_efficiency) {
    rad_eff = props->radiative_efficiency;
  } else {

    /* Start branching depending on which accretion mode the BH is in */
    if (bp->accretion_mode == BH_thin_disc) {

      /* Assign Novikov-Thorne efficiency to the thin disk. */
      rad_eff = eps_TD;
    } else if (bp->accretion_mode == BH_slim_disc) {

      /* Assign Madau 2014 efficiency to the slim disk. */
      rad_eff = eps_slim_disc(bp->spin, bp->eddington_fraction);
    } else {

#ifdef SWIFT_DEBUG_CHECKS
      if (props->beta_acc > 1.f) {
        error(
            "Something went wrong with calculation of radiative efficiency of "
            " black holes. beta_acc is %f instead of beta_acc < 1.",
            props->beta_acc);
      }
#endif

      /* Assign Mahadevan 1997 efficiency to the thick disk. We implement these
         using Eqns. (29) and (30) from Griffin et al. (2019). */
      if (bp->eddington_fraction < props->mdot_crit_ADAF) {
        rad_eff = 4.8f * eps_TD / black_hole_isco_radius(bp->spin) *
                  (1.f - props->beta_acc) * props->delta_ADAF;
      } else {
        rad_eff = 2.4f * eps_TD / black_hole_isco_radius(bp->spin) *
                  props->beta_acc * bp->eddington_fraction *
                  props->alpha_acc_2_inv;
      }

      /* Add contribution of truncated thin disc from larger radii */
      if (props->accretion_efficiency_mode ==
          BH_accretion_efficiency_variable) {
        float R_tr = props->ADIOS_R_in * props->mdot_crit_ADAF *
                     props->mdot_crit_ADAF * bp->accretion_efficiency *
                     bp->accretion_efficiency /
                     (bp->eddington_fraction * bp->eddington_fraction);
        R_tr = fmaxf(10.f, R_tr);
        rad_eff += 1.f - sqrtf(1. - 2.f / (3.f * R_tr));
      }
    }
  }

  /* Turn off radiative feedback if we want to do that */
  if (props->turn_off_radiative_feedback) {
    rad_eff = 0.f;
  }

#ifdef SWIFT_DEBUG_CHECKS
  if (rad_eff < 0.f) {
    error(
        "Something went wrong with calculation of radiative efficiency of "
        " black holes. rad_eff is %f instead of rad_eff >= 0.",
        rad_eff);
  }
#endif

  return rad_eff;
}

/**
 * @brief Compute the wind efficiency of a BH particle.
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk), with no wind assumed for the thin disc (effectively, the
 * radiation launches its own wind, while in the thick/slim disc, it is gas
 * pressure/MHD effects that launch the wind. In all cases, the wind is dumped
 * as thermal energy, alongside radiation.
 *
 * For the thick disk, we take the results from Sadowski et al. (2013)
 * (2013MNRAS.436.3856S), which is applicable to MAD discs. For the slim disc,
 * we constructed a fitting function by using the total MHD efficiency from
 * Ricarte et al. (2023) (2023ApJ...954L..22R), which includes both winds and
 * jets, and subtracting from that the jet efficiency used by our model.
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float black_hole_wind_efficiency(
    struct bpart* bp, const struct black_holes_props* props) {

  /* (Dimensionless) magnetic flux on the BH horizon, as given by the
     Narayan et al. (2022) fitting function for MAD discs. */
  float phi = -20.2f * bp->spin * bp->spin * bp->spin -
              14.9f * bp->spin * bp->spin + 34.f * bp->spin + 52.6f;

  if (bp->accretion_mode == BH_slim_disc) {

    /* We need to suppress the magnetic flux by an Eddington-ratio-dependent
       factor (Equation 3 from Ricarte et al. 2023). */
    float Eddington_ratio = bp->eddington_fraction;
    phi *= powf(Eddington_ratio / 1.88f, 1.29f) /
           (1.f + powf(Eddington_ratio / 1.88f, 1.29f));
    float phi_factor = (1.f + (phi / 50.f) * (phi / 50.f));

    float horizon_ang_vel =
        bp->spin / (2.f * (1.f + sqrtf(1.f - bp->spin * bp->spin)));
    float spin_factor =
        1.f - 8.f * horizon_ang_vel * horizon_ang_vel + 1.f * horizon_ang_vel;
    if (bp->spin > 0.f) {
      spin_factor = max(0.4f, spin_factor);
    } else {
      spin_factor = max(0.f, spin_factor);
    }

    /* Final result for slim disc wind efficiency. (Not published
       yet anywhere) */
    return props->slim_disc_wind_factor * 0.0635f * phi_factor * spin_factor;
  } else if (bp->accretion_mode == BH_thick_disc && props->use_ADIOS_winds) {

    /* Equation (29) from Sadowski et al. (2013). */
    float horizon_ang_vel =
        bp->spin / (2.f * (1.f + sqrtf(1.f - bp->spin * bp->spin)));
    return 0.005f * (1.f + 3.f * phi * phi / 2500.f * horizon_ang_vel *
                               horizon_ang_vel / 0.04f);
  } else {
    return 0.f;
  }
}

/**
 * @brief Compute the spec. ang. mom. at the inner radius of a BH particle.
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk), since advection-dominated modes (thick and slim disk)
 * have more radial orbits.
 *
 * For the thin disk, we assume that the spec. ang. mom. consumed matches that
 * of the innermost stable circular orbit (ISCO). For the other two modes, we
 * assume that the accreted ang. mom. at the event horizon is 45 per cent of
 * that at the ISCO, based on the fit from Benson & Babul (2009).
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float l_acc(
    struct bpart* bp, const struct phys_const* constants,
    const struct black_holes_props* props) {

  /* Define placeholder value for the result. We will assign the final result
     to this variable. */
  float L = -1.f;

#ifdef SWIFT_DEBUG_CHECKS
  if (black_hole_isco_radius(bp->spin) <= 0.6667f) {
    error(
        "Something went wrong with calculation of l_acc of black holes. "
        " r_isco is %f instead of r_isco > 1.",
        black_hole_isco_radius(bp->spin));
  }
#endif

  /* Spec. ang. mom. at ISCO */
  const float L_ISCO =
      0.385f *
      (1.f + 2.f * sqrtf(3.f * black_hole_isco_radius(bp->spin) - 2.f));

  /* Branch depending on which accretion mode the BH is in */
  if ((bp->accretion_mode == BH_thick_disc) ||
      (bp->accretion_mode == BH_slim_disc)) {
    L = 0.45f * L_ISCO;
  } else {
    L = L_ISCO;
  }

#ifdef SWIFT_DEBUG_CHECKS
  if (L <= 0.f) {
    error(
        "Something went wrong with calculation of l_acc of black holes. "
        " l_acc is %f instead of l_acc > 0.",
        L);
  }
#endif

  return L;
}

/**
 * @brief Compute the evolution of the spin of a BH particle. This
 * spinup/spindown rate is equal to da / dln(M_BH)_0, or
 * da / (d(M_BH,0)/M_BH ), where the subscript '0' means that it is
 * the mass increment before losses due to jets, radiation or winds
 * (i.e. without the effect of efficiencies).
 *
 * The result depends on bp->accretion_mode (thick disk, thin disk or
 * slim disk), due to differing spec. ang. momenta as well as jet and
 * radiative efficiencies.
 *
 * For the thick disc, we use the jet spindown formula from Narayan et al.
 * (2022). For the slim and thin disc, we use the formula from Ricarte et al.
 * (2023).
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float black_hole_spinup_rate(
    struct bpart* bp, const struct phys_const* constants,
    const struct black_holes_props* props) {
  const float a = bp->spin;

  if ((a == 0.f) || (a < -0.9981f) || (a > 0.9981f)) {
    error(
        "The spinup function was called and spin is %f. Spin should "
        " not be a = 0, a < -0.998 or a > 0.998.",
        a);
  }

  if (bp->accretion_mode == BH_thin_disc && !props->use_jets_in_thin_disc) {

    /* If we are in the thin disc and use no jets, we use the simple spinup /
     * spindown formula, e.g. from Benson & Babul (2009). This accounts for
     * accretion only. */
    return l_acc(bp, constants, props) -
           2.f * a * (1.f - bp->radiative_efficiency);

  } else if (bp->accretion_mode == BH_thick_disc) {

    /* Fiting function from Narayan et al. (2022) */
    return 0.45f - 12.53f * a - 7.8f * a * a + 9.44f * a * a * a +
           5.71f * a * a * a * a - 4.03f * a * a * a * a * a;

  } else if (bp->accretion_mode == BH_slim_disc ||
             (bp->accretion_mode == BH_thin_disc &&
              props->use_jets_in_thin_disc)) {

    /* Fitting function from Ricarte et al. (2023). */
    float Eddington_ratio =
        bp->eddington_fraction * eps_Novikov_Thorne(bp->spin) / 0.1f;
    float xi = Eddington_ratio * 0.017f;
    float s_min = 0.86f - 1.94f * bp->spin;
    float L_ISCO =
        0.385f *
        (1.f + 2.f * sqrtf(3.f * black_hole_isco_radius(bp->spin) - 2.f));
    float s_thin = L_ISCO - 2.f * a * (1.f - eps_Novikov_Thorne(bp->spin));
    float s_HD = (s_thin + s_min * xi) / (1.f + xi);

    float horizon_ang_vel =
        fabsf(bp->spin) / (2.f * (1.f + sqrtf(1.f - bp->spin * bp->spin)));
    float k_EM = 0.23f;
    if (bp->spin > 0.f) {
      k_EM = min(0.1f + 0.5f * bp->spin, 0.35f);
    }

    float s_EM = -1.f * bp->spin / fabsf(bp->spin) * bp->jet_efficiency *
                 (1.f / (k_EM * horizon_ang_vel) - 2.f * bp->spin);
    return s_HD + s_EM;
  } else {

#ifdef SWIFT_DEBUG_CHECKS
    error(
        "We shouldn't have reached this branch of the "
        "black_hole_spinup_rate() function!");
#endif

    return 0;
  }
}

/**
 * @brief Compute the heating temperature used for AGN feedback.
 *
 * @param bp The #bpart doing feedback.
 * @param props Properties of the BH scheme.
 * @param cosmo The current cosmological model.
 * @param constants The physical constants (in internal units).
 */
__attribute__((always_inline)) INLINE static float black_hole_feedback_delta_T(
    const struct bpart* bp, const struct black_holes_props* props,
    const struct cosmology* cosmo, const struct phys_const* constants) {

  float delta_T = -1.f;
  if (props->AGN_heating_temperature_model ==
      AGN_heating_temperature_constant) {
    delta_T = props->AGN_delta_T_desired;

  } else if (props->AGN_heating_temperature_model ==
             AGN_heating_temperature_local) {

    /* Calculate feedback power */
    const float feedback_power =
        bp->radiative_efficiency * props->epsilon_f * bp->accretion_rate *
        constants->const_speed_light_c * constants->const_speed_light_c;

    /* Get the sound speed of the hot gas in the kernel. Make sure the actual
     * value that is used is at least the value specified in the parameter
     * file. */
    float sound_speed_hot_gas =
        bp->sound_speed_gas_hot * cosmo->a_factor_sound_speed;
    sound_speed_hot_gas =
        max(sound_speed_hot_gas, props->sound_speed_hot_gas_min);

    /* Take the maximum of the sound speed of the hot gas and the gas velocity
     * dispersion. Calculate the replenishment time-scale by assuming that it
     * will replenish under the influence of whichever of those two values is
     * larger. */
    const float gas_dispersion = bp->velocity_dispersion_gas * cosmo->a_inv;
    const double replenishment_time_scale =
        bp->h * cosmo->a / max(sound_speed_hot_gas, gas_dispersion);

    /* Calculate heating temperature from the power, smoothing length (proper,
       not comoving), neighbour sound speed and neighbour mass. Apply floor. */
    const float delta_T_repl =
        (2.f * 0.6f * constants->const_proton_mass * feedback_power *
         replenishment_time_scale) /
        (3.f * constants->const_boltzmann_k * bp->ngb_mass);

    /* Calculate heating temperature from the crossing condition, i.e. set the
     * temperature such that a new particle pair will be heated roughly when
     * the previous one crosses (exits) the BH kernel on account of its sound-
     * crossing time-scale. This also depends on power, smoothing length and
     * neighbour mass (per particle, not total). */
    const float delta_T_cross =
        (0.6 * constants->const_proton_mass) / (constants->const_boltzmann_k) *
        powf(2.f * bp->h * cosmo->a * feedback_power /
                 (sqrtf(15.f) * bp->ngb_mass / ((double)bp->num_ngbs)),
             0.6667f);

    /* Calculate minimum temperature from Dalla Vecchia & Schaye (2012) to
       prevent numerical overcooling. This is in Kelvin. */
    const float delta_T_min_Dalla_Vecchia =
        props->normalisation_Dalla_Vecchia *
        cbrt(bp->ngb_mass / props->ref_ngb_mass_Dalla_Vecchia) *
        pow(bp->rho_gas * cosmo->a3_inv / props->ref_density_Dalla_Vecchia,
            2.f / 3.f);

    /* Apply the crossing and replenishment floors */
    delta_T = fmaxf(delta_T_cross, delta_T_repl);

    /* Apply the Dalla Vecchia floor, and multiply by scaling factor */
    delta_T = props->delta_T_xi * fmaxf(delta_T, delta_T_min_Dalla_Vecchia);

    /* Apply an additional, constant floor */
    delta_T = fmaxf(delta_T, props->delta_T_min);

    /* Apply a ceiling */
    delta_T = fminf(delta_T, props->delta_T_max);
  }

  return delta_T;
}

/**
 * @brief Compute the jet kick velocity to be used for jet feedback.
 *
 * @param bp The #bpart doing feedback.
 * @param props Properties of the BH scheme.
 * @param cosmo The current cosmological model.
 * @param constants The physical constants (in internal units).
 */
__attribute__((always_inline)) INLINE static float black_hole_feedback_dv_jet(
    const struct bpart* bp, const struct black_holes_props* props,
    const struct cosmology* cosmo, const struct phys_const* constants) {

  float v_jet = -1.;
  if (props->AGN_jet_velocity_model == AGN_jet_velocity_BH_mass) {

    v_jet =
        powf((bp->subgrid_mass / props->v_jet_BH_mass_scaling_reference_mass),
             props->v_jet_BH_mass_scaling_slope);

    /* Apply floor and ceiling values */
    v_jet = props->v_jet_max * fminf(v_jet, 1.f);
    v_jet = fmaxf(v_jet, props->v_jet_min);

  } else if (props->AGN_jet_velocity_model == AGN_jet_velocity_constant) {
    v_jet = props->v_jet;

  } else if (props->AGN_jet_velocity_model == AGN_jet_velocity_mass_loading) {

    /* Calculate jet velocity from the efficiency and mass loading, and then
       apply a floor value*/
    v_jet = sqrtf(2.f * bp->jet_efficiency / props->v_jet_mass_loading) *
            constants->const_speed_light_c;

    /* Apply floor and ceiling values */
    v_jet = fmaxf(props->v_jet_min, v_jet);
    v_jet = fminf(props->v_jet_max, v_jet);

  } else if (props->AGN_jet_velocity_model == AGN_jet_velocity_local) {

    /* Calculate jet power */
    const double jet_power = bp->jet_efficiency * bp->accretion_rate *
                             constants->const_speed_light_c *
                             constants->const_speed_light_c;

    /* Get the sound speed of the hot gas in the kernel. Make sure the actual
     * value that is used is at least the value specified in the parameter
     * file. */
    float sound_speed_hot_gas =
        bp->sound_speed_gas_hot * cosmo->a_factor_sound_speed;
    sound_speed_hot_gas =
        max(sound_speed_hot_gas, props->sound_speed_hot_gas_min);

    /* Take the maximum of the sound speed of the hot gas and the gas velocity
     * dispersion. Calculate the replenishment time-scale by assuming that it
     * will replenish under the influence of whichever of those two values is
     * larger. */
    const float gas_dispersion = bp->velocity_dispersion_gas * cosmo->a_inv;
    const double replenishment_time_scale =
        bp->h * cosmo->a / max(sound_speed_hot_gas, gas_dispersion);

    /* Calculate jet velocity from the replenishment condition, taking the
     * power, smoothing length (proper, not comoving), neighbour sound speed
     * and (total) neighbour mass. */
    const float v_jet_repl =
        sqrtf(jet_power * replenishment_time_scale / (2.f * bp->ngb_mass));

    /* Calculate jet velocity from the crossing condition, i.e. set the
     * velocity such that a new particle pair will be launched roughly when
     * the previous one crosses (exits) the BH kernel. This also depends on
     * power, smoothing length and neighbour mass (per particle, not total). */
    const float v_jet_cross =
        cbrtf(bp->h * cosmo->a * jet_power /
              (4.f * bp->ngb_mass / ((double)bp->num_ngbs)));

    /* Find whichever of these two is the minimum, and multiply it by an
     * arbitrary scaling factor (whose fiducial value is 1, i.e. no
     * rescaling. */
    v_jet = props->v_jet_xi * fmaxf(v_jet_repl, v_jet_cross);

    /* Apply floor and ceiling values */
    v_jet = fmaxf(v_jet, props->v_jet_min);
    v_jet = fminf(v_jet, props->v_jet_max);

  } else {
    error(
        "The scaling of jet velocities with halo mass is currently not "
        "supported.");
  }

  if (v_jet <= 0.f) {
    error(
        "The black_hole_feedback_dv_jet returned a value less than 0. which "
        " is v_jet = %f.",
        v_jet);
  }

  return v_jet;
}

/**
 * @brief Auxilliary function used for the calculation of final spin of
 * a BH merger.
 *
 * This implements the fitting formula for the variable l from Barausse &
 * Rezolla (2009), ApJ, 704, Equation 10. It is used in the merger_spin_evolve()
 * function.
 *
 * @param a1 spin of the first (more massive) black hole
 * @param a2 spin of the less massive black hole
 * @param q mass ratio of the two black holes, 0 < q < 1
 * @param eta symmetric mass ratio of the two black holes
 * @param cos_alpha cosine of the angle between the two spins
 * @param cos_beta cosine of the angle between the first spin and the initial
 * total angular momentum
 * @param cos_gamma cosine of the angle between the second spin and the initial
 * total angular momentu
 */
__attribute__((always_inline)) INLINE static float black_hole_l_variable(
    const float a1, const float a2, const float q, const float eta,
    const float cos_alpha, const float cos_beta, const float cos_gamma) {

  /* Define the numerical fitting parameters used in Eqn. 10 */
  const float s4 = -0.1229f;
  const float s5 = 0.4537f;
  const float t0 = -2.8904f;
  const float t2 = -3.5171f;
  const float t3 = 2.5763f;

  /* Gather the terms of Eqn. 10 */
  const float term1 = 2.f * sqrtf(3.f);
  const float term2 = t2 * eta;
  const float term3 = t3 * eta * eta;
  const float term4 =
      s4 *
      (a1 * a1 + a2 * a2 * q * q * q * q + 2.f * a1 * a2 * q * q * cos_alpha) /
      ((1.f + q * q) * (1.f + q * q));
  const float term5 = (s5 * eta + t0 + 2.f) *
                      (a1 * cos_beta + a2 * q * q * cos_gamma) / (1.f + q * q);

  /* Return the variable l */
  return term1 + term2 + term3 + term4 + term5;
}

/**
 * @brief Auxilliary function used for the calculation of mass lost to GWs.
 *
 * In this model (SWIFT-EAGLE with spin) we assume 0 losses.
 *
 * @param a1 spin of the first (more massive) black hole
 * @param a2 spin of the less massive black hole
 * @param q mass ratio of the two black holes, 0 < q < 1
 * @param eta symmetric mass ratio of the two black holes
 * @param cos_beta cosine of the angle between the first spin and the initial
 * total angular momentum
 * @param cos_gamma cosine of the angle between the second spin and the initial
 * total angular momentu
 */
__attribute__((always_inline)) INLINE static float mass_fraction_lost_to_GWs(
    const float a1, const float a2, const float q, const float eta,
    const float cos_beta, const float cos_gamma) {
  return 0.;
}

/**
 * @brief Compute the resultant spin of a black hole merger, as well as the
 * mass lost to gravitational waves.
 *
 * This implements the fitting formula for the final spin from Barausse &
 * Rezolla (2009), ApJ, 704, Equations 6 and 7. For the fraction of mass lost,
 * we use Eqns 16-18 from Barausse et al. (2012), ApJ, 758.
 *
 * @param bp Pointer to the b-particle data.
 * @param constants Physical constants (in internal units).
 * @param props Properties of the black hole scheme.
 */
__attribute__((always_inline)) INLINE static float
black_hole_merger_spin_evolve(struct bpart* bpi, const struct bpart* bpj,
                              const struct phys_const* constants) {

  /* Check if something is wrong with the masses. This is important and could
     possibly happen as a result of jet spindown and mass loss at any time,
     so we want to know about it. */
  if ((bpj->subgrid_mass <= 0.f) || (bpi->subgrid_mass <= 0.f)) {
    error(
        "Something went wrong with calculation of spin of a black hole "
        " merger remnant. The black hole masses are %f and %f, instead of  > "
        "0.",
        bpj->subgrid_mass, bpi->subgrid_mass);
  }

  /* Get the black hole masses before the merger and losses to GWs. */
  const float m1 = bpi->subgrid_mass;
  const float m2 = bpj->subgrid_mass;

  /* Define some variables (combinations of mass ratios) used in the
     papers described in the header. */
  const float mass_ratio = m2 / m1;
  const float sym_mass_ratio =
      mass_ratio / ((mass_ratio + 1.f) * (mass_ratio + 1.f));

  /* The absolute values of the spins are also needed */
  const float spin1 = fabsf(bpi->spin);
  const float spin2 = fabsf(bpj->spin);

  /* Check if the BHs have been spun down to 0. This is again an important
     potential break point, we want to know about it. */
  if ((spin1 == 0.f) || (spin2 == 0.f)) {
    error(
        "Something went wrong with calculation of spin of a black hole "
        " merger remnant. The black hole spins are %f and %f, instead of  > 0.",
        spin1, spin2);
  }

  /* Define the spin directions. */
  const float spin_vec1[3] = {spin1 * bpi->angular_momentum_direction[0],
                              spin1 * bpi->angular_momentum_direction[1],
                              spin1 * bpi->angular_momentum_direction[2]};
  const float spin_vec2[3] = {spin2 * bpj->angular_momentum_direction[0],
                              spin2 * bpj->angular_momentum_direction[1],
                              spin2 * bpj->angular_momentum_direction[2]};

  /* We want to compute the direction of the orbital angular momentum of the
     two BHs, which is used in the fits. Start by defining the coordinates in
     the frame of one of the BHs (it doesn't matter which one, the total
     angular momentum is the same). */
  const float centre_of_mass[3] = {
      (m1 * bpi->x[0] + m2 * bpj->x[0]) / (m1 + m2),
      (m1 * bpi->x[1] + m2 * bpj->x[1]) / (m1 + m2),
      (m1 * bpi->x[2] + m2 * bpj->x[2]) / (m1 + m2)};
  const float centre_of_mass_vel[3] = {
      (m1 * bpi->v[0] + m2 * bpj->v[0]) / (m1 + m2),
      (m1 * bpi->v[1] + m2 * bpj->v[1]) / (m1 + m2),
      (m1 * bpi->v[2] + m2 * bpj->v[2]) / (m1 + m2)};

  /* Coordinates of each of the BHs in the frame of the centre of mass. */
  const float relative_coordinates_1[3] = {bpi->x[0] - centre_of_mass[0],
                                           bpi->x[1] - centre_of_mass[1],
                                           bpi->x[2] - centre_of_mass[2]};
  const float relative_coordinates_2[3] = {bpj->x[0] - centre_of_mass[0],
                                           bpj->x[1] - centre_of_mass[1],
                                           bpj->x[2] - centre_of_mass[2]};

  /* The velocities of each BH in the centre of mass frame. */
  const float relative_velocities_1[3] = {bpi->v[0] - centre_of_mass_vel[0],
                                          bpi->v[1] - centre_of_mass_vel[1],
                                          bpi->v[2] - centre_of_mass_vel[2]};
  const float relative_velocities_2[3] = {bpj->v[0] - centre_of_mass_vel[0],
                                          bpj->v[1] - centre_of_mass_vel[1],
                                          bpj->v[2] - centre_of_mass_vel[2]};

  /* The angular momentum of each BH in the centre of mass frame. */
  const float angular_momentum_1[3] = {
      m1 * (relative_coordinates_1[1] * relative_velocities_1[2] -
            relative_coordinates_1[2] * relative_velocities_1[1]),
      m1 * (relative_coordinates_1[2] * relative_velocities_1[0] -
            relative_coordinates_1[0] * relative_velocities_1[2]),
      m1 * (relative_coordinates_1[0] * relative_velocities_1[1] -
            relative_coordinates_1[1] * relative_velocities_1[0])};
  const float angular_momentum_2[3] = {
      m2 * (relative_coordinates_2[1] * relative_velocities_2[2] -
            relative_coordinates_2[2] * relative_velocities_2[1]),
      m2 * (relative_coordinates_2[2] * relative_velocities_2[0] -
            relative_coordinates_2[0] * relative_velocities_2[2]),
      m2 * (relative_coordinates_2[0] * relative_velocities_2[1] -
            relative_coordinates_2[1] * relative_velocities_2[0])};

  /* Calculate the orbital angular momentum itself. */
  const float orbital_angular_momentum[3] = {
      angular_momentum_1[0] + angular_momentum_2[0],
      angular_momentum_1[1] + angular_momentum_2[1],
      angular_momentum_1[2] + angular_momentum_2[2]};

  /* Calculate the magnitude of the orbital angular momentum. */
  const float orbital_angular_momentum_magnitude =
      sqrtf(orbital_angular_momentum[0] * orbital_angular_momentum[0] +
            orbital_angular_momentum[1] * orbital_angular_momentum[1] +
            orbital_angular_momentum[2] * orbital_angular_momentum[2]);

  /* Normalize and get the direction of the orbital angular momentum. */
  float orbital_angular_momentum_direction[3] = {0.f, 0.f, 0.f};
  if (orbital_angular_momentum_magnitude > 0.) {
    orbital_angular_momentum_direction[0] =
        orbital_angular_momentum[0] / orbital_angular_momentum_magnitude;
    orbital_angular_momentum_direction[1] =
        orbital_angular_momentum[1] / orbital_angular_momentum_magnitude;
    orbital_angular_momentum_direction[2] =
        orbital_angular_momentum[2] / orbital_angular_momentum_magnitude;
  }

  /* We also need to compute the total (initial) angular momentum of the
     system, i.e. including the orbital angular momentum and the spins. This
     is needed since the final spin is assumed to be along the direction of
     this total angular momentum. Hence here we compute the direction. */
  const float j_BH_1 =
      fabs(bpi->subgrid_mass * bpi->subgrid_mass * bpi->spin *
           constants->const_newton_G / constants->const_speed_light_c);
  const float j_BH_2 =
      fabs(bpj->subgrid_mass * bpj->subgrid_mass * bpj->spin *
           constants->const_newton_G / constants->const_speed_light_c);

  float total_angular_momentum_direction[3] = {
      j_BH_1 * spin_vec1[0] + j_BH_2 * spin_vec2[0] +
          orbital_angular_momentum[0],
      j_BH_1 * spin_vec1[1] + j_BH_2 * spin_vec2[1] +
          orbital_angular_momentum[1],
      j_BH_1 * spin_vec1[2] + j_BH_2 * spin_vec2[2] +
          orbital_angular_momentum[2]};

  /* The above is actually the total angular momentum, so we need to normalize
     to get the directions. */
  const float total_angular_momentum_magnitude =
      sqrtf(total_angular_momentum_direction[0] *
                total_angular_momentum_direction[0] +
            total_angular_momentum_direction[1] *
                total_angular_momentum_direction[1] +
            total_angular_momentum_direction[2] *
                total_angular_momentum_direction[2]);
  total_angular_momentum_direction[0] =
      total_angular_momentum_direction[0] / total_angular_momentum_magnitude;
  total_angular_momentum_direction[1] =
      total_angular_momentum_direction[1] / total_angular_momentum_magnitude;
  total_angular_momentum_direction[2] =
      total_angular_momentum_direction[2] / total_angular_momentum_magnitude;

  /* We now define some extra variables used by the fitting functions. The
     below ones are cosines of angles between the two spins and orbital angular
     momentum in various combinations (Eqn 9 in Barausse & Rezolla 2009) */
  const float cos_alpha =
      (spin_vec1[0] * spin_vec2[0] + spin_vec1[1] * spin_vec2[1] +
       spin_vec1[2] * spin_vec2[2]) /
      (spin1 * spin2);
  const float cos_beta =
      (spin_vec1[0] * orbital_angular_momentum_direction[0] +
       spin_vec1[1] * orbital_angular_momentum_direction[1] +
       spin_vec1[2] * orbital_angular_momentum_direction[2]) /
      spin1;
  const float cos_gamma =
      (spin_vec2[0] * orbital_angular_momentum_direction[0] +
       spin_vec2[1] * orbital_angular_momentum_direction[1] +
       spin_vec2[2] * orbital_angular_momentum_direction[2]) /
      spin2;

  /* Get the variable l used in the fit, see Eqn. 10 in Barausse & Rezolla
     (2009). */
  const float l = black_hole_l_variable(
      spin1, spin2, mass_ratio, sym_mass_ratio, cos_alpha, cos_beta, cos_gamma);

  const float l_vector[3] = {l * orbital_angular_momentum_direction[0],
                             l * orbital_angular_momentum_direction[1],
                             l * orbital_angular_momentum_direction[2]};

  /* Final spin vector, constructed from the two spins and the auxilliary l
     vector. */
  const float spin_vector[3] = {
      (spin_vec1[0] +
       spin_vec2[0] * mass_ratio * mass_ratio * l_vector[0] * mass_ratio) /
          ((1.f + mass_ratio) * (1.f + mass_ratio)),
      (spin_vec1[1] +
       spin_vec2[1] * mass_ratio * mass_ratio * l_vector[1] * mass_ratio) /
          ((1.f + mass_ratio) * (1.f + mass_ratio)),
      (spin_vec1[2] +
       spin_vec2[2] * mass_ratio * mass_ratio * l_vector[2] * mass_ratio) /
          ((1.f + mass_ratio) * (1.f + mass_ratio))};

#ifdef SWIFT_DEBUG_CHECKS
  if (l < 0.f) {
    error(
        "Something went wrong with calculation of spin of a black hole "
        " merger remnant. The l factor is %f, instead of  >= 0.",
        l);
  }
#endif

  /* Get magnitude of the final spin simply as the magnitude of the vector. */
  const float final_spin_magnitude =
      sqrtf(spin_vector[0] * spin_vector[0] + spin_vector[1] * spin_vector[1] +
            spin_vector[2] * spin_vector[2]);

#ifdef SWIFT_DEBUG_CHECKS
  if (final_spin_magnitude <= 0.f) {
    error(
        "Something went wrong with calculation of spin of a black hole "
        " merger remnant. The final spin magnitude is %f, instead of > 0.",
        final_spin_magnitude);
  }
#endif

  /* Assign the final spin value to the BH, but also make sure we don't go
     above 0.998 nor below 0.001. */
  bpi->spin = min(final_spin_magnitude, 0.998f);
  if (fabsf(bpi->spin) < 0.01f) {
    bpi->spin = 0.01f;
  }

  /* Assign the directions of the spin to the BH. */
  bpi->angular_momentum_direction[0] = spin_vector[0] / final_spin_magnitude;
  bpi->angular_momentum_direction[1] = spin_vector[1] / final_spin_magnitude;
  bpi->angular_momentum_direction[2] = spin_vector[2] / final_spin_magnitude;

  /* Finally we also want to calculate the fraction of total mass-energy
     lost during the merger to gravitational waves. We use Eqn. 16 and 18
     from Barausse et al. (2012), ApJ, p758. */
  const float mass_frac_lost_to_GW = mass_fraction_lost_to_GWs(
      spin1, spin2, mass_ratio, sym_mass_ratio, cos_beta, cos_gamma);

  return mass_frac_lost_to_GW;
}

#endif /* SWIFT_SPIN_JET_BLACK_HOLES_SPIN_H */
