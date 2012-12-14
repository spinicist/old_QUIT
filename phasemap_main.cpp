/*
 *  phasemap_main.cpp
 *
 *  Created by Tobias Wood on 20/06/2012.
 *  Copyright (c) 2012 Tobias Wood. All rights reserved.
 */

#include <string>
#include <getopt.h>

#include "NiftiImage.h"
#include "procpar.h"

const std::string usage
{
"Usage is: phasemap input_1 input_2 outprefix\n\
\n\
Echo times will be read from procpar if present.\n\
Options:\n\
	--mask, -m mask_file : Mask input with specified file\n\
	--phasetime T        : Calculate the phase accumulated in time T\n\
	--smooth             : Smooth output with a gaussian.\n"
};

int main(int argc, char** argv)
{
	//**************************************************************************
	// Argument Processing
	//**************************************************************************
	static int smooth = false;
	static struct option long_options[] =
	{
		{"mask", required_argument, 0, 'm'},
		{"phasetime", required_argument, 0, 'p'},
		{"smooth", no_argument, &smooth, true},
		{0, 0, 0, 0}
	};
	
	int indexptr = 0, c;
	std::string procPath;
	par_t *pars;
	double TE1, TE2, deltaTE, phasetime = 0.;
	double *data1, *data2, *B0, *mask = NULL;
	NiftiImage inFile, outFile;
	while ((c = getopt_long(argc, argv, "m:", long_options, &indexptr)) != -1) {
		switch (c) {
			case 'm':
				std::cout << "Reading mask from " << optarg << std::endl;
				inFile.open(optarg, NiftiImage::NIFTI_READ);
				mask = inFile.readVolume<double>(0);
				inFile.close();
				break;
			case 'p':
				phasetime = atof(optarg);
				break;
		}
	}
	if ((argc - optind) == 2) {
		std::cout << "Opening input file " << argv[optind] << "." << std::endl;
		inFile.open(argv[optind], NiftiImage::NIFTI_READ);
		procPath = inFile.basename() + ".procpar";
		if ((pars = readProcpar(procPath.c_str()))) {
			TE1 = realVal(pars, "te", 0);
			TE2 = realVal(pars, "te", 1);
		} else {
			std::cout << "Enter TE2 & TE2 (seconds): ";
			std::cin >> TE1 >> TE2;
		}
		data1 = inFile.readVolume<double>(0);
		data2 = inFile.readVolume<double>(1);
		inFile.close();
	} else if ((argc - optind) == 3) {
		std::cout << "Opening input file 1" << argv[optind] << "." << std::endl;
		inFile.open(argv[optind], NiftiImage::NIFTI_READ);
		procPath = inFile.basename() + ".procpar";
		if ((pars = readProcpar(procPath.c_str())))
			TE1 = realVal(pars, "te", 0);
		else {
			std::cout << "Enter TE1 (seconds): ";
			std::cin >> TE1;
		}
		data1 = inFile.readVolume<double>(0);
		inFile.close();
		std::cout << "Opening input file 2" << argv[++optind] << "." << std::endl;
		inFile.open(argv[optind], NiftiImage::NIFTI_READ);
		procPath = inFile.basename() + ".procpar";
		if ((pars = readProcpar(procPath.c_str())))
			TE2 = realVal(pars, "te", 0);
		else {
			std::cout << "Enter TE2 (seconds): ";
			std::cin >> TE2;
		}
		data2 = inFile.readVolume<double>(1);
		inFile.close();
	} else {
		std::cerr << usage << std::endl;
		exit(EXIT_FAILURE);
	}
	std::string outPrefix(argv[++optind]);
	if (TE2 < TE1) {	// Swap them
		fprintf(stdout, "TE2 < TE1, swapping.\n");
		double *tmp = data1;
		data1 = data2;
		data2 = tmp;
		double tmpTE = TE2;
		TE2 = TE1;
		TE1 = tmpTE;
	}
	deltaTE = TE2 - TE1;
	std::cout << "Delta TE = " << deltaTE << std::endl;
	B0 = (double *)malloc(inFile.voxelsPerVolume() * sizeof(double));
	std::cout << "Processing..." << std::endl;
	for (size_t vox = 0; vox < inFile.voxelsPerVolume(); vox++) {
		if (!mask || mask[vox] > 0.) {
			double deltaPhase = data2[vox] - data1[vox];
			B0[vox] = deltaPhase / (2 * M_PI * deltaTE);
			if (phasetime > 0.) {
				double ph = fmod(B0[vox] * 2 * M_PI * phasetime, 2 * M_PI);
				if (ph > M_PI) ph -= (2 * M_PI);
				if (ph < -M_PI) ph += (2 * M_PI);
				B0[vox] = ph;
			}
		}
	}
	std::cout << "Writing B0 map." << std::endl;
	std::string outPath = outPrefix + "_B0.nii.gz";
	outFile = inFile;
	outFile.setnt(1);
	outFile.setDatatype(NIFTI_TYPE_FLOAT32);
	outFile.open(outPath, NiftiImage::NIFTI_WRITE);
	outFile.writeVolume(0, B0);
	outFile.close();
	std::cout << "Finished." << std::endl;
    return EXIT_SUCCESS;
}

