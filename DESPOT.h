/*
 *  DESPOT1.h
 *  MacRI
 *
 *  Created by Tobias Wood on 17/10/2011.
 *  Copyright 2011 Tobias Wood. All rights reserved.
 *
 */
#ifndef __DESPOT__
#define __DESPOT__

#include "math.h"
#include "mathArray.h"
int tests();
double SPGR(double flipAngle, double *p, double *c);
void SPGR_Jacobian(double *angles, int nD, double *p, double *c, double *result);
double IRSPGR(double TI, double *p, double *c);
void IRSPGR_Jacobian(double *data, int nD, double *par, double *c, double *result);
double SSFP(double flipAngle, double *p, double *c);
double calcHIFI(double *flipAngles, double *spgrVals, int nSPGR, double spgrTR,
				double *TI, double *irVals, int nIR, double irFlipAngle, double irTR,
				double *M0, double *T1, double *B1);
void calcDESPOT1(double *flipAngles, double *spgrVals, int n,
				 double TR, double B1, double *M0, double *T1);
void calcDESPOT2(double *flipAngles, double *ssfpVals, int n,
                 double TR, double T1, double B1, double *M0, double *T2);
double calcSPGR(double *angles, double *spgrVals, int n, double TR,
                double *M0, double *T1, double *B1);
double calcIR(double *TI, double *irVals, int nIR,
              double alpha, double TR,
			  double *M0, double *T1, double *B1);
#endif