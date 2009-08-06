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
//              constants and function prototypes used by the analyzer for its
//              operations.
//


#ifndef IAT_ANALYZER_H_
#define IAT_ANALYZER_H_

#include <string>

#include "./Result.h"

using namespace std;

// Function Prototypes
int main(int argc, char* argv[]);
string ParseCommandLineString(string arg, string flag);
int ParseCommandLineInt(const string arg, const string flag);
int DetermineResultCount(string file_name, char comment_char);
Result GenerateResult(string result_line, string index_directory,
                      long int baseline_raw_event_count,
                      int number_instructions, int number_iterations);
int CalculateEventsPerInstruction(long int raw_events, int baseline_events,
                                  int number_instructions,
                                  int number_iterations);
int CountUncommentedLines(string file_name, char comment_char);

// Constants
const char kResultIndexFileName[] = "successfulexecution.txt";
const char kResultBaselineFileName[] = "test_baseline_results.txt";
const char kTargetDirectoryFlag[] = "--results=";
const char kInstructionCountFlag[] = "--instructions=";
const char kIterationCountFlag[] = "--iterations=";
const char kFileCommentCharacter = '#';
const char kResultFileNamePrefix[] = "test_";
const char kResultFileNameSuffix[] = "_results.txt";
const char kTestSetDataFile[] = "test_set.dat";
const char kBaselineResultFileNameBody[] = "baseline";
const char kTestSetResultFileName[] = "results.txt";
const char kFileDelimiter[] = ", ";
const int kMaxBufferSize = 512;

#endif /* IAT_ANALYZER_H_ */
