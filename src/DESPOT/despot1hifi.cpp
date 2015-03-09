/*
 *  despot1hifi.cpp
 *
 *  Created by Tobias Wood on 17/10/2011.
 *  Copyright (c) 2011-2013 Tobias Wood.
 *
 *  Based on code by Sean Deoni
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <time.h>
#include <getopt.h>
#include <iostream>
#include <atomic>
#include "Eigen/Dense"

#include "Nifti/Nifti.h"
#include "DESPOT.h"
#include "Model.h"
#include "DESPOT_Functors.h"
#include "QUIT/QUIT.h"

using namespace std;
using namespace Eigen;
using namespace QUIT;

//******************************************************************************
// Arguments / Usage
//******************************************************************************
const string usage {
"Usage is: despot1hifi [options] spgr_input ir-spgr_input\n\
\
Options:\n\
	--help, -h        : Print this message\n\
	--verbose, -v     : Print more information\n\
	--no-prompt, -n   : Suppress input prompts\n\
	--out, -o path    : Add a prefix to the output filenames\n\
	--mask, -m file   : Mask input with specified file\n\
	--thresh, -t n    : Threshold maps at PD < n\n\
	--clamp, -c n     : Clamp T1 between 0 and n\n\
	--start, -s N     : Start processing from slice N\n\
	--stop, -p  N     : Stop processing at slice N\n\
	--its, -i N       : Max iterations for NLLS (default 4)\n\
	--threads, -T N   : Use N threads (default=hardware limit)\n"
};

static bool verbose = false, prompt = true;
static size_t nIterations = 4;
static string outPrefix;
static double thresh = -numeric_limits<double>::infinity();
static double clamp_lo = -numeric_limits<double>::infinity(), clamp_hi = numeric_limits<double>::infinity();
static size_t start_slice = 0, stop_slice = numeric_limits<size_t>::max();
static const struct option long_opts[] = {
	{"help", no_argument, 0, 'h'},
	{"verbose", no_argument, 0, 'v'},
	{"no-prompt", no_argument, 0, 'n'},
	{"mask", required_argument, 0, 'm'},
	{"out", required_argument, 0, 'o'},
	{"thresh", required_argument, 0, 't'},
	{"clamp", required_argument, 0, 'c'},
	{"start", required_argument, 0, 's'},
	{"stop", required_argument, 0, 'p'},
	{"its", required_argument, 0, 'i'},
	{"threads", required_argument, 0, 'T'},
	{0, 0, 0, 0}
};
static const char *short_opts = "hvnm:o:t:c:s:p:i:T:";

//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv) {
	//**************************************************************************
	// Argument Processing
	//**************************************************************************
	cout << version << endl << "Improved formulas thanks to Michael Thrippleton." << endl;
	Eigen::initParallel();
	Nifti::File maskFile, spgrFile, irFile;
	MultiArray<int8_t, 3> maskVol;
	ThreadPool threads;

	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, &indexptr)) != -1) {
		switch (c) {
			case 'v': verbose = true; break;
			case 'n': prompt = false; break;
			case 'm':
				cout << "Opening mask file: " << optarg << endl;
				maskFile.open(optarg, Nifti::Mode::Read);
				maskVol.resize(maskFile.matrix());
				maskFile.readVolumes(maskVol.begin(), maskVol.end(), 0, 1);
				break;
			case 'o':
				outPrefix = optarg;
				cout << "Output prefix will be: " << outPrefix << endl;
				break;
			case 't': thresh = atof(optarg); break;
			case 'c':
				clamp_lo = 0;
				clamp_hi = atof(optarg);
				break;
			case 's': start_slice = atoi(optarg); break;
			case 'p': stop_slice = atoi(optarg); break;
			case 'i':
				nIterations = atoi(optarg);
				break;
			case 'T':
				threads.resize(atoi(optarg));
				break;
			case 'h':
			case '?': // getopt will print an error message
				cout << usage << endl;
				return EXIT_FAILURE;
		}
	}
	if ((argc - optind) != 2) {
		cout << "Incorrect number of arguments." << endl << usage << endl;
		return EXIT_FAILURE;
	}
	
	cout << "Opening SPGR file: " << argv[optind] << endl;
	spgrFile.open(argv[optind], Nifti::Mode::Read);	
	Agilent::ProcPar pp; ReadPP(spgrFile, pp);
	shared_ptr<Sequence> spgrSequence = make_shared<SPGRSimple>(prompt, pp);
	
	cout << "Opening IR-SPGR file: " << argv[++optind] << endl;
	irFile.open(argv[optind], Nifti::Mode::Read);
	shared_ptr<MPRAGE> mprage = make_shared<MPRAGE>(prompt);

	SequenceGroup combined(Scale::None);
	combined.addSequence(spgrSequence);
	combined.addSequence(mprage);

	checkHeaders(spgrFile.header(),{irFile, maskFile});
	if (verbose) {
		cout << combined << endl;
	}

	if (verbose) cout << "Reading image data..." << flush;
	const auto dims = spgrFile.matrix();
	MultiArray<float, 4> SPGR_Vols(dims, spgrFile.dim(4));
	MultiArray<float, 4> IR_Vols(dims, irFile.dim(4));
	spgrFile.readVolumes(SPGR_Vols.begin(), SPGR_Vols.end());
	irFile.readVolumes(IR_Vols.begin(), IR_Vols.end());
	spgrFile.close();
	irFile.close();
	MultiArray<float, 3> PD_Vol(dims), T1_Vol(dims), B1_Vol(dims), res_Vol(dims);
	if (stop_slice > dims[2])
		stop_slice = dims[2];

	for (size_t k = start_slice; k < stop_slice; k++) {
		clock_t loopStart;
		// Read in data
		if (verbose) cout << "Starting slice " << k << "..." << flush;
		loopStart = clock();
		atomic<int> voxCount{0};
		function<void (const size_t&, const size_t&)> processVox = [&] (const size_t &j, const size_t &i) {
			double T1 = 0., PD = 0., B1 = 1., res = 0.; // Assume B1 field is uniform for classic DESPOT
			const MultiArray<float, 3>::Index idx{i,j,k};
			if (!maskFile || (maskVol[idx] > 0.)) {
				voxCount++;
				ArrayXd spgrSig = SPGR_Vols.slice<1>({i,j,k,0},{0,0,0,-1}).asArray().abs().cast<double>();

				// Get a first guess with DESPOT1
				VectorXd Y = spgrSig / spgrSequence->B1flip(1.0).sin();
				MatrixXd X(Y.rows(), 2);
				X.col(0) = spgrSig / spgrSequence->B1flip(1.0).tan();
				X.col(1).setOnes();
				VectorXd b = (X.transpose() * X).partialPivLu().solve(X.transpose() * Y);
				T1 = -spgrSequence->m_TR / log(b[0]);
				PD = b[1] / (1. - b[0]);

				ArrayXd irSig = IR_Vols.slice<1>({i,j,k,0},{0,0,0,-1}).asArray().abs().cast<double>();
				ArrayXd combinedSig(combined.size());
				combinedSig.head(spgrSig.size()) = spgrSig;
				combinedSig.tail(irSig.size()) = irSig;

				// Now add in IR-SPGR data and do a Lev-Mar fit
				HIFIFunctor f(combined, combinedSig, false);
				NumericalDiff<HIFIFunctor> nDiff(f);
				LevenbergMarquardt<NumericalDiff<HIFIFunctor>> lm(nDiff);
				lm.setMaxfev(nIterations);
				VectorXd p(3);
				p << PD, T1, B1; // Don't need T2 of f0 for this (yet)
				lm.lmder1(p);
				PD = p(0); T1 = p(1); B1 = p(2);
				if (PD < thresh) {
					PD = 0.;
					T1 = 0.;
					B1 = 0.;
				}
				T1 = clamp(T1, clamp_lo, clamp_hi);
				ArrayXd theory = combined.signal(Pools::One, Vector2d(PD, T1), B1).abs();
				ArrayXd resids = (combinedSig - theory);
				res = resids.square().sum();
			}
			PD_Vol[idx] = PD;
			T1_Vol[idx] = T1;
			B1_Vol[idx] = B1;
			res_Vol[idx] = res;
		};
		threads.for_loop2(processVox, dims(1), dims(0));
		
		if (verbose) {
			clock_t loopEnd = clock();
			if (voxCount > 0)
				cout << voxCount << " unmasked voxels, CPU time per voxel was "
				          << ((loopEnd - loopStart) / ((float)voxCount * CLOCKS_PER_SEC)) << " s, ";
			cout << "finished." << endl;
		}

		if (threads.interrupted())
			break;

	}
	
	//**************************************************************************
	#pragma mark Write out data
	//**************************************************************************
	Nifti::Header outHdr = spgrFile.header();
	outPrefix = outPrefix + "HIFI_";
	outHdr.description = version;
	outHdr.setDim(4, 1);
	outHdr.setDatatype(Nifti::DataType::FLOAT32);
	outHdr.intent = Nifti::Intent::Estimate;
	outHdr.intent_name = "T1 (seconds)";
	Nifti::File outFile(outHdr, outPrefix + "T1" + OutExt());
	outFile.writeVolumes(T1_Vol.begin(), T1_Vol.end());
	outFile.close();
	outHdr.intent_name = "PD (au)";
	outFile.setHeader(outHdr);
	outFile.open(outPrefix + "PD" + OutExt(), Nifti::Mode::Write);
	outFile.writeVolumes(PD_Vol.begin(), PD_Vol.end());
	outFile.close();
	outHdr.intent_name = "B1 Field Ratio";
	outFile.setHeader(outHdr);
	outFile.open(outPrefix + "B1" + OutExt(), Nifti::Mode::Write);
	outFile.writeVolumes(B1_Vol.begin(), B1_Vol.end());
	outFile.close();
	outHdr.intent_name = "Fractional Residual";
	outFile.setHeader(outHdr);
	outFile.open(outPrefix + "residual" + OutExt(), Nifti::Mode::Write);
	outFile.writeVolumes(res_Vol.begin(), res_Vol.end());
	outFile.close();
	if (verbose) cout << "Finished." << endl;
	return EXIT_SUCCESS;
}
