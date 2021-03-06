/*
 *  Util.cpp
 *  Part of the QUantitative Image Toolbox
 *
 *  Copyright (c) 2014 Tobias Wood. All rights reserved.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "QUIT/Util.h"

using namespace std;

namespace QUIT {

const std::string &OutExt() {
	static char *env_ext = getenv("QUIT_EXT");
	static string ext;
	static bool checked = false;
	if (!checked) {
		static map<string, string> valid_ext{
			{"NIFTI", ".nii"},
			{"NIFTI_PAIR", ".img"},
			{"NIFTI_GZ", ".nii.gz"},
			{"NIFTI_PAIR_GZ", ".img.gz"},
		};
		if (!env_ext || (valid_ext.find(env_ext) == valid_ext.end())) {
			cerr << "Environment variable QUIT_EXT is not valid, defaulting to NIFTI_GZ" << endl;
			ext = valid_ext["NIFTI_GZ"];
		} else {
			ext = valid_ext[env_ext];
		}
		checked = true;
	}
	return ext;
}

bool ReadPP(const Nifti::File &nii, Agilent::ProcPar &pp) {
	const list<Nifti::Extension> &exts = nii.extensions();
	for (auto &e : exts) {
		if (e.code() == NIFTI_ECODE_COMMENT) {
			string s(e.data().begin(), e.data().end());
			stringstream ss(s);
			ss >> pp;
			return true;
		}
	}
	// If we got to here there are no procpar extensions, try the old method
	string path = nii.basePath() + ".procpar";
	ifstream pp_file(path);
	if (pp_file) {
		pp_file >> pp;
		return true;
	}
	return false;
}

time_t printStartTime() {
	time_t theTime = time(NULL);
	char timeStr[256];
	strftime(timeStr, 256, "%H:%M:%S", localtime(&theTime));
	cout << "Started at " << timeStr << endl;
	return theTime;
}

time_t printElapsedTime(const time_t &startTime) {
    time_t theTime = time(NULL);
    char timeStr[256];
    strftime(timeStr, 512, "%H:%M:%S", localtime(&theTime));
    double elapsed = difftime(theTime, startTime);
	cout << "Finished at " << timeStr << ". Elapsed time was " << elapsed << " s." << endl;
	return theTime;
}

void printElapsedClock(const clock_t &startClock, const int voxCount) {
	clock_t endClock = clock();
	float totalMilliseconds = (endClock - startClock) * (1.e3 / CLOCKS_PER_SEC);
	cout << "Total CPU time: " << totalMilliseconds << " ms" << endl;
	cout << "Average voxel CPU time: " << totalMilliseconds / voxCount << " ms" << endl;
}

void printLoopTime(const clock_t &loopStart, const int voxCount) {
	clock_t loopEnd = clock();
	if (voxCount > 0) {
		cout << voxCount << " unmasked voxels, CPU time per voxel was "
		     << ((loopEnd - loopStart) / ((float)voxCount * CLOCKS_PER_SEC)) << " s" << endl;
	} else {
		cout << " no voxels." << endl;
	}
}

void checkHeaders(const Nifti::Header &n1, std::vector<Nifti::File> ns) {
	auto compare = ns.begin();
	while (compare != ns.end()) {
		if (*compare && !(n1.matchesSpace(compare->header()))) {
			throw(runtime_error("Incompatible matrix or transform in file: " + compare->imagePath()));
		}
		compare++;
	}
}

mt19937_64::result_type RandomSeed() {
	static random_device rd;
	static mt19937_64 rng;
	static bool init = false;
	mutex seed_mtx;
	if (!init) {
		rng = mt19937_64(rd());
	}
	seed_mtx.lock();
	mt19937_64::result_type r = rng();
	seed_mtx.unlock();
	return r;
}

} // End namespace QUIT
