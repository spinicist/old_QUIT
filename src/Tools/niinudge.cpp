/*
 *  niinudge.cpp
 *
 *  Copyright (c) 2014 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <iostream>
#include <sstream>
#include <string>
#include <getopt.h>

#include <Eigen/Core>
#include "Nifti/Nifti.h"
#include "QUIT/MultiArray.h"

using namespace std;
using namespace Eigen;
using namespace QUIT;

const string usage = "niinudge - A utility for moving Nifti images in physical space.\n\
\n\
Usage: niinudge [options] file1 [other files]\n\
By default nothing happens. Specify one of the options to move your image.\n\
Many of the options require a 3 dimensional vector argument. Valid formats for\n\
this are:\n\
X Y Z - Make sure you encase this format in quotes (\" \")!\n\
\n\
Options:\n\
	--nudge, -n \"X Y Z\"  : Nudge the image (X Y Z added to current offset)\n\
	--origin, -o \"X Y Z\" : Set the offset to -(X,Y,Z) so that the origin\n\
	                         (0,0,0) will appear at this position\n\
	--offset, -f \"X Y Z\" : Set the offset to (X,Y,Z)\n\
	--cog, -c              : Make the Center of Gravity lie at the origin\n\
	--verbose, -v          : Print out what the program is doing\n\
	-h, --help:   Print this message and quit.\n\
";

static const struct option long_opts[] = {
	{"nudge",  required_argument, 0, 'n'},
	{"origin", required_argument, 0, 'o'},
	{"offset", required_argument, 0, 'f'},
	{"cog",    no_argument, 0, 'c'},
	{"verbose", no_argument, 0, 'v'},
	{"help",   no_argument, 0, 'h'},
	{0, 0, 0, 0}
};
static const char *short_opts = "n:o:f:cvh";

static int verbose = false;

Vector3f parse_vector(char *str);
Vector3f parse_vector(char *str) {
	istringstream is(str);
	Vector3f vec;
	is >> vec[0] >> vec[1] >> vec[2];
	return vec;
}

Vector3f calc_cog(Nifti::File &f);
Vector3f calc_cog(Nifti::File &f) {
	const auto dims = f.matrix();
	MultiArray<float, 3> a(dims);
	f.open(f.imagePath(), Nifti::Mode::Read);
	f.readVolumes(a.begin(), a.end(), 0, 1);

	Vector3f cog = Vector3f::Zero();
	float mass = 0.;
	for (size_t k = 0; k < dims[2]; k++) {
		for (size_t j = 0; j < dims[1]; j++) {
			for (size_t i = 0; i < dims[0]; i++) {
				float val = a[{i,j,k}];
				if (isfinite(val)) {
					cog += val * Vector3f(i,j,k);
					mass += val;
				}
			}
		}
	}
	cog /= mass;
	if (verbose) cout << "CoG in voxels: " << cog.transpose() << ", mass: " << mass << endl;
	cog = f.header().transform() * cog;
	if (verbose) cout << "CoG in space:  " << cog.transpose() << endl;
	f.close();
	return cog;
}

int main(int argc, char **argv) {
	// Make a first pass to permute the options and get filenames at the end
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, &indexptr)) != -1) {
		switch (c) {
		case 'v': verbose = true; break;
		case '?': // getopt will print an error message
		case 'h':
			cout << usage << endl;
			return EXIT_FAILURE;
		default:
			break;
		}
	}
	if ((argc - optind) <= 0 ) {
		cerr << "No input image file specified." << endl;
		cout << usage << endl;
		return EXIT_FAILURE;
	}
	vector<Nifti::File> files;
	vector<vector<char>> data;
	files.reserve(argc - optind);
	data.reserve(argc - optind);
	while (optind < argc) {
		files.emplace_back(Nifti::File(argv[optind++], Nifti::Mode::Read));
		Nifti::File &f = files.back();
		if (verbose) cout << "Opened file: " << f.imagePath() << endl;
		data.emplace_back(vector<char>(f.dataSize()));
		f.readBytes(data.back());
		f.close();
	}
	optind = 1;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, &indexptr)) != -1) {
		Vector3f nudge;
		switch (c) {
		case 'n':
			nudge = parse_vector(optarg);
			for (Nifti::File &f : files) {
				if (verbose) cout << "Nudging offset by: " << nudge.transpose() << " in file: " << f.imagePath() << endl;
				Nifti::Header h = f.header();
				Affine3f xfm = h.transform();
				xfm = Translation3f(nudge) * xfm;
				h.setTransform(xfm);
				f.setHeader(h);
			}
			break;
		case 'o':
			nudge = parse_vector(optarg);
			for (Nifti::File &f : files) {
				if (verbose) cout << "Aligning origin to: " << nudge.transpose() << " in file: " << f.imagePath() << endl;
				Nifti::Header h = f.header();
				Affine3f xfm = h.transform();
				xfm.translation() = -nudge;
				h.setTransform(xfm);
				f.setHeader(h);
			}
			break;
		case 'f':
			nudge = parse_vector(optarg);
			for (Nifti::File &f : files) {
				if (verbose) cout << "Setting offset to: " << nudge.transpose() << " in file: " << f.imagePath() << endl;
				Nifti::Header h = f.header();
				Affine3f xfm = h.transform();
				xfm.translation() = nudge;
				h.setTransform(xfm);
				f.setHeader(h);
			}
			break;
		case 'c':
			for (Nifti::File &f : files) {
				if (verbose) cout << "Aligning origin to CoG in file: " << f.imagePath() << endl;
				Nifti::Header h = f.header();
				Vector3f CoG = calc_cog(f);
				Affine3f xfm = h.transform();
				xfm = Translation3f(-CoG) * xfm;
				h.setTransform(xfm);
				f.setHeader(h);
			}
			break;
		case '?': // getopt will print an error message
		case 'h':
			break;
		default:
			break;
		}
	}
	auto f = files.begin();
	auto d = data.begin();
	for (; f != files.end(); f++, d++) {
		if (verbose) cout << "Writing file: " << f->imagePath() << endl;
		f->open(f->imagePath(), Nifti::Mode::Write);
		f->writeBytes(*d);
		f->close();
	}

	return EXIT_SUCCESS;
}

