/*
 *  Util.h
 *  Part of the QUantitative Image Toolbox
 *
 *  Copyright (c) 2014 Tobias Wood. All rights reserved.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef QUIT_UTIL_H
#define QUIT_UTIL_H

#include <string>
#include <map>
#include <vector>
#include <random>
#include <functional>
#include <time.h>

#include "Agilent/procpar.h"
#include "Nifti/Nifti.h"
#include "Nifti/ExtensionCodes.h"

namespace QUIT {

const std::string &OutExt(); //!< Return the extension stored in $QUIT_EXT
bool ReadPP(const Nifti::File &nii, Agilent::ProcPar &pp);
time_t printStartTime();
time_t printElapsedTime(const time_t &start);
void printElapsedClock(const clock_t &clockStart, const int voxCount);
void printLoopTime(const clock_t &loopStart, const int voxCount);
void checkHeaders(const Nifti::Header &n1, std::vector<Nifti::File> n_other); //!< Throws an exception if the passed in Nifti files do not share same matrix size and transform

template<typename T>
T randNorm(double sigma)
{
  static std::mt19937_64 twister(time(NULL));
  static std::normal_distribution<T> nd(0., sigma);
  return nd(twister);
}


template<typename T> class Read;

template<typename T> class Read {
	public:
	static void FromLine(std::istream &in, T &val) {
		std::string line;
		if (!std::getline(in, line)) {
			throw(std::runtime_error("Failed to read input."));
		}
		std::istringstream stream(line);
		if (!(stream >> val)) {
			throw(std::runtime_error("Failed to parse input line: " + line));
		}
	}
};

template<typename T> class Read<Eigen::Array<T, Eigen::Dynamic, 1>> {
	public:
	static void FromLine(std::istream & in, Eigen::Array<T, Eigen::Dynamic, 1> &vals) {
		std::string line;
		if (!std::getline(in, line)) {
			throw(std::runtime_error("Failed to read input."));
		}
		std::istringstream stream(line);
		for (typename Eigen::Array<T, Eigen::Dynamic, 1>::Index i = 0; i < vals.size(); i++) {
			if (!(stream >> vals[i])) {
				throw(std::runtime_error("Failed to parse input line: " + line));
			}
		}
	}
};

} // End namespace QUIT

#endif // QUIT_UTIL_H
