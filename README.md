# QUIT - QUantitative Imaging Tools #

Credit / Blame / Contact - Tobias Wood - tobias.wood@kcl.ac.uk

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
If you find the tools useful the author would love to hear from you.

# Brief Description #

A collection of programs for processing quantitative MRI data, in particular
DESPOT and mcDESPOT.

# Installation #

## Dependencies & Requirements ##

1. A C++11 compliant compiler (GCC 4.8.0+, Clang 3.1+)
2. Eigen, version 3.2.1 or greater (http://eigen.tuxfamily.org)
3. zlib
4. Python 2.7 and TkInter (for GUI tools)

WARNING - You will require a recent compiler for DESPOT as it uses C++11
features. GCC 4.8.0 is a MINIMUM as earlier versions do not support C++11
threads. Clang 3.1 will also work. You can find out the version of your
system gcc by running "gcc -v".

## Compilation ##

1. A Makefile is provided. To compile, type "make all" in the base directory.
   Parallel compilation ("make -j all") is recommended.
2. The first time you compile, Make will automatically download the correct
   version of the Eigen library. Make sure you are connected to the internet.
3. The default install directory is QUIT/bin. If you wish to install somewhere
   else, change the INSTALL_DIR variable.

# Usage #

Manuals for the sub-suites of QUIT can be found in /doc

Each product has some basic usage instructions that will be printed with either
the -h or --help options, e.g. "mcdespot -h". The programs will prompt for
information when they need it.

