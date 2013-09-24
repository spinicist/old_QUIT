/*
 *  despot2fm_main.cpp
 *
 *  Created by Tobias Wood on 2013/08/12.
 *  Copyright (c) 2013 Tobias Wood.
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
#include <Eigen/Dense>

#include "Nifti/Nifti.h"
#include "DESPOT.h"
#include "DESPOT_Functors.h"
#include "RegionContraction.h"
#include "ThreadPool.h"

#ifdef AGILENT
	#include "procpar.h"
#endif

using namespace std;
using namespace Eigen;

//******************************************************************************
// Arguments / Usage
//******************************************************************************
const string credit {
"despot2 - Written by tobias.wood@kcl.ac.uk, based on work by Sean Deoni. \n\
Acknowledgements greatfully received, grant discussions welcome."
};

const string usage {
"Usage is: despot2-fm [options] T1_map ssfp_files\n\
\
Options:\n\
	--help, -h        : Print this message.\n\
	--mask, -m file   : Mask input with specified file.\n\
	--out, -o path    : Add a prefix to the output filenames.\n\
	--B0 file         : B0 Map file.\n\
	--B1 file         : B1 Map file.\n\
	--verbose, -v     : Print slice processing times.\n\
	--start_slice N   : Start processing from slice N.\n\
	--end_slice   N   : Finish processing at slice N.\n\
	--tesla, -t 3     : Use boundaries suitable for 3T (default)\n\
	            7     : Boundaries suitable for 7T\n\
	            u     : User specified boundaries from stdin.\n\
	--samples, -m n   : Use n samples for region contraction (Default 5000).\n\
	--retain, -r  n   : Retain n samples for new boundary (Default 50).\n\
	--contract, -c n  : Contract a maximum of n times (Default 10).\n\
	--expand, -e n    : Re-expand boundary by percentage n (Default 0).\n"
};

static auto scale = DESPOT2FM::Scaling::MeanPerSignal;
static auto tesla = DESPOT2FM::FieldStrength::Three;
static auto offRes = DESPOT2FM::OffResMode::SingleSymmetric;
static int verbose = false, debug = false, use_finite = false,
		   samples = 2000, retain = 20, contract = 10,
           voxI = -1, voxJ = -1;
static size_t start_slice = 0, end_slice = numeric_limits<size_t>::max();
static double expand = 0., weighting = 1.0;
static string outPrefix;
static struct option long_options[] =
{
	{"B0", required_argument, 0, '0'},
	{"B1", required_argument, 0, '1'},
	{"help", no_argument, 0, 'h'},
	{"mask", required_argument, 0, 'm'},
	{"tesla", required_argument, 0, 't'},
	{"verbose", no_argument, 0, 'v'},
	{"scaling", required_argument, 0, 's'},
	{"start_slice", required_argument, 0, 'S'},
	{"end_slice", required_argument, 0, 'E'},
	{"samples", required_argument, 0, 'm'},
	{"retain", required_argument, 0, 'r'},
	{"contract", required_argument, 0, 'c'},
	{"expand", required_argument, 0, 'e'},
	{0, 0, 0, 0}
};

//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv)
{
	//**************************************************************************
	// Argument Processing
	//**************************************************************************
	cout << credit << endl;
	Eigen::initParallel();
	Nifti maskFile, B0File, B1File;
	vector<double> maskData, B0Data, B1Data, T1Data;
	string procPath;
	
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, "hm:o:vt:s:n:r:c:e:i:j:w:fd", long_options, &indexptr)) != -1) {
		switch (c) {
			case 'o':
				outPrefix = optarg;
				cout << "Output prefix will be: " << outPrefix << endl;
				break;
			case 'm':
				cout << "Reading mask file " << optarg << endl;
				maskFile.open(optarg, Nifti::Mode::Read);
				maskData = maskFile.readVolume<double>(0);
				break;
			case '0':
				if (string(optarg) == "UNSYM") {
					offRes == DESPOT2FM::OffResMode::Single;
				} else {
					cout << "Reading B0 file: " << optarg << endl;
					B0File.open(optarg, Nifti::Mode::Read);
					B0Data = B0File.readVolume<double>(0);
					offRes = DESPOT2FM::OffResMode::Map;
				}
				break;
			case '1':
				cout << "Reading B1 file: " << optarg << endl;
				B1File.open(optarg, Nifti::Mode::Read);
				B1Data = B1File.readVolume<double>(0);
				break;
			case 't':
				switch (*optarg) {
					case '3': tesla = DESPOT2FM::FieldStrength::Three; break;
					case '7': tesla = DESPOT2FM::FieldStrength::Seven; break;
					case 'u': tesla = DESPOT2FM::FieldStrength::Unknown; break;
					default:
						cout << "Unknown boundaries type " << optarg << endl;
						exit(EXIT_FAILURE);
						break;
				} break;
			case 's':
				switch (atoi(optarg)) {
					case 1 : scale = DESPOT2FM::Scaling::Global; break;
					case 2 : scale = DESPOT2FM::Scaling::PerSignal; break;
					case 3 : scale = DESPOT2FM::Scaling::MeanPerSignal; break;
					default:
						cout << "Invalid scaling mode: " + to_string(atoi(optarg)) << endl;
						exit(EXIT_FAILURE);
						break;
				} break;
			case 'i': voxI = atoi(optarg); break;
			case 'j': voxJ = atoi(optarg); break;
			case 'w': weighting = atof(optarg); break;
			case 'v': verbose = true; break;
			case 'f': use_finite = true; break;
			case 'd': debug = true; break;
			case 'S': start_slice = atoi(optarg); break;
			case 'E': end_slice = atoi(optarg); break;
			case 'n': samples  = atoi(optarg); break;
			case 'r': retain   = atoi(optarg); break;
			case 'c': contract = atoi(optarg); break;
			case 'e': expand   = atof(optarg); break;
			case 0:
				// Just a flag
				break;
			case '?': // getopt will print an error message
			case 'h':
				cout << usage << endl;				
				return EXIT_FAILURE;
		}
	}
	if ((argc - optind) < 2) {
		cout << "Wrong number of arguments. Need at least a T1 map and 1 SSFP file." << endl;
		exit(EXIT_FAILURE);
	}
	cout << "Reading T1 Map from: " << argv[optind] << endl;
	Nifti inFile(argv[optind++], Nifti::Mode::Read);
	T1Data = inFile.readVolume<double>(0);
	inFile.close();
	if ((maskFile.isOpen() && !inFile.matchesSpace(maskFile)) ||
	    (B0File.isOpen() && !inFile.matchesSpace(B0File)) ||
		(B1File.isOpen() && !inFile.matchesSpace(B1File))){
		cerr << "Dimensions/transforms do not match in input files." << endl;
		exit(EXIT_FAILURE);
	}
	Nifti templateFile(inFile, 1); // Save header data to write out results
	//**************************************************************************
	// Gather SSFP Data
	//**************************************************************************
	size_t nPhases = argc - optind;
	vector<shared_ptr<SignalFunctor>> sigs;
	vector<vector<double>> ssfpData(nPhases);
	VectorXd inFlip;
	for (size_t p = 0; p < nPhases; p++) {
		cout << "Reading SSFP header from " << argv[optind] << endl;
		inFile.open(argv[optind], Nifti::Mode::Read);
		if (p == 0)
			templateFile = Nifti(inFile, 1);
		if (!inFile.matchesSpace(templateFile)) {
			cerr << "Input file dimensions and/or transforms do not match." << endl;
			exit(EXIT_FAILURE);
		}
		cout << "Reading SSFP data..." << endl;
		sigs.emplace_back(parseSSFP(inFile, true, Components::One));
		ssfpData[p] = inFile.readAllVolumes<double>();
		inFile.close();
		optind++;
	}
	
	if (optind != argc) {
		cerr << "Unprocessed arguments supplied.\n" << usage;
		exit(EXIT_FAILURE);
	}
	
	DESPOT2FM d2fm(sigs, 0., tesla, offRes, scale, use_finite);
	ArrayXd thresh = d2fm.defaultThresholds();
	ArrayXXd bounds = d2fm.defaultBounds();
	if (tesla == DESPOT2FM::FieldStrength::Unknown) {
		cout << "Enter parameter pairs (low then high)" << endl;
		for (int i = 0; i < d2fm.inputs(); i++) {
			cout << d2fm.names()[i] << ": " << flush;
			cin >> bounds(i, 0) >> bounds(i, 1);
		}
	}
	
	if (verbose) {
		for (auto &s : sigs) {
			cout << "SSFP Angles (deg): " << s->m_flip.transpose() * 180 / M_PI << endl;
		}
		cout << "Low bounds: " << bounds.col(0).transpose() << endl
		     << "Hi bounds:  " << bounds.col(1).transpose() << endl;
	}
	//**************************************************************************
	// Set up results data
	//**************************************************************************
	size_t voxelsPerSlice = templateFile.voxelsPerSlice();
	size_t voxelsPerVolume = templateFile.voxelsPerVolume();
	vector<vector<double>> paramsData(d2fm.inputs());
	for (auto &p : paramsData)
		p.resize(voxelsPerVolume);
	vector<vector<double>> residuals(d2fm.values());
	for (auto &r : residuals)
		r.resize(voxelsPerVolume);
	vector<size_t> contractData;
	vector<vector<double>> midpData(d2fm.inputs());
	vector<vector<double>> widthData(d2fm.inputs());
	if (debug) {
		contractData.resize(voxelsPerVolume);
		for (auto &m : midpData)
			m.resize(voxelsPerVolume);
		for (auto &r : widthData)
			r.resize(voxelsPerVolume);
	}
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
	if (end_slice > templateFile.dim(3))
		end_slice = templateFile.dim(3);
	ThreadPool threads;
    time_t procStart = time(NULL);
	char theTime[512];
	strftime(theTime, 512, "%H:%M:%S", localtime(&procStart));
	cout << "Started processing at " << theTime << endl;
	for (size_t slice = start_slice; slice < end_slice; slice++) {
		// Read in data
		if (verbose)
			cout << "Starting slice " << slice << "..." << flush;
		
		atomic<int> voxCount{0};
		const size_t sliceOffset = slice * voxelsPerSlice;
		clock_t loopStart = clock();
		function<void (const size_t&)> processVox = [&] (const size_t &vox) {
			// Set up parameters and constants
			DESPOT2FM locald2(d2fm); // Take a thread local copy of the functor
			ArrayXd params(locald2.inputs()); params.setZero();
			ArrayXd resid(locald2.values()); resid.setZero();
			// Debug stuff
			size_t c = 0;
			ArrayXd width(locald2.inputs()); width.setZero();
			ArrayXd midp(locald2.inputs()); midp.setZero();
			if (!maskFile.isOpen() || ((maskData[sliceOffset + vox] > 0.) && (T1Data[sliceOffset + vox] > 0.)))
			{	// -ve T1 is non-sensical, no point fitting
				voxCount++;
				ArrayXd weights(locald2.values());
				weights.setConstant(1.0);
				for (size_t p = 0; p < nPhases; p++) {
					locald2.m_f0 = B0File.isOpen() ? B0Data[sliceOffset + vox] : 0.;
					locald2.m_B1 = B1File.isOpen() ? B1Data[sliceOffset + vox] : 1.;
					for (int i = 0; i < locald2.actual(p).rows(); i++)
						locald2.actual(p)(i) = ssfpData[p][i*voxelsPerVolume + sliceOffset + vox];
				}
				// DESPOT2-FM
				locald2.rescaleActual();
				locald2.setT1(T1Data.at(sliceOffset + vox));
				RegionContraction<DESPOT2FM> rc(locald2, bounds, weights, thresh,
				                                samples, retain, contract, expand, (voxI != -1));
				// Add the voxel number to the time to get a decent random seed
				size_t rSeed = time(NULL) + vox;
				rc.optimise(params, rSeed);
				resid = rc.residuals();
				if (debug) {
					c = rc.contractions();
					width = rc.width();
					midp = rc.midPoint();
				}
			}
			for (size_t p = 0; p < params.size(); p++) {
				paramsData.at(p).at(sliceOffset + vox) = params(p);
			}
			for (size_t i = 0; i < resid.size(); i++) {
				residuals.at(i).at(sliceOffset + vox) = resid(i);
			}
			if (debug) {
				contractData.at(sliceOffset + vox) = c;
				for (size_t i = 0; i < widthData.size(); i++) {
					widthData.at(i).at(sliceOffset + vox) = width(i);
					midpData.at(i).at(sliceOffset + vox) = midp(i);
				}
			}
		};
		if (voxI == -1)
			threads.for_loop(processVox, voxelsPerSlice);
		else {
			size_t voxInd = templateFile.dim(1) * voxJ + voxI;
			processVox(voxInd);
			exit(0);
		}
		
		if (verbose) {
			clock_t loopEnd = clock();
			if (voxCount > 0)
				cout << voxCount << " unmasked voxels, CPU time per voxel was "
				          << ((loopEnd - loopStart) / ((float)voxCount * CLOCKS_PER_SEC)) << " s, ";
			cout << "finished." << endl;
		}
	}
    time_t procEnd = time(NULL);
    strftime(theTime, 512, "%H:%M:%S", localtime(&procEnd));
	cout << "Finished processing at " << theTime << ". Run-time was " 
	     << difftime(procEnd, procStart) << " s." << endl;
	
	outPrefix = outPrefix + "FM_";
	for (int p = 0; p < d2fm.inputs(); p++) {
		templateFile.open(outPrefix + d2fm.names().at(p) + ".nii.gz", Nifti::Mode::Write);
		templateFile.writeVolume(0, paramsData.at(p));
		templateFile.close();
	}
	templateFile.setDim(4, static_cast<int>(residuals.size()));
	templateFile.open(outPrefix + "residuals.nii.gz", Nifti::Mode::Write);
	for (size_t i = 0; i < residuals.size(); i++)
		templateFile.writeSubvolume(0, 0, 0, i, -1, -1, -1, i+1, residuals[i]);
	templateFile.close();
	if (debug) {
		templateFile.setDim(4, 1);
		templateFile.setDatatype(Nifti::DataType::INT16);
		templateFile.open(outPrefix + "n_contract.nii.gz", Nifti::Mode::Write);
		templateFile.writeVolume(0, contractData);
		templateFile.close();
		templateFile.setDatatype(Nifti::DataType::FLOAT32);
		for (int p = 0; p < d2fm.inputs(); p++) {
			templateFile.open(outPrefix + d2fm.names().at(p) + "_width.nii.gz", Nifti::Mode::Write);
			templateFile.writeVolume(0, widthData.at(p));
			templateFile.close();
			templateFile.open(outPrefix + d2fm.names().at(p) + "_mid.nii.gz", Nifti::Mode::Write);
			templateFile.writeVolume(0, midpData.at(p));
			templateFile.close();
		}
	}
	return EXIT_SUCCESS;
}
