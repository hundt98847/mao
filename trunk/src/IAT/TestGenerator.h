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
#include "./Operation.h"
#include "./Operand.h"

using namespace std;

// Function Prototypes
int main(int argc, char* argv[]);
int ParseCommandLineInt(string arg, const string flag);
string GetOutputDirectoryName();
int CountUncommentedLines(string file_name, char comment_char);
Operation GenerateOperation(char data[], const char delimiter[]);
Operand GenerateOperand(char data[], const char delimiter[]);
Assembly GenerateBaselineTest(int number_instructions, int number_iterations);
int DetermineTestCount(int number_operations, int number_operands,
                       Operation operations[], Operand operands[]);
Assembly GenerateTest(int number_instructions, int number_iterations,
                      int number_operands, Operation& operation,
                      Operand operands[]);
string GetBodyPrefix();
string GetBodyMain(Assembly& obj, int numberInstructions, int numberIterations);
string GetBodySuffix();

// Constants
const char kIndexFileName[] = "index.txt";
const char kBaselineFileName[] = "baseline.s";
const char kOperationDataFileName[] = "operations.dat";
const char kOperandDataFileName[] = "operands.dat";
const char kTestSetDataFileName[] = "test_set.dat";
const char kMakeFileName[] = "makefile";
const char kExecutableFileNamePrefix[] = "test_";
const char kExecutableFileNameSuffix[] = ".exe";
const char kTestSetDataFileHeader[] = "# This file was generated automatically "
                                      "by the Test Generator.  It contains \n"
                                      "# information specific to this test set"
                                      "and should not be deleted.";
const char kInstructionCountFlag[] = "--instructions=";
const char kIterationCountFlag[] = "--iterations=";
const char kFileCommentCharacter = '#';
const char kFileDelimiter[] = ", ";
const string kOutputDirectoryName = GetOutputDirectoryName();
const int kDefaultInstructionCount = 1000;
const int kDefaultIterationCount = 10000;
const int kMaxBufferSize = 512;
const int kArgumentsInOperationDataFile = 4;
const int kArgumentsInOperandDataFile = 3;
const int kAbsoluteMinimumOperands = 0;
const int kAbsoluteMaximumOperands = 3;

#endif /* IAT_TESTGENERATOR_H_ */
