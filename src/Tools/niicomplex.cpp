/*
 *  niicomplex.cpp
 *
 *  Created by Tobias Wood on 22/04/2014.
 *  Copyright (c) 2014 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <getopt.h>
#include <iostream>
#include <functional>

#include "Nifti/Nifti.h"
#include "QUIT/MultiArray.h"
using namespace std;
using namespace Nifti;
using namespace QUIT;

//******************************************************************************
// Arguments / Usage
//******************************************************************************
const string usage {
"Usage is: niicomplex [options] inputs outputs\n\
\n\
Default mode is to convert a magnitude/phase image pair into a real/imaginary \n\
image pair. If you have/want different inputs/outputs, then specify the -i/-o \n\
options. Multiple output types can be chosen. The correct number of names must\n\
be given as additional arguments.\n\
\n\
Options:\n\
	--help, -h           : Print this message\n\
	--verbose, -v        : Print more information\n\
	--input, -i m        : Input is magnitude/phase (default)\n\
	            r        : Input is real/imaginary\n\
	            c        : Input is complex\n\
	--output, -o [mpric] : Where any of [mpric] are present\n\
	             m       : Output a magnitude image\n\
	             p       : Output a phase image\n\
	             r       : Output a real image\n\
	             i       : Output an imaginary image\n\
	             c       : Output a complex image\n\
	--dtype, -d f     : Force output datatype to float\n\
	            d     : Force output datatype to double\n\
	            l     : Force output datatype to long double\n\
	--fixge, -f       : Fix alternate slice, opposing phase issue on GE.\n"
};

enum class Type { MagPhase, RealImag, Complex };
static bool verbose = false, forceDType = false, fixge = false;
static Type inputType = Type::MagPhase;
static string outputImages{"ri"};
static DataType precision = DataType::FLOAT32;
const struct option long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"verbose", no_argument, 0, 'v'},
	{"input", required_argument, 0, 'i'},
	{"output", required_argument, 0, 'o'},
	{"dtype", required_argument, 0, 'd'},
	{"fixge", no_argument, 0, 'f'},
	{0, 0, 0, 0}
};
const char* short_options = "hvi:o:d:f";
template<typename T>
void mag_to_cmp(Nifti::File &in1, Nifti::File &in2, size_t vol,
                MultiArray<T, 3> &v1, MultiArray<T, 3> &v2,
                MultiArray<complex<T>, 3> &c)
{
	if (verbose) cout << "Reading magnitude volume " << vol << endl;
	in1.readVolumes(v1.begin(), v1.end(), vol, 1);
	if (verbose) cout << "Reading phase volume " << vol << endl;
	in2.readVolumes(v2.begin(), v2.end(), vol, 1);
	typename MultiArray<T, 3>::Index d = v1.dims();
	for (size_t k = 0; k < d[2]; k++) {
		for (size_t j = 0; j < d[1]; j++) {
			for (size_t i = 0; i < d[0]; i++) {
				if (fixge && ((k % 2) == 1))
					c[{i,j,k}] = -polar(v1[{i,j,k}], v2[{i,j,k}]);
				else
					c[{i,j,k}] = polar(v1[{i,j,k}], v2[{i,j,k}]);
			}
		}
	}
}

template<typename T>
void re_im_to_cmp(Nifti::File &in1, Nifti::File &in2, size_t vol,
                  MultiArray<T, 3> &v1, MultiArray<T, 3> &v2,
                  MultiArray<complex<T>, 3> &c)
{
	if (verbose) cout << "Reading real volume " << vol << endl;
	in1.readVolumes(v1.begin(), v1.end(), vol, 1);
	if (verbose) cout << "Reading imaginary volume " << vol << endl;
	in2.readVolumes(v2.begin(), v2.end(), vol, 1);
	typename MultiArray<T, 3>::Index d = v1.dims();
	for (size_t k = 0; k < d[2]; k++) {
		for (size_t j = 0; j < d[1]; j++) {
			for (size_t i = 0; i < d[0]; i++) {
				if (fixge && ((k % 2) == 1))
					c[{i,j,k}] = -complex<T>(v1[{i,j,k}], v2[{i,j,k}]);
				else
					c[{i,j,k}] = complex<T>(v1[{i,j,k}], v2[{i,j,k}]);
			}
		}
	}
}

template<typename T>
void write_mag_vol(Nifti::File &out, size_t vol,
                   MultiArray<complex<T>, 3> &c, MultiArray<T, 3> &s)
{
	if (verbose) cout << "Writing magnitude volume..." << endl;
	for (size_t i = 0; i < s.size(); i++) { s[i] = abs(c[i]); }
	out.writeVolumes(s.begin(), s.end(), vol, 1);
}

template<typename T>
void write_ph_vol(Nifti::File &out, size_t vol,
                  MultiArray<complex<T>, 3> &c, MultiArray<T, 3> &s)
{
	if (verbose) cout << "Writing phase volume..." << endl;
	for (size_t i = 0; i < s.size(); i++) { s[i] = arg(c[i]); }
	out.writeVolumes(s.begin(), s.end(), vol, 1);
}

template<typename T>
void write_real_vol(Nifti::File &out, size_t vol,
                    MultiArray<complex<T>, 3> &c, MultiArray<T, 3> &s)
{
	if (verbose) cout << "Writing real volume..." << endl;
	for (size_t i = 0; i < s.size(); i++) { s[i] = real(c[i]); }
	out.writeVolumes(s.begin(), s.end(), vol, 1);
}

template<typename T>
void write_imag_vol(Nifti::File &out, size_t vol,
                    MultiArray<complex<T>, 3> &c, MultiArray<T, 3> &s)
{
	if (verbose) cout << "Writing imaginary volume..." << endl;
	for (size_t i = 0; i < s.size(); i++) { s[i] = imag(c[i]); }
	out.writeVolumes(s.begin(), s.end(), vol, 1);
}

//******************************************************************************
// Main
//******************************************************************************
int main(int argc, char **argv)
{
	//**************************************************************************
	// Argument Processing
	//**************************************************************************
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, short_options, long_options, &indexptr)) != -1) {
		switch (c) {
		case 'v': verbose = true; break;
		case 'i':
			switch (*optarg) {
				case 'm': inputType = Type::MagPhase;  cout << "Input is magnitude and phase." << endl; break;
				case 'r': inputType = Type::RealImag; cout << "Input is real and imaginary." << endl; break;
				case 'c': inputType = Type::Complex; cout << "Input is complex." << endl; break;
				default:
					cerr << "Unknown input type " << optarg << endl;
					return EXIT_FAILURE;
			} break;
		case 'o': outputImages = string(optarg); break;
		case 'd':
			switch (*optarg) {
				case 'f': precision = DataType::FLOAT32; break;
				case 'd': precision = DataType::FLOAT64; break;
				case 'l': precision = DataType::FLOAT128; break;
				default:
					cerr << "Unknown precision type " << optarg << endl;
					return EXIT_FAILURE;
			} break;
		case 'f': fixge = true; break;
		case 'h':
		case '?': // getopt will print an error message
			cout << usage << endl;
			return EXIT_SUCCESS;
		}
	}

	size_t expected_number_of_arguments = 0;
	switch (inputType) {
		case Type::MagPhase: expected_number_of_arguments += 2; break;
		case Type::RealImag: expected_number_of_arguments += 2; break;
		case Type::Complex:  expected_number_of_arguments += 1; break;
	}
	expected_number_of_arguments += outputImages.size();
	if (expected_number_of_arguments != (argc - optind)) {
		cout << "Expected " << expected_number_of_arguments<<  " filenames, but " << (argc - optind) << " were given." << endl << usage << endl;
		return EXIT_FAILURE;
	}

	File in1, in2;
	if (verbose) cout << "Opening input file: " << argv[optind] << endl;
	in1.open(argv[optind++], Mode::Read);
	if (inputType != Type::Complex) {
		if (verbose) cout << "Opening input file: " << argv[optind] << endl;
		in2.open(argv[optind++], Mode::Read);
		if (!in2.header().matchesSpace(in1.header())) {
			cerr << "Input files are incompatible." << endl;
			return EXIT_FAILURE;
		}
	}

	vector<File> outFiles;
	Header outHdr = in1.header();
	for (size_t oi = 0; oi < outputImages.size(); oi++) {
		char type = outputImages.at(oi);
		switch (type) {
		case 'm': case 'p': case 'r': case 'i':
			outHdr.setDatatype(precision);
			break;
		case 'c':
			switch (precision) {
				case DataType::FLOAT32: outHdr.setDatatype(DataType::COMPLEX64); break;
				case DataType::FLOAT64: outHdr.setDatatype(DataType::COMPLEX128); break;
				case DataType::FLOAT128: outHdr.setDatatype(DataType::COMPLEX256); break;
				default: throw(std::logic_error("Invalid precision type."));
			}
			break;
		default:
			// Need the string(1, type) constructor to build a string from a char
			throw(std::runtime_error("Invalid output image type: " + string(1, type)));
			break;
		}
		if (verbose) cout << "Opening output file: " << argv[optind] << endl;
		outFiles.emplace_back(File(outHdr, argv[optind++]));
	}

	// Allocate different types to save memory
	MultiArray<complex<float>, 3> cmp_flt;
	MultiArray<complex<double>, 3> cmp_dbl;
	MultiArray<complex<long double>, 3> cmp_ldbl;
	MultiArray<float, 3> flt1, flt2;
	MultiArray<double, 3> dbl1, dbl2;
	MultiArray<long double, 3> ldbl1, ldbl2;

	const auto d = in1.matrix();
	switch (precision) {
		case DataType::FLOAT32:  cmp_flt.resize(d);  flt1.resize(d);  flt2.resize(d); break;
		case DataType::FLOAT64:  cmp_dbl.resize(d);  dbl1.resize(d);  dbl2.resize(d); break;
		case DataType::FLOAT128: cmp_ldbl.resize(d); ldbl1.resize(d); ldbl2.resize(d); break;
		default:
			break; // We have checked that this can't happen earlier (famous last words)
	}

	for (size_t vol = 0; vol < in1.dim(4); vol++) {
		if (verbose) cout << "Converting volume " << vol << endl;
		switch (inputType) {
			case Type::MagPhase: {
				switch (precision) {
					case DataType::FLOAT32:  mag_to_cmp<float>(in1, in2, vol, flt1, flt2, cmp_flt); break;
					case DataType::FLOAT64:  mag_to_cmp<double>(in1, in2, vol, dbl1, dbl2, cmp_dbl); break;
					case DataType::FLOAT128: mag_to_cmp<long double>(in1, in2, vol, ldbl1, ldbl2, cmp_ldbl); break;
					default: break;
				}
			} break;
			case Type::RealImag: {
				switch (precision) {
					case DataType::FLOAT32:  re_im_to_cmp<float>(in1, in2, vol, flt1, flt2, cmp_flt); break;
					case DataType::FLOAT64:  re_im_to_cmp<double>(in1, in2, vol, dbl1, dbl2, cmp_dbl); break;
					case DataType::FLOAT128: re_im_to_cmp<long double>(in1, in2, vol, ldbl1, ldbl2, cmp_ldbl); break;
					default: break;
				}
			} break;
			case Type::Complex : {
				if (verbose) cout << "Reading complex volume " << vol << endl;
				switch (precision) {
					case DataType::FLOAT32: in1.readVolumes(cmp_flt.begin(), cmp_flt.end(), vol, 1); break;
					case DataType::FLOAT64: in1.readVolumes(cmp_dbl.begin(), cmp_dbl.end(), vol, 1); break;
					case DataType::FLOAT128: in1.readVolumes(cmp_ldbl.begin(), cmp_ldbl.end(), vol, 1); break;
					default: break;
				}
			} break;
		}

		for (size_t oi = 0; oi < outFiles.size(); oi++) {
			#define DECL_SWITCH( FUNC )\
			switch (precision) {\
			case DataType::FLOAT32: FUNC<float>(outFiles.at(oi), vol, cmp_flt, flt1); break;\
			case DataType::FLOAT64: FUNC<double>(outFiles.at(oi), vol, cmp_dbl, dbl1); break;\
			case DataType::FLOAT128: FUNC<long double>(outFiles.at(oi), vol, cmp_ldbl, ldbl1); break;\
			default: break;\
			}

			switch (outputImages.at(oi)) {
			case 'm': DECL_SWITCH( write_mag_vol ) break;
			case 'p': DECL_SWITCH( write_ph_vol ) break;
			case 'r': DECL_SWITCH( write_real_vol ) break;
			case 'i': DECL_SWITCH( write_imag_vol ) break;
			case 'c':
				if (verbose) cout << "Writing complex volume..." << endl;
				switch (precision) {
					case DataType::FLOAT32: outFiles.at(oi).writeVolumes(cmp_flt.begin(), cmp_flt.end(), vol, 1); break;
					case DataType::FLOAT64: outFiles.at(oi).writeVolumes(cmp_dbl.begin(), cmp_dbl.end(), vol, 1); break;
					case DataType::FLOAT128: outFiles.at(oi).writeVolumes(cmp_ldbl.begin(), cmp_ldbl.end(), vol, 1); break;
					default: break;
				}
				break;
			default:
				break;
			}

			#undef DECL_SWITCH
		}
	}

	for (File& of: outFiles) {
		of.close();
	}

	return EXIT_SUCCESS;
}
