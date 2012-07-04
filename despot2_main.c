/*
 *  despot1_main.c
 *  MacRI
 *
 *  Created by Tobias Wood on 23/01/2012.
 *  Copyright 2012 Tobias Wood. All rights reserved.
 *
 */

#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <getopt.h>
#include <dispatch/dispatch.h>

#ifdef __APPLE__
	#include <libkern/OSAtomic.h>
	#define AtomicAdd OSAtomicAdd32
#else
	#define AtomicAdd(x, y) (*y) += x
#endif

#include "DESPOT.h"
#include "FSLIO.h"
#include "mathsArray.h"

char *usage = "Usage is: despot2 [options] output_prefix T1_map ssfp_180_file [additional ssfp files] \n\
\
Options:\n\
	-m file  : Mask input with specified file.\n\
	--B0 file : B0 Map File.\n\
	--B1 file : B1 Map File.\n\
	--M0 file : M0 Map File.\n\
	-z       : Output .nii.gz files.\n";

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

	char *outPrefix = NULL, *outExt = ".nii";
	size_t nPhases;
	double ssfpTR, *ssfpPhases = NULL, *ssfpAngles = NULL;
	FSLIO *maskFile = NULL, *B0File = NULL, *B1File = NULL, *M0File = NULL, *T1File = NULL, **ssfpFiles = NULL;
	
	static struct option long_options[] =
	{
		{"B0", required_argument, 0, '0'},
		{"B1", required_argument, 0, '1'},
		{"M0", required_argument, 0, 'M'},
		{0, 0, 0, 0}
	};
	
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, "m:z", long_options, &indexptr)) != -1)
	{
		switch (c)
		{
			case 'm':
				maskFile = FslOpen(optarg, "rb");
				break;
			case '0':
				B0File = FslOpen(optarg, "rb");
				break;
			case '1':
				B1File = FslOpen(optarg, "rb");
				break;
			case 'M':
				M0File = FslOpen(optarg, "rb");
				break;
			case 'z':
				outExt = ".nii.gz";
				break;
		}
	}
	
	fprintf(stdout, "Output prefix will be: %s\n", argv[optind]);
	outPrefix = argv[optind++];
	fprintf(stdout, "Reading T1 Map from: %s\n", argv[optind]);
	T1File = FslOpen(argv[optind++], "rb");
	//**************************************************************************
	// Gather SSFP Data
	//**************************************************************************
	nPhases    = argc - optind;
	if (nPhases < 1)
	{
		fprintf(stderr, "Must have at least the 180 degree phase-cycling pattern to process.\n");
		exit(EXIT_FAILURE);
	}
	ssfpFiles  = malloc(nPhases * sizeof(nifti_image *));
	ssfpPhases = malloc(nPhases * sizeof(double));
	ssfpPhases[0] = M_PI;
	if (nPhases > 1)
		fprintf(stdout, "Enter %zu Phase-Cycling Patterns (degrees):", nPhases - 1);
	fgetArray(stdin, 'd', nPhases - 1, ssfpPhases + 1); fprintf(stdout, "\n");
	arrayApply(ssfpPhases, ssfpPhases, radians, nPhases);
	fprintf(stdout, "Enter SSFP TR (ms):");
	fscanf(stdin, "%lf", &ssfpTR);

	ssfpFiles[0] = FslOpen(argv[optind++], "rb");
	size_t nx = ssfpFiles[0]->niftiptr->nx;
	size_t ny = ssfpFiles[0]->niftiptr->ny;
	size_t nz = ssfpFiles[0]->niftiptr->nz;
	size_t nSSFP = ssfpFiles[0]->niftiptr->nt;	

	ssfpAngles = malloc(nSSFP * sizeof(double));
	fprintf(stdout, "Enter %zu SSFP flip angles (degrees) :", nSSFP);
	fgetArray(stdin, 'd', nSSFP, ssfpAngles); fprintf(stdout, "\n");
	arrayApply(ssfpAngles, ssfpAngles, radians, nSSFP);	

	for (size_t p = 1; p < nPhases; p++)
	{
		fprintf(stdout, "Reading %f SSFP header from %s.\n", degrees(ssfpPhases[p]), argv[optind]);
		ssfpFiles[p] = FslOpen(argv[optind++], "rb");
		if (!FslCheckDims(ssfpFiles[0], ssfpFiles[p]))
		{
			fprintf(stderr, "Image %s has differing dimensions.\n", argv[optind - 1]);
			exit(EXIT_FAILURE);
		}
		if (ssfpFiles[p]->niftiptr->nt != nSSFP)
		{
			fprintf(stderr, "Image %s has wrong number of flip angles.\n", argv[optind - 1]);
			exit(EXIT_FAILURE);
		}
	}
	//**************************************************************************	
	// Allocate memory for slices
	//**************************************************************************
	int voxelsPerSlice = nx * ny;
	int totalVoxels = voxelsPerSlice * nz;
	float *maskData = NULL, *B0Data = NULL, *B1Data = NULL, *M0Data = NULL, *T1Data = NULL, **ssfpData = NULL;
	ssfpData = malloc(nPhases * sizeof(float *));
	for (int p = 0; p < nPhases; p++)
		ssfpData[p] = malloc(nSSFP * voxelsPerSlice * sizeof(float));
	if (maskFile)
		maskData = malloc(voxelsPerSlice * sizeof(float));
	if (B0File)
		B0Data = malloc(voxelsPerSlice * sizeof(float));
	if (B1File)
		B1Data = malloc(voxelsPerSlice * sizeof(float));
	if (M0File)
		M0Data = malloc(voxelsPerSlice * sizeof(float));
	T1Data = malloc(voxelsPerSlice * sizeof(float));	
	//**************************************************************************
	// Create results files
	// T2, residue
	// Need to write a full file of zeros first otherwise per-plane writing
	// won't produce a complete image.
	//**************************************************************************
	#define NR 3
	FSLIO **resultsHeaders = malloc(NR * sizeof(FSLIO *));
	float **resultsData = malloc(NR * sizeof(float*));
	char *names[NR] = { "_d2_M0", "_T2", "_d2_res" };
	char outName[strlen(outPrefix) + 15];
	for (int r = 0; r < NR; r++)
	{
		strcpy(outName, outPrefix); strcat(outName, names[r]); strcat(outName, outExt);
		fprintf(stdout, "Writing result header:%s.\n", outName);
		resultsHeaders[r] = FslOpen(outName, "wb");
		FslCloneHeader(resultsHeaders[r], ssfpFiles[0]);
		FslSetDim(resultsHeaders[r], nx, ny, nz, 1);
		FslSetDataType(resultsHeaders[r], DTYPE_FLOAT);
		resultsData[r] = malloc(totalVoxels * sizeof(float));
	}
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
	time_t allStart = time(NULL);
	struct tm *localStart = localtime(&allStart);
	char theTime[1024];
	strftime(theTime, 1024, "%H:%M:%S", localStart);
	fprintf(stdout, "Started processing at %s.\n", theTime);
	for (size_t slice = 0; slice < nz; slice++)
	{
		// Read in data
		fprintf(stdout, "Starting slice %zu...\n", slice);
		
		FslReadSliceSeries(T1File, (void *)T1Data, slice, 1);
		if (B0File)
			FslReadSliceSeries(B0File, (void *)B0Data, slice, 1);
		if (B1File)
			FslReadSliceSeries(B1File, (void *)B1Data, slice, 1);
		if (M0File)
			FslReadSliceSeries(M0File, (void *)M0Data, slice, 1);
		if (maskFile)
			FslReadSliceSeries(maskFile, (void *)maskData, slice, 1);
		for (int p = 0; p < nPhases; p++)
			FslReadSliceSeries(ssfpFiles[p], (void *)ssfpData[p], slice, nSSFP);
		
		int sliceOffset = slice * voxelsPerSlice;
		__block int voxCount = 0;
		clock_t loopStart = clock();
		//for (size_t vox = 0; vox < voxelsPerSlice; vox++)
		void (^processVoxel)(size_t vox) = ^(size_t vox)
		{
			double params[NR];
			arraySet(params, 0., NR);
			if (!maskFile || ((maskData[vox] > 0.) && (T1Data[vox] > 0.)))
			{	// Zero T1 causes zero-pivot error.
				AtomicAdd(1, &voxCount);
				
				// Gather signals. 180 Phase data is required to be the first file passed into program
				double *signals[nPhases];
				for (int p = 0; p < nPhases; p++)
				{
					signals[p] = malloc(nSSFP * sizeof(double));
					for (int img = 0; img < nSSFP; img++)
						signals[p][img] = (double)ssfpData[p][voxelsPerSlice * img + vox];
				}
				// Some constants set up here because they change per-voxel
				double T1 = (double)T1Data[vox];
				double B0 = 0, B1 = 1., M0 = 1;
				if (B0File)
					B0 = (double)B0Data[vox];
				if (B1File)
					B1 = (double)B1Data[vox];
				if (M0File)
					M0 = (double)M0Data[vox];				
				// Run classic DESPOT2 on 180 phase data
				params[2] = classicDESPOT2(ssfpAngles, signals[0], nSSFP, ssfpTR, T1, B1, params);
				params[0] = clamp(params[0], 0, 1.e7);
				params[1] = clamp(params[1], 0.001, 0.250);
				if (nPhases > 1)
				{
					double *xData[nPhases], *consts[nPhases];
					size_t dSize[nPhases];
					eval_array_type *fs[nPhases];
					
					for (int p = 0; p < nPhases; p++)
					{
						xData[p] = ssfpAngles;
						dSize[p] = nSSFP;
						consts[p] = arrayAlloc(5);
						consts[p][0] = ssfpTR;
						consts[p][1] = M0;
						consts[p][2] = T1;
						consts[p][3] = B0;
						consts[p][4] = B1;
						consts[p][5] = ssfpPhases[p];
						
						fs[p] = &a1cSSFP;
					}
					
					double loBounds[1] = { 0.010 };
					double hiBounds[1] = { 0.250 };
					//double *bounds[2] = { loBounds, hiBounds };
					//bool loC[1] = { TRUE }, hiC[1] = { TRUE };
					//bool *constrained[2] = { loC, hiC };
					levMar(params + 1, 1, consts, xData, signals, fs, NULL, dSize, nPhases, loBounds, hiBounds, false, params + 2);
					//regionContraction(params + 1, 1, consts, nPhases, xData, signals, dSize, TRUE, fs, bounds, constrained, 2000, 10, 10, 0.05, 0.05, &(params[2]));												
				}
				// Clean up memory
				for (int p = 0; p < nPhases; p++)
					free(signals[p]);
			}
			for (int p = 0; p < NR; p++)
				resultsData[p][sliceOffset + vox]  = (float)params[p];
		};
		dispatch_queue_t global_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
		dispatch_apply(voxelsPerSlice, global_queue, processVoxel);
		
        clock_t loopEnd = clock();
        fprintf(stdout, "Finished slice %zu", slice);
		if (voxCount > 0)
		{
			fprintf(stdout, ", had %d unmasked voxels, CPU time per voxel was %f s.", 
			        voxCount, (loopEnd - loopStart) / ((double)voxCount * CLOCKS_PER_SEC));
		}
		fprintf(stdout, ".\n");
	}
    time_t allEnd = time(NULL);
    struct tm *localEnd = localtime(&allEnd);
    strftime(theTime, 1024, "%H:%M:%S", localEnd);
	fprintf(stdout, "Finished processing at %s. Run-time was %f s.\n", theTime, difftime(allEnd, allStart));
	
	for (size_t r = 0; r < NR; r++)
	{
		FslWriteHeader(resultsHeaders[r]);
		FslWriteVolumes(resultsHeaders[r], resultsData[r], 1);
		FslClose(resultsHeaders[r]);
	}
	// Clean up memory
	for (int p = 0; p < nPhases; p++)
		free(ssfpData[p]);
	if (B0Data)
		free(B0Data);
	if (B1Data)
		free(B1Data);
	if (maskData)
		free(maskData);
	exit(EXIT_SUCCESS);
}
