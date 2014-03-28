//
//  main.c
//  procparse
//
//  Created by Tobias Wood on 10/07/2012.
//  Copyright (c) 2012 Tobias Wood. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <list>
#include <vector>
#include <getopt.h>

#include "Nifti/Nifti.h"
#include "Nifti/ExtensionCodes.h"
#include "procpar.h"

using namespace std;
using namespace Agilent;

static int full = false, partial = false, verbose = false;
static struct option long_options[] =
{
	{"full", no_argument, &full, true},
	{"partial", no_argument, &partial, true},
	{"verbose", no_argument, &verbose, true},
	{"in", required_argument, 0, 'i'},
	{0, 0, 0, 0}
};
const string usage {
"procparse - A utility to find interesting information in Agilent procpar files.\n\
\n\
Usage: procparse [opts] file1 par1 par2 ... parN\n\
par1 to parN are parameter names to search for in procpar. If none are specified \
then the whole file will be listed.\n\
Options:\n\
 -f, --full:       Print the full parameter information, not a shortened version.\n\
 -p, --partial:    Print parameters that are partial matches.\n\
 -i, --in file:    Check additional procpar files (can specify more than once).\n\
 -v, --verbose:    Print more information.\n"
};

int main(int argc, char **argv) {
	int indexptr = 0, c;
	list<string> paths;
	while ((c = getopt_long(argc, argv, "fphvi:", long_options, &indexptr)) != -1) {
		switch (c) {
			case 0: break; // It was an option that just sets a flag.
			case 'f': full = true; break;
			case 'p': partial = true; break;
			case 'h': cout << usage << endl; break;
			case 'v': verbose = true; break;
			case 'i': paths.emplace_back(optarg); break;
			default: cout << "Unknown option " << optarg << endl;
		}
	}
	
	if ((argc - optind) <= 0) {
		cout << "No procpar file specified." << endl << usage << endl;
		exit(EXIT_FAILURE);
	}
	paths.emplace_front((argv[optind++]));
		
	vector<ProcPar> pps;
	for (auto &p : paths) {
		ProcPar pp;
		try {
			if (p.substr(p.size() - 7) == "procpar") {
				ifstream pp_file(p);
				pp_file >> pp;
				if (!pp_file.eof()) {
					throw(runtime_error("Failed to read contents of file: " + p));
				}
				pps.push_back(pp);
			} else {
				Nifti nii(p, Nifti::Mode::ReadHeader);
				const list<Nifti::Extension> &exts = nii.extensions();
				for (auto &e : exts) {
					if (e.code() == NIFTI_ECODE_COMMENT) {
						string s(e.data().begin(), e.data().end());
						stringstream ss(s);
						ss >> pp;
						pps.push_back(pp);
					}
				}
			}
		} catch (exception &e) {
			cout << "Could not read file " << p << ", not a valid procpar file." << endl;
		}
	}
	
	while (optind < argc) {
		string searchName(argv[optind]);
		if (verbose) {
			cout << "Searching for parameter: " << searchName << endl;
		}
		auto pp_it = pps.begin();
		auto path_it = paths.begin();
		for (; pp_it != pps.end() && path_it != paths.end(); pp_it++, path_it++) {
			if (verbose) {
				cout << "In file: " << *path_it << endl;
			}
			//for (auto &n : pp_it->names())
			//	cout << n << endl;
			
			if (partial) {
				size_t matches(0);
				vector<string> names = pp_it->names();
				for (auto &n : names) {
					if (n.find(searchName) != string::npos) {
						if (full) {
							cout << pp_it->parameter(n) << endl;
						} else {
							cout << n << ": " << pp_it->parameter(n).print_values() << endl;
						}
						matches++;
					}
				}
				cout << matches << " matches." << endl;
			} else if (pp_it->contains(searchName)) {
				if (full)
					cout << pp_it->parameter(searchName) << endl;
				else
					cout << searchName << ": " << pp_it->parameter(searchName).print_values() << endl;
			} else {
				if (verbose)
					cout << "Not found." << endl;
			}
		}
		optind++;
	}
    return EXIT_SUCCESS;
}

