/*
 *  despot2_main.cpp
 *
 *  Created by Tobias Wood on 23/01/2012.
 *  Copyright (c) 2012-2013 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <getopt.h>
#include <iostream>
#include <atomic>
#include <Eigen/Dense>
#include "unsupported/Eigen/NonLinearOptimization"
#include "unsupported/Eigen/NumericalDiff"

#include "Nifti/Nifti.h"
#include "QUIT/QUIT.h"
#include "DESPOT.h"
#include "DESPOT_Functors.h"

using namespace std;
using namespace Eigen;
using namespace QUIT;

//******************************************************************************
// Arguments / Usage
//******************************************************************************
const string usage {
"Usage is: despot2 [options] T1_map ssfp_file\n\
\n\
Options:\n\
	--help, -h        : Print this message.\n\
	--mask, -m file   : Mask input with specified file.\n\
	--out, -o path    : Add a prefix to the output filenames.\n\
	--B1 file         : B1 Map file.\n\
	--verbose, -v     : Print slice processing times.\n\
	--no-prompt, -n   : Suppress input prompts.\n\
	--algo, -a l      : LLS algorithm (default)\n\
	           w      : WLLS algorithm\n\
	           n      : NLLS (Levenberg-Marquardt)\n\
	--its, -i N       : Max iterations for WLLS (default 4)\n\
	--threads, -T N   : Use N threads (default=hardware limit)\n\
	--elliptical, -e  : Input is band-free elliptical data.\n\
	Only apply for elliptical data:\n\
	--negflip         : Flip-angles are in negative sense.\n\
	--meanf0          : Output average f0 value.\n"
};

enum class Algos { LLS, WLLS, NLLS };
static int verbose = false, prompt = true, elliptical = false, negflip = false, meanf0 = false;
static size_t nIterations = 4;
static Algos algo;
static string outPrefix;
static struct option long_opts[] =
{
	{"B1", required_argument, 0, '1'},
	{"elliptical", no_argument, 0, 'e'},
	{"negflip", no_argument, &negflip, true},
	{"meanf0", no_argument, &meanf0, true},
	{"help", no_argument, 0, 'h'},
	{"mask", required_argument, 0, 'm'},
	{"verbose", no_argument, 0, 'v'},
	{"no-prompt", no_argument, 0, 'n'},
	{"algo", required_argument, 0, 'a'},
	{"its", required_argument, 0, 'i'},
	{"threads", required_argument, 0, 'T'},
	{0, 0, 0, 0}
};
static const char *short_opts = "hm:o:b:vna:i:T:e";

//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv)
{
	try { // To fix uncaught exceptions on Mac

	Nifti::File maskFile, B0File, B1File;
	MultiArray<double, 3> maskVol, B1Vol;
	ThreadPool threads;
	string procPath;

	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, &indexptr)) != -1) {
		switch (c) {
			case 'o':
				outPrefix = optarg;
				if (verbose) cout << "Output prefix will be: " << outPrefix << endl;
				break;
			case 'm':
				if (verbose) cout << "Reading mask file " << optarg << endl;
				maskFile.open(optarg, Nifti::Mode::Read);
				maskVol.resize(maskFile.matrix());
				maskFile.readVolumes(maskVol.begin(), maskVol.end(), 0, 1);
				break;
			case 'b':
				if (verbose) cout << "Reading B1 file: " << optarg << endl;
				B1File.open(optarg, Nifti::Mode::Read);
				B1Vol.resize(B1File.matrix());
				B1File.readVolumes(B1Vol.begin(), B1Vol.end(), 0, 1);
				break;
			case 'a':
				switch (*optarg) {
					case 'l': algo = Algos::LLS;  if (verbose) cout << "LLS algorithm selected." << endl; break;
					case 'w': algo = Algos::WLLS; if (verbose) cout << "WLLS algorithm selected." << endl; break;
					case 'n': algo = Algos::NLLS; if (verbose) cout << "NLLS algorithm selected." << endl; break;
					default:
						cout << "Unknown algorithm type " << optarg << endl;
						return EXIT_FAILURE;
						break;
				} break;
			case 'i':
				nIterations = atoi(optarg);
				break;
			case 'v': verbose = true; break;
			case 'n': prompt = false; break;
			case 'e': elliptical = true; break;
			case 'T':
				threads.resize(atoi(optarg));
				break;
			case 0:
				// Just a flag
				break;
			case '?': // getopt will print an error message
			case 'h':
				cout << usage << endl;				
				return EXIT_SUCCESS;
		}
	}
	if (verbose) cout << version << endl << credit_shared << endl;
	Eigen::initParallel();
	if ((argc - optind) != 2) {
		cout << "Wrong number of arguments. Need a T1 map and 1 SSFP file." << endl;
		cout << usage << endl;
		return EXIT_FAILURE;
	}
	if (verbose) cout << "Reading T1 Map from: " << argv[optind] << endl;
	Nifti::File T1File(argv[optind++]);
	MultiArray<double, 3> T1Vol(T1File.matrix());
	T1File.readVolumes(T1Vol.begin(), T1Vol.end(), 0, 1);
	//**************************************************************************
	// Gather SSFP Data
	//**************************************************************************
	Sequences ssfp(Scale::None);
	if (verbose) cout << "Opening SSFP file: " << argv[optind] << endl;
	Nifti::File SSFPFile(argv[optind++]);
	if (verbose) cout << "Checking headers for consistency." << endl;
	checkHeaders(SSFPFile.header(), {T1File, maskFile, B1File});
	if (verbose) cout << "Checking for ProcPar." << endl;
	Agilent::ProcPar pp; ReadPP(SSFPFile, pp);
	if (verbose) cout << "Reading sequence parameters." << endl;
	if (elliptical) {
		ssfp.addSequence(SequenceType::SSFP_Ellipse, prompt, pp);
	} else {
		ssfp.addSequence(SequenceType::SSFP, prompt, pp);
	}
	if (ssfp.size() != SSFPFile.header().dim(4)) {
		throw(std::runtime_error("The specified number of flip-angles and phase-cycles does not match the input file: " + SSFPFile.imagePath()));
	}
	if (verbose) {
		cout << ssfp;
		cout << "Reading data." << endl;
	}
	MultiArray<complex<double>, 4> ssfpVols(SSFPFile.header().fulldims().head(4));
	SSFPFile.readVolumes(ssfpVols.begin(), ssfpVols.end());
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
	const auto dims = SSFPFile.matrix();
	double TR = ssfp.sequence(0)->m_TR;
	MultiArray<float, 3> T2Vol(dims), PDVol(dims), SoSVol(dims);
	size_t nf0 = 0;
	if ((elliptical) && (meanf0)) {
		nf0 = 1;
	} else if (elliptical) {
		nf0 = ssfp.size();
	}
	MultiArray<float, 4> offResVols(dims, nf0);
	time_t startTime;
	if (verbose)
		startTime = printStartTime();
	clock_t startClock = clock();
	int voxCount = 0;
	for (size_t k = 0; k < dims(2); k++) {
		if (verbose)
			cout << "Starting slice " << k << "..." << flush;
		clock_t loopStart = clock();
		atomic<int> sliceCount{0};
		function<void (const int&)> process = [&] (const int &j) {
			for (size_t i = 0; i < dims(0); i++) {
				if (!maskFile || (maskVol[{i,j,k}])) {
					sliceCount++;
					double B1, T1, T2, E1, E2, PD, offRes, SoS;
					B1 = B1File ? B1Vol[{i,j,k}] : 1.;
					T1 = T1Vol[{i,j,k}];
					E1 = exp(-TR / T1);
					const ArrayXd localAngles(ssfp.sequence(0)->B1flip(B1));
					ArrayXcd data = ssfpVols.slice<1>({i,j,k,0},{0,0,0,-1}).asArray().cast<complex<double>>();
					const ArrayXd s = data.abs();
					VectorXd Y = s / localAngles.sin();
					MatrixXd X(Y.rows(), 2);
					X.col(0) = s / localAngles.tan();
					X.col(1).setOnes();
					VectorXd b = (X.transpose() * X).partialPivLu().solve(X.transpose() * Y);
					if (elliptical) {
						T2 = 2. * TR / log((b[0]*E1 - 1.) / (b[0] - E1));
						E2 = exp(-TR / T2);
						PD = b[1] * (1. - E1*E2*E2) / (sqrt(E2) * (1. - E1));
					} else {
						T2 = TR / log((b[0]*E1 - 1.)/(b[0] - E1));
						E2 = exp(-TR / T2);
						PD = b[1] * (1. - E1*E2) / (1. - E1);
					}
					if (algo == Algos::WLLS) {
						VectorXd W(ssfp.size());
						for (size_t n = 0; n < nIterations; n++) {
							if (elliptical) {
								W = ((1. - E1*E2) * localAngles.sin() / (1. - E1*E2*E2 - (E1 - E2*E2)*localAngles.cos())).square();
							} else {
								W = ((1. - E1*E2) * localAngles.sin() / (1. - E1*E2 - (E1 - E2)*localAngles.cos())).square();
							}
							b = (X.transpose() * W.asDiagonal() * X).partialPivLu().solve(X.transpose() * W.asDiagonal() * Y);
							if (elliptical) {
								T2 = 2. * TR / log((b[0]*E1 - 1.) / (b[0] - E1));
								E2 = exp(-TR / T2);
								PD = b[1] * (1. - E1*E2*E2) / (sqrt(E2) * (1. - E1));
							} else {
								T2 = TR / log((b[0]*E1 - 1.)/(b[0] - E1));
								E2 = exp(-TR / T2);
								PD = b[1] * (1. - E1*E2) / (1. - E1);
							}
						}
					} else if (algo == Algos::NLLS) {
						D2Functor f(T1, ssfp, Pools::One, data, B1, true, false);
						NumericalDiff<D2Functor> nDiff(f);
						LevenbergMarquardt<NumericalDiff<D2Functor>> lm(nDiff);
						lm.parameters.maxfev = nIterations;
						VectorXd p(3);
						p << PD, T2, offRes;
						//cout << "Running LM..." << endl;
						//cout << "Start P: " << p.transpose() << endl;
						lm.lmder1(p);
						//cout << "End P: " << p.transpose() << endl;
						//cout << "Finished LM" << endl;
						PD = p(0); T2 = p(1); offRes = p(2);
						//exit(EXIT_SUCCESS);
					}
					ArrayXd theory = ssfp.signal(Pools::One, Vector4d(PD, T1, T2, offRes), B1).abs();
					SoS = (s - theory).abs2().sum() / ssfp.size();
					T2Vol[{i,j,k}]  = static_cast<float>(T2);
					PDVol[{i,j,k}]  = static_cast<float>(PD);
					SoSVol[{i,j,k}] = static_cast<float>(SoS);

					if (elliptical) {
						if (negflip)
							data = -data;
						if (meanf0) {
							// Use phase of mean instead of mean of phase to avoid wrap issues
							offResVols[{i,j,k,0}] = static_cast<float>(arg(data.mean()) / (M_PI * TR));
						} else {
							offResVols.slice<1>({i,j,k,0},{0,0,0,-1}).asArray() = (data.imag().binaryExpr(data.real(), ptr_fun<double,double,double>(atan2))).cast<float>() / (M_PI * TR);
						}
					}
				}
			}
		};
		threads.for_loop(process, dims(1));
		if (verbose) printLoopTime(loopStart, sliceCount);
		voxCount += sliceCount;
	}
	if (verbose) {
		printElapsedTime(startTime);
		printElapsedClock(startClock, voxCount);
		cout << "Writing results." << endl;
	}
	Nifti::Header outHdr = SSFPFile.header();
	outHdr.description = version;
	outHdr.setDim(4, 1);
	outHdr.setDatatype(Nifti::DataType::FLOAT32);
	outHdr.intent = Nifti::Intent::Estimate;
	outHdr.intent_name = "T2 (s)";
	Nifti::File outFile(outHdr, outPrefix + "D2_T2" + OutExt());
	outFile.writeVolumes(T2Vol.begin(), T2Vol.end());
	outFile.close();
	outHdr.intent_name = "PD (au)";
	outFile.open(outPrefix + "D2_PD" + OutExt(), Nifti::Mode::Write);
	outFile.writeVolumes(PDVol.begin(), PDVol.end());
	outFile.close();
	outHdr.intent_name = "Sum of Squared Residuals";
	outFile.open(outPrefix + "D2_SoS" + OutExt(), Nifti::Mode::Write);
	outFile.writeVolumes(SoSVol.begin(), SoSVol.end());
	outFile.close();
	if (elliptical) {
		outHdr.setDim(4, nf0);
		outHdr.intent_name = "Off-resonance (Hz)";
		outFile.setHeader(outHdr);
		outFile.open(outPrefix + "D2_f0" + OutExt(), Nifti::Mode::Write);
		outFile.writeVolumes(offResVols.begin(), offResVols.end(), 0, nf0);
		outFile.close();
	}
	if (verbose) cout << "All done." << endl;
	} catch (exception &e) {
		cerr << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
