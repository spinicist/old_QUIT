/*
 *  SignalEquations.h
 *
 *  Created by Tobias Wood on 17/10/2011.
 *  Copyright (c) 2011-2013 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef DESPOT_SIGEQU
#define DESPOT_SIGEQU

#include <iostream>
#include <exception>
#include <Eigen/Dense>
#include <Eigen/Geometry>

using namespace std;
using namespace Eigen;

//******************************************************************************
// Convenience
//******************************************************************************
double clamp(double value, double low, double high);

//******************************************************************************
// Magnetisation Evolution Matrices, helper functions etc.
//******************************************************************************
typedef const double cdbl; // To save tedious typing
typedef const ArrayXd carr;

typedef Matrix<double, 6, 6> Matrix6d;
typedef Matrix<double, 6, 1> Vector6d;
typedef Matrix<double, 9, 9> Matrix9d;
typedef Matrix<double, 9, 1> Vector9d;
typedef Matrix<double, 3, Dynamic> MagVector;

const VectorXd SigMag(const MagVector &M_in);
const VectorXcd SigComplex(const MagVector &M_in);
const MagVector SumMC(const MatrixXd &M_in);

inline const Matrix3d Relax(cdbl &T1, cdbl &T2);
inline const Matrix3d InfinitesimalRF(cdbl &dalpha);
inline const Matrix3d OffResonance(cdbl &inHertz);
inline const Matrix3d Spoiling();
inline const Matrix6d Exchange(cdbl &k_ab, cdbl &k_ba);
const void CalcExchange(cdbl tau_a, cdbl f_a, cdbl f_b, double &k_ab, double &k_ba);

//******************************************************************************
// Actual Signal Equations
//******************************************************************************
MagVector One_SPGR(carr &flip, cdbl TR, cdbl PD, cdbl T1);
MagVector One_SSFP(carr &flip, cdbl TR, cdbl ph, cdbl PD, cdbl T1, cdbl T2, cdbl f0);
MagVector One_SSFP_Finite(carr &flip, const bool spoil, cdbl TR, cdbl Trf, cdbl TE, cdbl ph,
                          cdbl PD, cdbl T1, cdbl T2, cdbl f0);
MagVector One_SSFP_Ellipse(carr &flip, cdbl TR, cdbl PD, cdbl T1, cdbl T2, cdbl f0);
MagVector MP_RAGE(cdbl flip, cdbl TR, const int N, carr &TI, cdbl TD, cdbl PD, cdbl T1);

MagVector Two_SPGR(carr &flip, cdbl TR, cdbl PD, cdbl T1_a, cdbl T1_b, cdbl tau_a, cdbl f_a);
MagVector Two_SSFP(carr &flip, cdbl TR, cdbl ph, cdbl PD, cdbl T1_a, cdbl T2_a, cdbl T1_b, cdbl T2_b, cdbl tau_a, cdbl f_a, cdbl f0_a, cdbl f0_b);
MagVector Two_SSFP_Finite(carr &flip, const bool spoil, cdbl TR, cdbl Trf, cdbl TE, cdbl ph,
                          cdbl PD, cdbl T1_a, cdbl T2_a, cdbl T1_b, cdbl T2_b,
                          cdbl tau_a, cdbl f_a, cdbl f0_a, cdbl f0_b);

MagVector Three_SPGR(carr &flip, cdbl TR, cdbl PD, cdbl T1_a, cdbl T1_b, cdbl T1_c, cdbl tau_a, cdbl f_a, cdbl f_c);
MagVector Three_SSFP(carr &flip, cdbl TR, cdbl ph, cdbl PD, cdbl T1_a, cdbl T2_a, cdbl T1_b, cdbl T2_b, cdbl T1_c, cdbl T2_c, cdbl tau_a, cdbl f_a, cdbl f_c, cdbl f0_a, cdbl f0_b, cdbl f0_c);
MagVector Three_SSFP_Finite(carr &flip, const bool spoil, cdbl TR, cdbl Trf, cdbl TE, cdbl ph,
                            cdbl PD, cdbl T1_a, cdbl T2_a, cdbl T1_b, cdbl T2_b, cdbl T1_c, cdbl T2_c,
                            cdbl tau_a, cdbl f_a, cdbl f_c,
                            cdbl f0_a, cdbl f0_b, cdbl f0_c);

#endif
