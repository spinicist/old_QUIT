//
//  main.cpp
//  dixon
//
//  Created by Tobias Wood on 25/03/2014.
//
//

#include <time.h>
#include <getopt.h>
#include <iostream>
#include <atomic>
#include "Eigen/Dense"

#include "Nifti/Nifti.h"
#include "QUIT/Volume.h"
#include "QUIT/ThreadPool.h"

using namespace std;
using namespace Eigen;
using namespace QUIT;

//******************************************************************************
// Arguments / Usage
//******************************************************************************
const string usage {
"Usage is: dixon [options] magnitude phase \n\
\
Options:\n\
	--help, -h        : Print this message\n\
	--verbose, -v     : Print more information\n\
	--out, -o path    : Add a prefix to the output filenames\n"
};

static bool verbose = false;
static string outPrefix;
static struct option long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"verbose", no_argument, 0, 'v'},
	{"mask", required_argument, 0, 'm'},
	{"out", required_argument, 0, 'o'},
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
	Nifti maskFile;
	MultiArray<int8_t, 3> maskVol;
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, "hvm:o:", long_options, &indexptr)) != -1) {
		switch (c) {
			case 'v': verbose = true; break;
			case 'm':
				cout << "Reading mask file " << optarg << endl;
				maskFile.open(optarg, Nifti::Mode::Read);
				maskVol.resize(maskFile.dims());
				maskFile.readVolumes(maskVol.begin(), maskVol.end(), 0, 1);
				break;
			case 'o':
				outPrefix = optarg;
				cout << "Output prefix will be: " << outPrefix << endl;
				break;
			case 'h':
			case '?': // getopt will print an error message
				exit(EXIT_FAILURE);
		}
	}
	//**************************************************************************
	#pragma mark Gather data
	//**************************************************************************
	if ((argc - optind) != 2) {
		cout << "Requires 1 magnitude file and 1 phase file with 3 echos each as input." << endl << usage << endl;
		exit(EXIT_FAILURE);
	}

	cout << "Opening magnitude file: " << argv[optind] << endl;
	Nifti inputFile;
	inputFile.open(argv[optind++], Nifti::Mode::Read);
	Nifti templateFile(inputFile, 1);
	MultiArray<float, 4> mag{inputFile.dims()};
	inputFile.readVolumes(mag.begin(), mag.end());
	inputFile.close();

	cout << "Opening phase file: " << argv[optind] << endl;
	inputFile.open(argv[optind++], Nifti::Mode::Read);
	MultiArray<float, 4> phase{inputFile.dims()};
	inputFile.readVolumes(phase.begin(), phase.end());
	if (!templateFile.matchesSpace(inputFile) || (maskFile.isOpen() && !templateFile.matchesSpace(maskFile))) {
		cerr << "Input file dimensions or orientations do not match." << endl;
		exit(EXIT_FAILURE);
	}
	inputFile.close();

	Nifti::ArrayXs dims = templateFile.dims().head(3);
	MultiArray<float, 3> Wv(dims), Fv(dims), Av(dims);
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
	ThreadPool pool;
	cout << "Starting processing..." << endl;
	auto S0 = mag.slice<3>({0,0,0,0},{-1,-1,-1,0});
	auto S1 = mag.slice<3>({0,0,0,1},{-1,-1,-1,0});
	auto S2 = mag.slice<3>({0,0,0,2},{-1,-1,-1,0});
	auto phi0 = phase.slice<3>({0,0,0,0},{-1,-1,-1,0});
	auto phi1 = phase.slice<3>({0,0,0,1},{-1,-1,-1,0});
	auto phi2 = phase.slice<3>({0,0,0,2},{-1,-1,-1,0});
	for (size_t k = 0; k < S0.dims()[2]; k++) {
		clock_t loopStart;
		// Read in data
		if (verbose)
			cout << "Starting slice " << k << "..." << flush;
		loopStart = clock();
		atomic<int> voxCount{0};
		//cout << endl << I0s << endl << I1s << endl << I2s << endl;
		//cout << Ws << endl << Fs << endl << As << endl;
		function<void (const size_t)> processVox = [&] (const size_t j) {
			for (size_t i = 0; i < S0.dims()[0]; i++)
				if (!maskFile.isOpen() || maskVol[{i,j,k}]) {
					// From Ma et al JMR 1997
					Av[{i,j,k}] = sqrt(S2[{i,j,k}] / S0[{i,j,k}]);
					float phi = (phi2[{i,j,k}] - phi0[{i,j,k}]) / 2.;
					float psi = cos((phi1[{i,j,k}] - phi0[{i,j,k}]) - phi);
					float frac = S1[{i,j,k}] / sqrt(S0[{i,j,k}]*S2[{i,j,k}]);
					Wv[{i,j,k}] = (1 + psi * frac) * S0[{i,j,k}] / 2.;
					Fv[{i,j,k}] = (1 - psi * frac) * S0[{i,j,k}] / 2.;
				}
		};
		pool.for_loop(processVox, S0.dims()[1]);
		
		if (verbose) {
			clock_t loopEnd = clock();
			if (voxCount > 0)
				cout << voxCount << " unmasked voxels, CPU time per voxel was "
				          << ((loopEnd - loopStart) / ((float)voxCount * CLOCKS_PER_SEC)) << " s, ";
			cout << "finished." << endl;
		}
	}

	if (verbose)
		cout << "Writing results." << endl;
	templateFile.open(outPrefix + "W.nii.gz", Nifti::Mode::Write);
	templateFile.writeVolumes(Wv.begin(), Wv.end());
	templateFile.close();
	templateFile.open(outPrefix + "F.nii.gz", Nifti::Mode::Write);
	templateFile.writeVolumes(Fv.begin(), Fv.end());
	templateFile.close();
	templateFile.open(outPrefix + "A.nii.gz", Nifti::Mode::Write);
	templateFile.writeVolumes(Av.begin(), Av.end());
	templateFile.close();
	cout << "All done." << endl;
	exit(EXIT_SUCCESS);
}

