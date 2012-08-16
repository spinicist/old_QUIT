/*
 *  mcdespot_main.c
 *  Fitting
 *
 *  Created by Tobias Wood on 14/02/2012.
 *  Copyright 2012 Tobias Wood. All rights reserved.
 *
 */

#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <dispatch/dispatch.h>
#include <getopt.h>
#include <signal.h>

#ifdef __APPLE__
	#include <libkern/OSAtomic.h>
	#define AtomicAdd OSAtomicAdd32
#else
	#define AtomicAdd(x, y) (*y)++
#endif

#include "DESPOT.h"
#include "fslio.h"
#include "procpar.h"

//******************************************************************************
// Constants
//******************************************************************************
const char *usage = "Usage is: mcdespot [options] output_prefix spgr_file ssfp_file1 (ssfp_fileN)\n\
\
Options:\n\
	-m file   : Mask input with specified file.\n\
	-z        : Output .nii.gz files.\n\
	--M0 file : M0 Map file.\n\
	--B0 file : B0 Map file.\n\
	--B1 file : B1 Map file.\n\
	-b 3      : Boundaries suitable for 3T (default)\n\
	   7      : Boundaries suitable for 7T\n\
	   u      : User specified boundaries from stdin.\n";

//******************************************************************************
// SIGTERM interrupt handler - for ensuring data gets saved even on a ctrl-c
//******************************************************************************
#define NR 8
FSLIO *resultsHeaders[NR] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
double *resultsData[NR];
const char *names[NR] = { "_T1_short", "_T1_long", "_T2_short", "_T2_long", "_frac_short", "_tau_short", "_mc_B0", "_mc_res" };
void int_handler(int sig);
void int_handler(int sig)
{
	fprintf(stdout, "Processing terminated. Writing currently processed data.\n");
	for (size_t r = 0; r < NR; r++)
	{
		FslWriteHeader(resultsHeaders[r]);
		FslWriteVolumeFromDouble(resultsHeaders[r], resultsData[r], 0);
		FslClose(resultsHeaders[r]);
	}
	exit(EXIT_FAILURE);
}


