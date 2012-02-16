/*
 *  mcdespot_main.c
 *  Fitting
 *
 *  Created by Tobias Wood on 14/02/2012.
 *  Copyright 2012 Tobias Wood. All rights reserved.
 *
 */

#include <string.h>
#include <dispatch/dispatch.h>
#include "DESPOT.h"
#include "nifti_tools.h"
#include "znzlib.h"

#define FALSE 0
#define TRUE 1

#define __DEBUG__ FALSE
#define __DEBUG_THRESH__ 60000

char *usage = "Usage is: mcdespot2 [Output Prefix] [SPGR input file] [SPGR TR] [Number of phase cycling patterns] <[SSFP input file] [Phase Cycling]>  [SSFP TR] [Simplex/Region Contraction] [T1 Map File] [B1 Map File] <[Mask File]>\n\
\n\
The input file requires 2 columns - SSFP 180 file, flip angle.\n\
Specify the phase cycling pattern in degrees after each input file.\n";

//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv)
{
	//**************************************************************************
	// Argument Processing
	//**************************************************************************
	if (argc == 1)
	{
		fprintf(stderr, "%s", usage);
		exit(EXIT_FAILURE);
	}
	
	if (argc < 13)
	{
		fprintf(stderr, "Incorrect number of arguments (= %d) specified, should be at least 12.\n",
				argc - 1); // Subtract off filename
		fprintf(stderr, "%s", usage);
		exit(EXIT_FAILURE);
	}

	char *outPrefix = argv[1];
	char **spgrFilenames;
	double *spgrAngles;
	size_t nSPGR = readRecordFile(argv[2], "sd", &spgrFilenames, &spgrAngles);
	double spgrTR = atof(argv[3]);
	for (int i = 0; i < nSPGR; i++)
		spgrAngles[i] = radians(spgrAngles[i]);
	fprintf(stdout, "Specified %ld SPGR files with TR = %f ms.\n", nSPGR, spgrTR);
	
	size_t nPhases = atoi(argv[4]);
	size_t *nSSFP = malloc(nPhases * sizeof(size_t));
	char **ssfpFilenames[nPhases];
	double ssfpTR, *ssfpPhases = malloc(nPhases * sizeof(double)),
	       **ssfpAngles = malloc(nPhases * sizeof(double *));

	fprintf(stdout, "Specified %ld phase cycling patterns.\n", nPhases);	
	for (size_t p = 0; p < nPhases; p++)
	{
		nSSFP[p] = readRecordFile(argv[5 + (p * 2)], "sd", &(ssfpFilenames[p]), &(ssfpAngles[p]));
		ssfpPhases[p] = radians(atof(argv[6 + (p * 2)]));
		for (int i = 0; i < nSSFP[p]; i++)
			ssfpAngles[p][i] = radians(ssfpAngles[p][i]);
		fprintf(stdout, "Specified pattern %ld with phase %f deg and %ld files.\n", p, degrees(ssfpPhases[p]), nSSFP[p]);
		//ARR_D( ssfpAngles[p], nSSFP[p] );
	}
	ssfpTR = atof(argv[5 + (nPhases * 2)]);
	int mode = atoi(argv[6 + (nPhases * 2)]);
	fprintf(stdout, "Specified TR of %f ms.\n", ssfpTR);
	nifti_image *T1Map = NULL, *B1Map = NULL, *mask = NULL;
	fprintf(stdout, "Reading T1 Map: %s\n", argv[7 + (nPhases * 2)]);
	T1Map = nifti_image_read(argv[7 + (nPhases * 2)], FALSE);
	fprintf(stdout, "Reading B1 Map: %s\n", argv[8 + (nPhases * 2)]);
	B1Map = nifti_image_read(argv[8 + (nPhases * 2)], FALSE);
	if (argc == (9 + (nPhases * 2)))
	{}
	else if (argc == (10 + (nPhases * 2)))
	{
		fprintf(stdout, "Reading Mask: %s\n", argv[9 + (nPhases * 2)]);
		mask = nifti_image_read(argv[9 + (nPhases * 2)], FALSE);
	}
	else
	{
		fprintf(stdout, "Argument count was %d, expected %ld or %ld\n", argc - 1, 9 + (nPhases * 2), 10 + (nPhases * 2));
		exit(EXIT_FAILURE);
	}
	//**************************************************************************	
	// Read in headers / Allocate memory for slices and results
	//**************************************************************************
	nifti_image **spgrFiles = malloc(nSPGR * sizeof(nifti_image *));
	loadHeaders(spgrFilenames, spgrFiles, nSPGR);
	nifti_image ***ssfpFiles = malloc(nPhases * sizeof(nifti_image **));
	for (int p = 0; p < nPhases; p++)
	{
		ssfpFiles[p] = (nifti_image **)malloc(nSSFP[p] * sizeof(nifti_image *));
		loadHeaders(ssfpFilenames[p], ssfpFiles[p], nSSFP[p]);
		if ((p > 0) && ssfpFiles[p - 1][0]->nvox != ssfpFiles[p][0]->nvox)
		{
			fprintf(stderr, "Differing number of voxels in phase %d and %d headers.\n", p - 1, p);
			exit(EXIT_FAILURE);
		}
	}
	if (ssfpFiles[0][0]->nvox != spgrFiles[0]->nvox)
	{
		fprintf(stderr, "Differing number of voxels in SPGR and SSFP data.\n");
		exit(EXIT_FAILURE);
	}
	
	int voxelsPerSlice = spgrFiles[0]->nx * spgrFiles[0]->ny;	
	int totalVoxels = voxelsPerSlice * spgrFiles[0]->nz;
	/* T1_s, T1_f, T2_s, T2_f,	f_s, tau_s, dw */
	__block float *T1_sData = malloc(totalVoxels * sizeof(float));
	__block float *T1_fData = malloc(totalVoxels * sizeof(float));
	__block float *T2_sData = malloc(totalVoxels * sizeof(float));
	__block float *T2_fData = malloc(totalVoxels * sizeof(float));
	__block float *f_sData  = malloc(totalVoxels * sizeof(float));
	__block float *tau_sData = malloc(totalVoxels * sizeof(float));
	__block float *dwData    = malloc(totalVoxels * sizeof(float));
	//**************************************************************************
	// Do the fitting
	//**************************************************************************
	fprintf(stdout, "Fitting mcDESPOT.\n");
	
	for (int slice = 0; slice < ssfpFiles[0][0]->nz; slice++)
	//void (^processSlice)(size_t slice) = ^(size_t slice)
	{
		// Read in data
		fprintf(stdout, "Starting slice %ld...\n", slice);
		double params[7];
		
		int sliceStart[7] = {0, 0, slice, 0, 0, 0, 0};
		int sliceDim[7] = {spgrFiles[0]->nx, spgrFiles[0]->ny, 1, 1, 1, 1, 1};
		float **spgrData = malloc(nSPGR * sizeof(float *));
		double *spgrSignal = malloc(nSPGR * sizeof(double));
		for (int i = 0; i < nSPGR; i++)
		{
			spgrData[i] = malloc(voxelsPerSlice * sizeof(float));
			nifti_read_subregion_image(spgrFiles[i], sliceStart, sliceDim, (void**)&(spgrData[i]));
		}
		
		float ***ssfpData = malloc(nPhases * sizeof(float **));
		double **ssfpSignal = malloc(nPhases * sizeof(double *));
		for (int p = 0; p < nPhases; p++)
		{
			ssfpSignal[p] = malloc(nSSFP[p] * sizeof(double));
			ssfpData[p] = malloc(nSSFP[p] * sizeof(float *));
			for (int i = 0; i < nSSFP[p]; i++)
			{
				ssfpData[p][i] = malloc(voxelsPerSlice * sizeof(float));
				nifti_read_subregion_image(ssfpFiles[p][i], sliceStart, sliceDim, (void**)&(ssfpData[p][i]));
			}
		}
		
		float *T1Data, *B1Data, *maskData;
		T1Data = malloc(voxelsPerSlice * sizeof(float));
		B1Data = malloc(voxelsPerSlice * sizeof(float));
		if (mask)
			maskData = malloc(voxelsPerSlice * sizeof(float));
		nifti_read_subregion_image(T1Map, sliceStart, sliceDim, (void**)&(T1Data));
		nifti_read_subregion_image(B1Map, sliceStart, sliceDim, (void**)&(B1Data));
		if (mask)
			nifti_read_subregion_image(mask, sliceStart, sliceDim, (void**)&(maskData));
		
		int sliceIndex = slice * voxelsPerSlice;
		for (int vox = 0; vox < voxelsPerSlice; vox++)
		{
			if (!mask || (maskData[vox] > 0.))
			{
				arrayZero(params, 7);
				for (int img = 0; img < nSPGR; img++)
					spgrSignal[img] = (double)spgrData[img][vox];

				for (int p = 0; p < nPhases; p++)
				{
					for (int img = 0; img < nSSFP[p]; img++)
						ssfpSignal[p][img] = (double)ssfpData[p][img][vox];
				}
				
				double T1 = (double)T1Data[vox];
				double B1 = (double)B1Data[vox];
				if (mode == 0)
				{	// Use phase cycle with highest intensity to bootstrap simplex
					/*double *pSigs = malloc(nPhases * sizeof(double));
					for (int p = 0; p < nPhases; p++)
						pSigs[p] = ssfpSignal[p][0];
					size_t *ind = malloc(nPhases * sizeof(double));
					arraySort(pSigs, nPhases, ind);
					size_t hi = ind[nPhases - 1];
					classicDESPOT2(ssfpAngles[hi], ssfpSignal[hi], nSSFP[hi], ssfpTR, T1, B1, &M0, &T2);
					simplexDESPOT2(nPhases, nSSFP, ssfpPhases, ssfpAngles, ssfpSignal, ssfpTR, T1, B1, &M0, &T2, &dO);
					
					free(ind);
					free(pSigs);*/
				}
				else
				{	// Use region contraction
					mcDESPOT(nSPGR, spgrAngles, spgrSignal, spgrTR,
					         nPhases, nSSFP, ssfpPhases, ssfpAngles, ssfpSignal, ssfpTR,
							 T1, B1, params);
				}
				T1_sData[sliceIndex + vox]  = (float)params[0];
				T1_fData[sliceIndex + vox]  = (float)params[1];
				T2_sData[sliceIndex + vox]  = (float)params[2];
				T2_fData[sliceIndex + vox]  = (float)params[3];
				f_sData[sliceIndex + vox]   = (float)params[4];
				tau_sData[sliceIndex + vox] = (float)params[5];
				dwData[sliceIndex + vox]    = (float)params[6];
			}
		}
		
		// Clean up memory
		for (int img = 0; img < nSPGR; img++)
			free(spgrData[img]);
		free(spgrSignal);
		for (int p = 0; p < nPhases; p++)
		{
			for (int img = 0; img < nSSFP[p]; img++)
				free(ssfpData[p][img]);
			free(ssfpData[p]);
			free(ssfpSignal[p]);
		}
		free(T1Data);
		free(B1Data);
		fprintf(stdout, "Finished slice %ld.\n", slice);		
	};
	//dispatch_queue_t global_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	//dispatch_apply(ssfpFiles[0][0]->nz, global_queue, processSlice);
	fprintf(stdout, "Finished fitting. Writing results files.\n");

	//**************************************************************************
	// Create results files
	//**************************************************************************
	nifti_image *out = nifti_copy_nim_info(ssfpFiles[0][0]);
	char outName[strlen(outPrefix) + 4]; // Space for "T1" plus null
	strcpy(outName, outPrefix); strcat(outName, "_T1_myel");
	writeResult(out, outName, (void*)T1_fData);
	strcpy(outName, outPrefix); strcat(outName, "_T1_free");
	writeResult(out, outName, (void*)T1_sData);
	strcpy(outName, outPrefix); strcat(outName, "_T2_myel");
	writeResult(out, outName, (void*)T2_fData);
	strcpy(outName, outPrefix); strcat(outName, "_T2_free");
	writeResult(out, outName, (void*)T2_sData);
	strcpy(outName, outPrefix); strcat(outName, "_frac_my");
	writeResult(out, outName, (void*)f_sData);
	strcpy(outName, outPrefix); strcat(outName, "_tau_my");
	writeResult(out, outName, (void*)tau_sData);
	strcpy(outName, outPrefix); strcat(outName, "_dw");
	writeResult(out, outName, (void*)dwData);
	fprintf(stdout, "All done.\n");
	return EXIT_SUCCESS;
}

