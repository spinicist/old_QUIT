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
#include <dispatch/dispatch.h>
#ifdef __APPLE__
	#include <libkern/OSAtomic.h>
	#define AtomicAdd OSAtomicAdd32
#else
	#define AtomicAdd(x, y) (*y)++
#endif
#include "DESPOT.h"
#include "nifti_tools.h"
#include "znzlib.h"

char *usage = "Usage is: despot2 [options] output_prefix T1_map ssfp_file1 [ssfp_fileN] \n\
\
Options:\n\
	-m file  : Mask input with specified file.\n\
	-B1 file : B1 Map File.\n\
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
	size_t nPhases, *nSSFP;
	double ssfpTR, *ssfpPhases, **ssfpAngles;
	nifti_image *maskFile = NULL, *B1File = NULL, *T1File = NULL, **ssfpFiles;
	
	int thisArg = 1;
	while ((thisArg < argc) && (argv[thisArg][0] =='-'))
	{
		if (strcmp(argv[thisArg], "-m") == 0) {
			fprintf(stdout, "Reading Mask from: %s\n", argv[++thisArg]);
			maskFile = nifti_image_read(argv[thisArg], FALSE);
		} else if (strcmp(argv[thisArg], "-B1") == 0) {
			fprintf(stdout, "Reading B1 Map from: %s\n", argv[++thisArg]);
			B1File = nifti_image_read(argv[thisArg], FALSE);
		} else if (strcmp(argv[thisArg], "-z") == 0) {
			outExt = ".nii.gz";
		} else {
			fprintf(stderr, "Undefined command line option\n%s", usage);
			exit(EXIT_FAILURE);
		}
		++thisArg;
	}
	fprintf(stdout, "Output prefix will be: %s\n", argv[thisArg]);
	outPrefix = argv[thisArg++];
	fprintf(stdout, "Reading T1 Map from: %s\n", argv[thisArg]);
	T1File = nifti_image_read(argv[thisArg++], FALSE);
	//**************************************************************************
	// Gather SSFP Data
	//**************************************************************************
	nPhases    = argc - thisArg;
	nSSFP      = malloc(nPhases * sizeof(int));
	ssfpPhases = malloc(nPhases * sizeof(double));
	ssfpAngles = malloc(nPhases * sizeof(double *));
	ssfpFiles  = malloc(nPhases * sizeof(nifti_image *));
	fprintf(stdout, "Specified %zu phase cycling patterns.\n", nPhases);
	fprintf(stdout, "Enter SSFP TR (ms):");
	fscanf(stdin, "%lf", &ssfpTR);
	fprintf(stdout, "Enter %zu Phase-Cycling Patterns (degrees):", nPhases);
	fgetArray(stdin, 'd', nPhases, ssfpPhases); fprintf(stdout, "\n");
	arrayApply(ssfpPhases, ssfpPhases, radians, nPhases);
	for (int p = 0; p < nPhases; p++)
	{
		fprintf(stdout, "Reading SSFP header from %s.\n", argv[thisArg]);
		ssfpFiles[p] = nifti_image_read(argv[thisArg], FALSE); thisArg++;
		if ((p > 0) && (ssfpFiles[p - 1]->nx * ssfpFiles[p - 1]->ny * ssfpFiles[p - 1]->nz) != 
		               (ssfpFiles[0]->nx * ssfpFiles[0]->ny * ssfpFiles[0]->nz))
		{
			fprintf(stderr, "Differing number of voxels in phase %d and %d headers.\n", p - 1, p);
			exit(EXIT_FAILURE);
		}
		nSSFP[p] = ssfpFiles[p]->nt;
		ssfpAngles[p] = malloc(nSSFP[p] * sizeof(double));
		fprintf(stdout, "Enter %zu Angles (degrees) for Pattern %d:", nSSFP[p], p);
		fgetArray(stdin, 'd', nSSFP[p], ssfpAngles[p]); fprintf(stdout, "\n");
		arrayApply(ssfpAngles[p], ssfpAngles[p], radians, nSSFP[p]);
	}
	
	//**************************************************************************	
	// Allocate memory for slices
	//**************************************************************************
	int voxelsPerSlice = ssfpFiles[0]->nx * ssfpFiles[0]->ny;	
	int totalVoxels = voxelsPerSlice * ssfpFiles[0]->nz;
	float *maskData, *B1Data, *T1Data, **ssfpData;
	ssfpData = malloc(nPhases * sizeof(float *));
	for (int p = 0; p < nPhases; p++)
		ssfpData[p] = malloc(nSSFP[p] * voxelsPerSlice * sizeof(float));
	if (maskFile)
		maskData = malloc(voxelsPerSlice * sizeof(float));
	if (B1File)
		B1Data = malloc(voxelsPerSlice * sizeof(float));
	T1Data = malloc(voxelsPerSlice * sizeof(float));	
	//**************************************************************************
	// Create results files
	// T2, B0, residue
	// Need to write a full file of zeros first otherwise per-plane writing
	// won't produce a complete image.
	//**************************************************************************
	#define NR 4
	nifti_image **resultsHeaders = malloc(NR * sizeof(nifti_image *));
	int outDims[8] = {3, ssfpFiles[0]->nx, ssfpFiles[0]->ny, ssfpFiles[0]->nz, 1, 1, 1, 1};
	float *blank = calloc(totalVoxels, sizeof(float));
	float **resultsSlices = malloc(NR * sizeof(float*));
	char *names[NR] = { "_M0", "_T2", "_B0", "_res" };
	char outName[strlen(outPrefix) + 12];
	for (int r = 0; r < NR; r++)
	{
		strcpy(outName, outPrefix); strcat(outName, names[r]); strcat(outName, outExt);
		fprintf(stdout, "Writing blank result file:%s.\n", outName);
		resultsHeaders[r] = nifti_copy_orientation(ssfpFiles[0], outDims, DT_FLOAT);
		nifti_set_filenames(resultsHeaders[r], outName, FALSE, TRUE);
		resultsHeaders[r]->data = blank;
		nifti_image_write(resultsHeaders[r]);
		resultsSlices[r] = malloc(voxelsPerSlice * sizeof(float));
	}
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
	time_t allStart = time(NULL);
	struct tm *localStart = localtime(&allStart);
	char theTime[1024];
	strftime(theTime, 1024, "%H:%M:%S", localStart);
	fprintf(stdout, "Started processing at %s.\n", theTime);
	for (size_t slice = 0; slice < ssfpFiles[0]->nz; slice++)
	{
		// Read in data
		fprintf(stdout, "Starting slice %zu...\n", slice);
		int sliceStart[7] = {0, 0, (int)slice, 0, 0, 0, 0};
		int sliceDim[7] = {ssfpFiles[0]->nx, ssfpFiles[0]->ny, 1, 1, 1, 1, 1};
		
		if (B1File)
			nifti_read_subregion_image(B1File, sliceStart, sliceDim, (void **)&B1Data);
		if (maskFile)
		{
			if (maskFile->datatype == DT_FLOAT)
				nifti_read_subregion_image(maskFile, sliceStart, sliceDim, (void **)&maskData);
			else
			{
				nifti_read_subregion_image(maskFile, sliceStart, sliceDim, &(maskFile->data));
				arrayConvert(maskData, maskFile->data, DT_FLOAT, maskFile->datatype, voxelsPerSlice);
			}
		}
		nifti_read_subregion_image(T1File, sliceStart, sliceDim, (void **)&T1Data);
		for (int p = 0; p < nPhases; p++)
		{
			sliceDim[3] = nSSFP[p];
			nifti_read_subregion_image(ssfpFiles[p], sliceStart, sliceDim, (void **)&(ssfpData[p]));
		}
		
		__block int voxCount = 0;
		clock_t loopStart = clock();
		for (size_t vox = 0; vox < voxelsPerSlice; vox++)
		//void (^processVoxel)(size_t vox) = ^(size_t vox)
		{
			double params[NR];
			arraySet(params, 0., NR);
			if (!maskFile || ((maskData[vox] > 0.) && (T1Data[vox] > 0.)))
			{	// Zero T1 causes zero-pivot error.
				AtomicAdd(1, &voxCount);
				
				double B1 = 1.;
				if (B1File)
					B1 = (double)B1Data[vox];
				double T1 = (double)T1Data[vox];
				// Constants need to be set up here because B1 changes per-voxels
				double *signals[nPhases];
				for (int p = 0; p < nPhases; p++)
				{
					signals[p] = malloc(nSSFP[p] * sizeof(double));
					for (int img = 0; img < nSSFP[p]; img++)
						signals[p][img] = (double)ssfpData[p][voxelsPerSlice * img + vox];
				}

				if (nPhases == 1)
					classicDESPOT2(ssfpAngles[0], signals[0], nSSFP[0], ssfpTR, T1, B1, params);
				else
				{
					// Find the phase with highest mean
					double means[nPhases];
					size_t indxs[nPhases];
					double *consts[nPhases];
					eval_array_type *fs[nPhases];					
					for (int p = 0; p < nPhases; p++)
					{
						means[p] = arrayMean(signals[p], nSSFP[p]);
						arrayScale(signals[p], signals[p], 1. / means[p], nSSFP[p]);
						
						consts[p] = arrayAlloc(4);
						consts[p][0] = ssfpTR;
						consts[p][1] = T1;
						consts[p][2] = B1;
						consts[p][3] = ssfpPhases[p];
						
						fs[p] = &a1cSSFP;
					}
					arrayIndexSort(means, indxs, SORT_DESCEND, nPhases);
					// Do classic DESPOT2 on the highest mean (shouldn't be a signal null)
					classicDESPOT2(ssfpAngles[indxs[0]], signals[indxs[0]], nSSFP[indxs[0]], ssfpTR, T1, B1, params);
					// Now do a local fit to all the data including off-resonance
					extern int MATH_DEBUG;
					MATH_DEBUG = 1;
					if ((params[0] > 0.) && (params[1] > 0.))
						levMar(&(params[1]), 2, consts, ssfpAngles, signals, fs, NULL, nSSFP, nPhases, true, &(params[3]));
					//contractDESPOT2(nPhases, nSSFP, ssfpPhases, ssfpAngles, signals,
					//			    ssfpTR, T1, B1, params + 1); // Skip M0
				}
				
				// Clean up memory
				for (int p = 0; p < nPhases; p++)
					free(signals[p]);
			}
			for (int p = 0; p < NR; p++)
				resultsSlices[p][vox]  = (float)params[p];
		};
		//dispatch_queue_t global_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
		//dispatch_apply(voxelsPerSlice, global_queue, processVoxel);
		
        clock_t loopEnd = clock();
        fprintf(stdout, "Finished slice %zu", slice);
		if (voxCount > 0)
		{
			fprintf(stdout, ", had %d unmasked voxels, CPU time per voxel was %f s. Writing to results files...", 
			        voxCount, (loopEnd - loopStart) / ((float)voxCount * CLOCKS_PER_SEC));
			for (int p = 0; p < NR; p++)
				nifti_write_subregion_image(resultsHeaders[p], sliceStart, sliceDim, (void **)&(resultsSlices[p]));
            fprintf(stdout, "done");
		}
		fprintf(stdout, ".\n");
	}
    time_t allEnd = time(NULL);
    struct tm *localEnd = localtime(&allEnd);
    strftime(theTime, 1024, "%H:%M:%S", localEnd);
	fprintf(stdout, "Finished processing at %s. Run-time was %f s.\n", theTime, difftime(allEnd, allStart));
	
	// Clean up memory
	for (int p = 0; p < nPhases; p++)
		free(ssfpData[p]);
	free(B1Data);
	free(maskData);
	exit(EXIT_SUCCESS);
}
