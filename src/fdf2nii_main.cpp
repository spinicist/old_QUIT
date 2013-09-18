//
//  fdf2nii_main.cpp
//
//  Created by Tobias Wood on 29/08/2013.
//
//

#include <string>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <getopt.h>

#include "fdf.h"
#include "Nifti.h"

using namespace std;

static bool zip = false, procpar = false, verbose = false;
static int echoMode = -1;
static double scale = 1.;
static string outPrefix;
static struct option long_options[] =
{
	{"scale", required_argument, 0, 's'},
	{"out", required_argument, 0, 'o'},
	{"zip", no_argument, 0, 'z'},
	{"echo", required_argument, 0, 'e'},
	{"procpar", no_argument, 0, 'p'},
	{"verbose", no_argument, 0, 'v'},
	{0, 0, 0, 0}
};

const string usage {
"fdf2nii - A utility to convert Agilent fdf files to nifti.\n\
\n\
Usage: fdf2nii [opts] image1 image2 ... imageN\n\
image1 to imageN are paths to the Agilent .img folders, not individual .fdf\n\
files\n\
Options:\n\
 -s, --scale:   Scale factor for image dimensions (set to 10 for use with SPM).\n\
 -o, --out:     Specify an output prefix.\n\
 -z, --zip:     Create .nii.gz files\n\
 -e, --echo N:  Choose echo N in a multiple echo file. Valid values for N are:\n\
                0..max echo   Write out just this echo\n\
				-1 (default)  Write out all echoes as individual images.\n\
				-2            Sum echoes\n\
				-3            Average echoes\n\
				If an echo is chosen beyond the maximum nothing is written.\n\
 -p, --procpar: Embed procpar in the nifti header.\n\
 -v, --verbose: Print out extra info (e.g. after each volume is written).\n"
};

int main(int argc, char **argv) {
	int indexptr = 0, c;
	while ((c = getopt_long(argc, argv, "s:o:ze:pv", long_options, &indexptr)) != -1) {
		switch (c) {
			case 0: break; // It was an option that just sets a flag.
			case 's': scale = atof(optarg); break;
			case 'o': outPrefix = string(optarg); break;
			case 'z': zip = true; break;
			case 'e':
				echoMode = atoi(optarg);
				if (echoMode < -3) {
					throw (invalid_argument("Invalid echo mode: " + to_string(echoMode)));
				}
				break;
			case 'p': procpar = true; break;
			case 'v': verbose = true; break;
			default: cout << "Unknown option " << optarg << endl;
		}
	}
	
	if ((argc - optind) <= 0) {
		cout << "No input images specified." << endl << usage << endl;
		exit(EXIT_FAILURE);
	}
	while (optind < argc) {
		string inPath(argv[optind]);
		optind++;
		size_t fileSep = inPath.find_last_of("/") + 1;
		size_t fileExt = inPath.find_last_of(".");
		if (inPath.substr(fileExt) != ".img") {
			cerr << inPath << " is not a valid .img folder. Skipping." << endl;
			continue;
		}
		string outPath = outPrefix + inPath.substr(fileSep, fileExt - fileSep) + ".nii";

		if (zip)
			outPath += ".gz";
		if (verbose)
			cout << "Converting " << inPath << " to " << outPath << "..." << endl;
		try {
			Agilent::fdfImage input(inPath);
			size_t nOutImages = input.dim(3);
			if (echoMode == -1) {
				nOutImages *= input.dim(4);
			} else if ((echoMode >= 0) && (echoMode >= static_cast<int>(input.dim(4)))) {
				throw(invalid_argument("Selected echo was above the maximum."));
			}
			try {
				Nifti::File output(input.dims(), (input.voxdims() * scale).cast<float>(), DT_FLOAT32, input.transform().cast<float>());
				output.setDim(4, nOutImages);
				if (procpar) {
					ifstream pp_file(inPath + "/procpar", ios::binary);
					pp_file.seekg(ios::end);
					size_t fileSize = pp_file.tellg();
					pp_file.seekg(ios::beg);
					vector<char> data; data.reserve(fileSize);
					data.assign(istreambuf_iterator<char>(pp_file), istreambuf_iterator<char>());
					output.addExtension(NIFTI_ECODE_COMMENT, data);
				}
				output.open(outPath, Nifti::Modes::Write);
				size_t outVol = 0;
				for (size_t inVol = 0; inVol < input.dim(3); inVol++) {
					if (verbose)
						cout << "Writing volume " << (outVol + 1) << " of " << nOutImages << endl;
					if (echoMode >= 0) {
						output.writeVolume<float>(outVol++, input.readVolume<float>(inVol, echoMode));
					} else if (echoMode == -1) {
						for (size_t e = 0; e < input.dim(4); e++) {
							output.writeVolume<float>(outVol++, input.readVolume<float>(inVol, e));
						}
					} else {
						vector<float> sum = input.readVolume<float>(inVol, 0);
						for (size_t e = 1; e < input.dim(4); e++) {
							vector<float> data = input.readVolume<float>(inVol, e);
							transform(sum.begin(), sum.end(), data.begin(), sum.begin(), plus<float>());
						}
						if (echoMode == -3) {
							transform(sum.begin(), sum.end(), sum.begin(), [&](float &f) { return f / input.dim(4); });
						}
						output.writeVolume<float>(outVol++, sum);
					}
				}
				output.close();
			} catch (exception &e) {
				cerr << "Error, skipping to next input. " << e.what() << endl;
				continue;
			}
		} catch (exception &e) {
			cerr << "Error, skipping to next input. " << e.what() << endl;
			continue;
		}
		if (verbose)
			cout << "Finished writing file " << outPath << endl;
	}
	if (verbose)
		cout << "Finished." << endl;
    return EXIT_SUCCESS;
}