//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv)
{
	//**************************************************************************
	// Argument Processing
	//**************************************************************************
	if (argc < 4)
	{
		fprintf(stderr, "%s", usage);
		exit(EXIT_FAILURE);
	}
	
	const char *outPrefix = NULL, *outExt = ".nii.gz";
	char procpar[MAXSTR];
	size_t nSPGR, nPhases, nSSFP;
	double spgrTR, ssfpTR,
	       *maskData = NULL, *M0Data = NULL, *B0Data = NULL, *B1Data = NULL,
		   loUser[7], hiUser[7];
	ArrayXd loBounds(7), hiBounds(7);
	ArrayXi loConstraints(7), hiConstraints(7);
	loConstraints << true, true, true, true, true, true, false;
	hiConstraints << true, true, true, true, true, true, false;
	FSLIO *inFile = NULL, *spgrFile = NULL, **ssfpFiles;
	short nx, ny, nz, nt;
	par_t *pars;
	static int verbose = false, start_slice = -1, end_slice = -1;
	static struct option long_options[] =
	{
		{"B0", required_argument, 0, '0'},
		{"B1", required_argument, 0, '1'},
		{"M0", required_argument, 0, 'M'},
		{"mask", required_argument, 0, 'm'},
		{"verbose", no_argument, 0, 'v'},
		{"start_slice", required_argument, 0, 'S'},
		{"end_slice", required_argument, 0, 'E'},
		{0, 0, 0, 0}
	};
	
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, "b:m:vz", long_options, &indexptr)) != -1)
	{
		switch (c)
		{
			case 'm':
				fprintf(stdout, "Reading mask file %s.\n", optarg);
				inFile = FslOpen(optarg, "rb");
				maskData = FslGetVolumeAsScaledDouble(inFile, 0);
				FslClose(inFile);
				break;
			case '0':
				fprintf(stdout, "Reading B0 file %s.\n", optarg);
				inFile = FslOpen(optarg, "rb");
				B0Data = FslGetVolumeAsScaledDouble(inFile, 0);
				FslClose(inFile);
				break;
			case '1':
				fprintf(stdout, "Reading B1 file %s.\n", optarg);
				inFile = FslOpen(optarg, "rb");
				B1Data = FslGetVolumeAsScaledDouble(inFile, 0);
				FslClose(inFile);
				break;
			case 'M':
				fprintf(stdout, "Reading M0 file %s.\n", optarg);
				inFile = FslOpen(optarg, "rb");
				M0Data = FslGetVolumeAsScaledDouble(inFile, 0);
				FslClose(inFile);
				break;
			case 'v':
				verbose = true;
				break;
			case 'S':
				start_slice = atoi(optarg);
				break;
			case 'E':
				end_slice = atoi(optarg);
				break;
			case 'b':
				switch (*optarg)
				{
					case '3':
						fprintf(stdout, "Using 3T boundaries.\n");
						loBounds << 0.200, 0.500, 0.002, 0.040, 0.0, 0.050, 0.;
						hiBounds << 0.700, 2.500, 0.040, 0.200, 0.4, 2.000, 0.;
						break;
					case '7':
						fprintf(stdout, "Using 7T boundaries.\n");
						loBounds << 0.200, 0.500, 0.002, 0.040, 0.,   0.050, 0.;
						hiBounds << 1.500, 5.000, 0.050, 0.400, 0.5,  2.000, 0.;
						break;
					case 'u':
					{
						fprintf(stdout, "Enter low boundaries (%d values):", 7);
						fgetArray(stdin, 'd', 7, loUser);
						Map<ArrayXd> tempLo(loUser, 7);
						loBounds = tempLo;
						fprintf(stdout, "Enter high boundaries (%d values):", 7);
						fgetArray(stdin, 'd', 7, hiUser);
						Map<ArrayXd> tempHi(hiUser, 7);
						hiBounds = tempHi;
					}
						break;
					default:
						fprintf(stdout, "Unknown boundaries type '%s'.\n", optarg);
						exit(EXIT_FAILURE);
						break;
				}
				break;
			case 0:
				// Just a flag
				break;
		}
	}
	if ((argc - optind) < 3)
	{
		fprintf(stderr, "Insufficient number of arguments.\n%s", usage);
		exit(EXIT_FAILURE);
	}
	outPrefix = argv[optind++];
	fprintf(stdout, "Output prefix will be: %s\n", outPrefix);
	//**************************************************************************
	// Gather SPGR Data
	//**************************************************************************
	fprintf(stdout, "Opening SPGR file: %s\n", argv[optind]);
	spgrFile = FslOpen(argv[optind], "rb");
	FslGetDim(spgrFile, &nx, &ny, &nz, &nt);
	nSPGR = nt;
	snprintf(procpar, MAXSTR, "%s.procpar", argv[optind]);
	pars = readProcpar(procpar);
	double spgr_angles[nSPGR];
	if (pars)
	{
		spgrTR = realVal(pars, "tr", 0);
		memcpy(spgr_angles, realVals(pars, "flip1", NULL), nSPGR * sizeof(double));
		freeProcpar(pars);
	}
	else
	{
		fprintf(stdout, "Enter SPGR TR (s):");
		fscanf(stdin, "%lf", &spgrTR);
		fprintf(stdout, "Enter SPGR Flip Angles (degrees):");
		fgetArray(stdin, 'd', nSPGR, spgr_angles);
	}
	Map<ArrayXd> spgrAngles(spgr_angles, nSPGR);
	std::cout << "SPGR TR = " << spgrTR << ". Angles = " << spgrAngles.transpose() << std::endl;
	spgrAngles *= (M_PI / 180.);
	optind++;
	//**************************************************************************
	// Gather SSFP Data
	//**************************************************************************
	nPhases = argc - optind;
	ssfpFiles = (FSLIO **)malloc(nPhases * sizeof(FSLIO *));
	ArrayXd ssfpPhases(nPhases);
	ssfpPhases[0] = M_PI;
	ssfpFiles[0] = FslOpen(argv[optind], "rb");
	nSSFP = ssfpFiles[0]->niftiptr->nt;
	double ssfp_angles[nSSFP];
	snprintf(procpar, MAXSTR, "%s.procpar", argv[optind]);
	pars = readProcpar(procpar);
	if (pars)
	{
		int check;
		fprintf(stdout, "Reading SSFP 180 parameters from procpar.\n");
		memcpy(ssfp_angles, realVals(pars, "flip1", &check), nSSFP * sizeof(double));
		if (check != nSSFP)
		{
			fprintf(stderr, "flip1 and nvols do not match.\n");
			exit(EXIT_FAILURE);
		}
		ssfpTR = realVal(pars, "tr", 0);
		fprintf(stdout, "TR (s): %f, Angles (deg) = ", ssfpTR);
		freeProcpar(pars);
	}
	else
	{
		fprintf(stdout, "Enter %zu SSFP flip angles (degrees) :", nSSFP);
		fgetArray(stdin, 'd', nSSFP, ssfp_angles); fprintf(stdout, "\n");
		fprintf(stdout, "Enter SSFP TR (ms):");
		fscanf(stdin, "%lf", &ssfpTR);		
	}
	Map<ArrayXd> ssfpAngles(ssfp_angles, nSSFP);
	std::cout << "SSFP flip angles: " << ssfpAngles.transpose() << std::endl;
	// Make sure the B0 bounds are sensible
	loBounds(6) = (-0.5 / ssfpTR);
	hiBounds(6) = (0.5 / ssfpTR);	
	for (size_t p = 1; p < nPhases; p++)
	{
		fprintf(stdout, "Reading SSFP header from %s.\n", argv[++optind]);
		ssfpFiles[p] = FslOpen(argv[optind], "rb");
		if (!FslCheckDims(ssfpFiles[0], ssfpFiles[p]))
		{
			fprintf(stderr, "Image %s has differing dimensions.\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		if (ssfpFiles[p]->niftiptr->nt != nSSFP)
		{
			fprintf(stderr, "Image %s has wrong number of flip angles.\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		snprintf(procpar, MAXSTR, "%s.procpar", argv[optind]);
		pars = readProcpar(procpar);
		if (pars)
		{
			ssfpPhases[p] = radians(realVal(pars, "rfphase", 0));
			freeProcpar(pars);
		}
		else
		{
			fprintf(stdout, "Enter phase-cycling (degrees):");
			fscanf(stdin, "%lf", ssfpPhases[p]);
			ssfpPhases[p] *= M_PI / 180.;
		}
	}
	//**************************************************************************	
	// Get input data
	//**************************************************************************
	int voxelsPerSlice = nx * ny;
	int totalVoxels = voxelsPerSlice * nz;
	fprintf(stdout, "Reading SPGR data...\n");
	double *SPGR = FslGetAllVolumesAsScaledDouble(spgrFile);
	fprintf(stdout, "Reading SSFP data...\n");
	double **SSFP = (double **)malloc(nPhases * sizeof(double *));
	for (int p = 0; p < nPhases; p++)
		SSFP[p] = FslGetAllVolumesAsScaledDouble(ssfpFiles[p]);
	//**************************************************************************
	// Create results files
	// T1_m, T1_m, T2_f, T2_f,	f_m, tau_m, residue
	//**************************************************************************
	char outName[strlen(outPrefix) + 32];
	for (int r = 0; r < NR; r++)
	{
		strcpy(outName, outPrefix); strcat(outName, names[r]); strcat(outName, outExt);
		fprintf(stdout, "Creating result header %s\n", outName);
		resultsHeaders[r] = FslOpen(outName, "wb");
		FslCloneHeader(resultsHeaders[r], spgrFile);
		FslSetDim(resultsHeaders[r], nx, ny, nz, 1);
		FslSetDataType(resultsHeaders[r], NIFTI_TYPE_FLOAT32);
		resultsData[r] = (double *)malloc(totalVoxels * sizeof(double));
	}
	signal(SIGINT, int_handler);

	fprintf(stdout, "Starting processing.\n");
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
    time_t procStart = time(NULL);
	if ((start_slice < 0) || (start_slice >= nz))
		start_slice = 0;
	if ((end_slice < 0) || (end_slice > nz))
		end_slice = nz;
	for (size_t slice = start_slice; slice < end_slice; slice++)
	{
		if (verbose)
			fprintf(stdout, "Starting slice %zu...\n", slice);
		__block int voxCount = 0;
		int sliceOffset = slice * voxelsPerSlice;
		clock_t loopStart = clock();
		//for (int vox = 0; vox < voxelsPerSlice; vox++)
		void (^processVoxel)(size_t vox) = ^(size_t vox)
		{
			double M0 = 1, B0 = 0, B1 = 1., residual = 0.;
			if (M0Data)
				M0 = (double)M0Data[sliceOffset + vox];
			if (B0Data)
				B0 = (double)B0Data[sliceOffset + vox];
			if (B1Data)
				B1 = (double)B1Data[sliceOffset + vox];		
			ArrayXd params(7);
			if (!maskData || (maskData[sliceOffset + vox] > 0.))
			{
				AtomicAdd(1, &voxCount);
				ArrayXd SPGR_signals(nSPGR);
				for (int img = 0; img < nSPGR; img++)
					SPGR_signals[img] = SPGR[totalVoxels * img + sliceOffset +  + vox];

				ArrayXXd SSFP_signals(nPhases, nSSFP);
				for (int p = 0; p < nPhases; p++)
				{
					for (int img = 0; img < nSSFP; img++)
						SSFP_signals(p, img) = SSFP[p][totalVoxels * img + sliceOffset +  + vox];
				}
				
				SPGR_2c SPGR(spgrAngles, SPGR_signals, spgrTR, M0, B1);
				SSFP_2c SSFP(ssfpPhases, ssfpAngles, SSFP_signals, ssfpTR, M0, B1);
				
				residual = regionContraction(params, SPGR, SSFP,
				                             loBounds, hiBounds,
											 loConstraints, hiConstraints,
											 1000, 10, 20, 0.1, 0.025);
			}
			for (int p = 0; p < NR - 1; p++)
				resultsData[p][sliceOffset + vox]  = params[p];
			resultsData[NR - 1][sliceOffset + vox] = residual;
		};
		dispatch_queue_t global_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
		dispatch_apply(voxelsPerSlice, global_queue, processVoxel);
		
		if (verbose)
		{
			clock_t loopEnd = clock();
			fprintf(stdout, "Finished slice %zu", slice);
			if (voxCount > 0)
				fprintf(stdout, ", had %d unmasked voxels, CPU time per voxel was %f s.",
						voxCount, (loopEnd - loopStart) / ((float)voxCount * CLOCKS_PER_SEC));
			fprintf(stdout, ".\n");
		}
	}
    time_t procEnd = time(NULL);
    struct tm *localEnd = localtime(&procEnd);
	char theTime[MAXSTR];
    strftime(theTime, MAXSTR, "%H:%M:%S", localEnd);
	fprintf(stdout, "Finished processing at %s. Run-time was %f s.\n", theTime, difftime(procEnd, procStart));

	for (size_t r = 0; r < NR; r++)
	{
		FslWriteHeader(resultsHeaders[r]);
		FslWriteVolumeFromDouble(resultsHeaders[r], resultsData[r], 0);
		FslClose(resultsHeaders[r]);
	}
	// Clean up memory
	free(SPGR);
	for (int p = 0; p < nPhases; p++)
		free(SSFP[p]);
	free(SSFP);
	free(M0Data);
	free(B0Data);
	free(B1Data);
	free(maskData);
	exit(EXIT_SUCCESS);
}

