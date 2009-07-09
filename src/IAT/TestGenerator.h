//
// Copyright 2009 and later Google Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Author: Casey Burkhardt (caseyburkhardt)
// Description: File serves as the header for the test generator.  It defines
//              constants and function prototypes used by the test generator
//              for its operations.
//


#ifndef IAT_TESTGENERATOR_H_
#define IAT_TESTGENERATOR_H_

#include <string>

#include "./Assembly.h"

using namespace std;

// Function Prototypes
int ParseCommandLineInt(const string arg, const string flag);
string GetOutputDirectoryName();
string GetBodyPrefix();
string GetBodyMain(Assembly obj, int numberInstructions, int numberIterations);
string GetBodySuffix();

// Constants
const char kIndexFileName[] = "index.txt";
const char kInstructionCountFlag[] = "--instructions=";
const char kIterationCountFlag[] = "--iterations=";
const string kOutputDirectoryName = GetOutputDirectoryName();
const int kDefaultInstructionCount = 10000;
const int kDefaultIterationCount = 10000;
const int kMaxBufferSize = 512;

#endif /* IAT_TESTGENERATOR_H_ */
