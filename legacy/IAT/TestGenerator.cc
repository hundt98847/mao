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
// Description: File serves as the main operational source for the test
//              generator.  It parses command line arguments, generates
//              assembly objects, and writes the output files to the file
//              system.


#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>
#include <sstream>
#include <string>
#include <vector>

#include "Assembly.h"
#include "Operation.h"
#include "Operand.h"

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
const char kVerbosityFlag[] = "--verbose";
const char kHelpFlag[] = "--help";
const char kHelpText[] =
"Usage: TestGenerator [ARGUMENT=VALUE]... [ARGUMENT]...\n"
"Exhaustively generates assembly file tests based on operations and operands\n"
"data files.\n\n"
"All command line arguments are optional.\n"
"  --instructions=     The number of instructions contained within the body\n"
"                        loop of each assembly test.\n"
"  --iterations=       The number of times each assembly test should iterate\n"
"                        over the body instructions.\n"
"  --verbose           Display status messages for each processed operation, \n"
"                        operand, and test.\n"
"  --help              Display this help message";
const char kFileCommentCharacter = '#';
const char kFileDelimiter[] = ", ";
const char kTestEnabledKeyword[] = "test";
const int kDefaultInstructionCount = 100;
const int kDefaultIterationCount = 10000;
const int kMaxBufferSize = 512;
const int kArgumentsInOperationDataFile = 4;
const int kArgumentsInOperandDataFile = 3;
const int kAbsoluteMinimumOperands = 0;
const int kAbsoluteMaximumOperands = 3;

bool verbose = false;

std::string GetBodyPrefix() {
  return ".text\n"
      ".globl main\n"
      ".type   main, @function\n"
      "main:\n"
      ".LFB2:\n"
      "pushq   %rbp\n"
      ".LCFI0:\n"
      "movq    %rsp, %rbp\n"
      ".LCFI1:\n"
      "movl    $0, -4(%rbp)\n"
      "sub    $0x8, %rsp\n"
      "leal -74(%rbp), %ebx\n"
      "jmp     .L2\n"
      ".L3:";
}

std::string GetBodyMain(std::string instruction_name, int number_operands,
                        std::vector<Operand*> operands, int number_instructions,
                        int number_iterations) {
  std::stringstream result;

  if (instruction_name != "") {
    for (int i = 0; i < number_instructions; ++i) {
      result << " " << instruction_name;
      for (int j = 0; j < number_operands; ++j) {
        if (j == 0) {
          result << "  " << operands[j]->operand_value();
        } else {
          result << ", " << operands[j]->operand_value();
        }
      }
      result << std::endl;
    }
  }
  result << "  add  $1, -4(%rbp)\n"
      ".L2:\n"
      "cmpl  $" << number_iterations << ", -4(%rbp)";
  return result.str();
}

std::string GetBodySuffix() {
  return "jle     .L3\n"
      "movl    $0, %eax\n"
      "leave\n"
      "ret\n"
      ".LFE2:\n"
      ".size   main, .-main\n"
      ".section        .eh_frame,\"a\",@progbits\n"
      ".Lframe1:\n"
      ".long   .LECIE1-.LSCIE1\n"
      ".LSCIE1:\n"
      ".long   0x0\n"
      ".byte   0x1\n"
      ".string \"zR\"\n"
      ".uleb128 0x1\n"
      ".sleb128 -8\n"
      ".byte   0x10\n"
      ".uleb128 0x1\n"
      ".byte   0x3\n"
      ".byte   0xc\n"
      ".uleb128 0x7\n"
      ".uleb128 0x8\n"
      ".byte   0x90\n"
      ".uleb128 0x1\n"
      ".align 8\n"
      ".LECIE1:\n"
      ".LSFDE1:\n"
      ".long   .LEFDE1-.LASFDE1\n"
      ".LASFDE1:\n"
      ".long   .LASFDE1-.Lframe1\n"
      ".long   .LFB2\n"
      ".long   .LFE2-.LFB2\n"
      ".uleb128 0x0\n"
      ".byte   0x4\n"
      ".long   .LCFI0-.LFB2\n"
      ".byte   0xe\n"
      ".uleb128 0x10\n"
      ".byte   0x86\n"
      ".uleb128 0x2\n"
      ".byte   0x4\n"
      ".long   .LCFI1-.LCFI0\n"
      ".byte   0xd\n"
      ".uleb128 0x6\n"
      ".align 8\n"
      ".LEFDE1:\n"
      ".ident  \"GCC: (GNU) 4.2.4 (Ubuntu 4.2.4-1ubuntu4)\"\n"
      ".section        .note.GNU-stack,\"\",@progbits";
}

// This function returns the integer value of a command line argument, given the
// actual argument and anticipated argument flag
int ParseCommandLineInt(std::string arg, const std::string flag) {
  arg = arg.substr(flag.length());
  return atoi(arg.c_str());
}

// This function determines the output directory named based on the current
// date and time of the system clock.  Due to some undefined behavior with the
// asctime function, the last character needed to be truncated, and all ' 's and
// ':'s were replaced with '_'s.
std::string GetOutputDirectoryName() {
  time_t raw_time;
  std::string result;
  struct tm * time_info;

  time(&raw_time);
  time_info = localtime(&raw_time);
  result = asctime(time_info);
  for (unsigned int i = 0; i < result.length(); ++i) {
    if (result[i] == ' ' || result[i] == ':')
      result[i] = '_';
  }
  return result.substr(0, result.length() - 1);
}

// This function returns the number of uncommented lines within a file.  This is
// used to determine the size of the array used to hold the Operation and
// Operand objects.
int CountUncommentedLines(std::string file_name, char comment_char) {
  int result = 0;
  FILE *file_in;
  char line[kMaxBufferSize];

  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open file.\n");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != comment_char) {
      ++result;
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close file.\n");
    exit(1);
  }
  return result;
}

// This function is intended to check the validity of the syntax of a line from
// the operations data file, which is supposed to be of the following format:
// operation_name, test/ignore, min_operands, max_operands
//   Legend:
//   (Note: Function short-circuits in this order as well)
//     operation_name = The name of the operation. (add)
//     test/ignore = "test" to generate tests for this operation
//                   "ignore" to not load this operation into the Test Generator
//     min_operands = The minimum number of operands for this operation
//     max_operands = The maximum number of operands for this operation
//
// If the sanity check passes, the function will return a pointer to a fully
// provisioned object of type Operation, otherwise it will return a null
// pointer.
Operation* GenerateOperation(char data[], const char delimiter[]) {
  // String array to hold tokenized data from a single line of the operations
  // data file.
  std::string args[kArgumentsInOperationDataFile];

  // Min/Max Operands Value
  int min_operands = 0;
  int max_operands = 0;

  // Populate the string array by tokenizing the line.
  // First argument
  args[0] = strtok(data, delimiter);
  // Following Arguments
  for (int i = 1; i < kArgumentsInOperationDataFile; ++i) {
    args[i] = strtok(NULL, delimiter);
  }

  // Check Argument 0: Operation Name
  if (args[0] == "") {
    return NULL;
  }
  // Check Argument 1: Operation Status
  // Value of "test" will result in sanity check passing.  Other values will
  // cause failure.  "ignore" values also cause failure because the object
  // isn't needed anyway.
  if (args[1] == "") {
    return NULL;
  } else if (args[1].compare(kTestEnabledKeyword) != 0) {
    return NULL;
  }
  // Check Argument 2: Minimum Number of Operands
  if (args[2] == "") {
    return NULL;
  }
  min_operands = atoi(args[2].c_str());
  if (min_operands < kAbsoluteMinimumOperands ||
      min_operands > kAbsoluteMaximumOperands) {
    return NULL;
  }
  // Check Argument 3: Maximum Number of Operands
  if (args[3] == "") {
    return NULL;
  }
  max_operands = atoi(args[3].c_str());
  if (max_operands < min_operands ||
      max_operands > kAbsoluteMaximumOperands) {
    return NULL;
  }
  // All sanity checks passed.
  // Initialize Operation Object and return pointer.
  Operation* result = new Operation(args[0], min_operands, max_operands);
  return result;
}

// This function is intended to check the validity of the syntax of a line from
// the operands data file, which is supposed to be of the following format:
// operand_value, test/ignore, operand_type
// Legend:
//   operation_value = The value of the operand when output to an assembly file
//   test/ignore = "test" to generate tests for this operand
//                 "ignore" to not load this operand into the Test Generator
//   operand_type = The type of the operand and its size
//
// If the sanity check passes, the function will return a pointer to a fully
// provisioned object of type Operand, otherwise it will return a null pointer.
Operand* GenerateOperand(char data[], const char delimiter[]) {
  // String array to hold tokenized data from a single line of the operations
  // data file.
  std::string args[kArgumentsInOperandDataFile];

  // Populate the string array by tokenizing the line.
  // First argument
  args[0] = strtok(data, delimiter);
  // Following Arguments
  for (int i = 1; i < kArgumentsInOperandDataFile; ++i) {
    args[i] = strtok(NULL, delimiter);
  }

  // Check Argument 0: Operand Value
  if (args[0] == "") {
    return NULL;
  }
  // Check Argument 1: Operand Status
  // Value of "test" will result in sanity check passing.  Other values will
  // cause failure.  "ignore" values also cause failure because the object
  // isn't needed anyway.
  if (args[1] == "") {
    return NULL;
  } else if (args[1].compare("test") != 0) {
    return NULL;
  }
  // Check Argument 2: Operand Type
  if (args[2] == "") {
    return NULL;
  }

  // All sanity checks passed.
  // Initialize Operand Object and return pointer.
  Operand* result = new Operand(args[0], args[2]);
  return result;
}

// This function generates a baseline test, an assembly object that contains no
// instruction or addressing mode.  It returns a pointer to the Assembly object
// generated.
Assembly* GenerateBaselineTest(int number_instructions, int number_iterations) {
  // Set Baseline Data Members
  std::stringstream instruction_body;
  std::vector<Operand*> blank_operands;

  // Append the instructions to the Baseline Test
  instruction_body << GetBodyPrefix() + "\n";
  instruction_body << GetBodyMain("", 0, blank_operands, number_instructions,
                                  number_iterations) << std::endl;
  instruction_body << GetBodySuffix() << std::endl;

  Assembly* result = new Assembly("", "", instruction_body.str(),
                                  kBaselineFileName, 0, NULL, blank_operands);

  return result;
}

int DetermineTestCount(int number_operations, int number_operands,
                       Operation operations[], Operand operands[]) {
  int result = 0;
  for (int i = 0; i < number_operations; ++i) {
    if (operations[i].min_operands() <= 0
        && operations[i].max_operands() >= 0) {
      // Operation can have 0 operands.  This constitutes a single test.
      result += 1;
    }
    if (operations[i].min_operands() <= 1
        && operations[i].max_operands() >= 1) {
      // Operation can have 1 operand.  This constitutes a number_operands^1
      // tests.
      result += number_operands;
    }
    if (operations[i].min_operands() <= 2
        && operations[i].max_operands() >= 2) {
      // Operation can have 2 operands.  This constitutes a number_operands^2
      // tests.
      result += pow(number_operands, 2);
    }
    if (operations[i].min_operands() <= 3
        && operations[i].max_operands() >= 3) {
      // Operation can have 3 operands.  This constitutes a number_operands^3
      // tests.
      result += pow(number_operands, 3);
    }
  }
  // Add 1 for the baseline test
  ++result;
  return result;
}

// This function initializes and returns a pointer to a fully populated
// Assembly object, representing an assembly test.
Assembly* GenerateTest(int number_instructions, int number_iterations,
                      int number_operands, Operation* operation,
                      Operand* operands[]) {
  std::string addressing_mode = "";
  std::string file_name = "";
  std::string instruction_body;

  // Generate Operands Vector
  std::vector<Operand*> v_operands;
  for (int i = 0; i < number_operands; ++i) {
    v_operands.push_back(operands[i]);
  }

  // Add some string formatting tricks to fix null character bug
  for (int i = 0; i < number_operands; i++) {
    addressing_mode += operands[i]->operand_type();
    addressing_mode = addressing_mode.substr(0, addressing_mode.length() - 1);
    addressing_mode += "_";
  }
  addressing_mode = addressing_mode.substr(0, addressing_mode.length() - 1);

  if (number_operands > 0) {
    file_name = operation->operation_name() + "_" + addressing_mode + ".s";
  } else {
    file_name = operation->operation_name() + ".s";
  }
  if (verbose) {
    printf("Generating Test: %s\n", file_name.c_str());
  }

  // Append the instructions to the assembly object
  instruction_body = GetBodyPrefix() + "\n";
  instruction_body += GetBodyMain(operation->operation_name(), number_operands,
                                  v_operands, number_instructions,
                                  number_iterations) + "\n";
  instruction_body += GetBodySuffix() + "\n";

  Assembly* result = new Assembly(operation->operation_name(), addressing_mode,
                                  instruction_body, file_name, number_operands,
                                  operation, v_operands);
  return result;
}

// Checks to see if a given command line flag is present within a given command
// line argument.
bool ArgumentHasFlag(std::string argument, std::string flag) {
  return strncmp(argument.c_str(), flag.c_str(), flag.length()) == 0;
}

int main(int argc, char* argv[]) {
  // Variables used for array size declaration
  int number_iterations = 0;
  int number_instructions = 0;

  // Variables and Objects used for file operations
  std::string file_name;
  FILE *file_in;
  FILE *out_file;
  char line[kMaxBufferSize];
  Operation* current_operation;
  Operand* current_operand;

  // Command Line Argument Parsing
  for (int i = 1; i < argc; ++i) {
    if (ArgumentHasFlag(argv[i], kInstructionCountFlag)) {
      number_instructions = ParseCommandLineInt(argv[i], kInstructionCountFlag);
    } else if (ArgumentHasFlag(argv[i], kIterationCountFlag)) {
      number_iterations = ParseCommandLineInt(argv[i], kIterationCountFlag);
    } else if (ArgumentHasFlag(argv[i], kVerbosityFlag)) {
      verbose = true;
    } else if (ArgumentHasFlag(argv[i], kHelpFlag)) {
          printf("%s\n", kHelpText);
          exit(0);
    } else {
      fprintf(stderr, "Ignoring Unknown Command Line Argument: %s\n", argv[i]);
    }
  }

  // Check (and replace if necessary) Command Line Argument Values
  if (number_instructions <= 0) {
    number_instructions = kDefaultInstructionCount;
    fprintf(stderr,
            "Instruction Count Flag Missing or Invalid - Defaulting to: %d\n",
            kDefaultInstructionCount);
  } else {
    printf("Instruction Count Set to: %d\n", number_instructions);
  }
  if (number_iterations <= 0) {
    number_iterations = kDefaultIterationCount;
    fprintf(stderr,
            "Iteration Count Flag Missing or Invalid - Defaulting to: %d\n",
            kDefaultIterationCount);
  } else {
    printf("Iteration Count Set to: %d\n", number_iterations);
  }

  // Process Operations
  // Holds the instances of Operation Objects
  std::vector<Operation*> operations;

  // Set all members for all Operation Objects
  current_operation = NULL;

  if ((file_in = fopen(kOperationDataFileName, "r")) == 0) {
    perror("Unable to open operations data file.\n");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != kFileCommentCharacter) {
      current_operation = GenerateOperation(line, kFileDelimiter);
      if (current_operation == NULL) {
        fprintf(stderr,
                "Ignored operation or data syntax error in %s: Ignoring: %s\n",
                kOperationDataFileName, line);
      } else {
        operations.push_back(current_operation);
        if (verbose) {
          printf("Successfully processed operation: %s\n",
                 current_operation->operation_name().c_str());
        }
      }
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close operations data file.\n");
    exit(1);
  }
  // TODO(caseyburkhardt): Check to see if size_t warning is appropriate.
  printf("Successfully processed %d operations.\n", operations.size());

  // Process Operands
  // Holds the instances of Operand Objects
  std::vector<Operand*> operands;

  // Set all members for all Operation Objects
  current_operand = NULL;

  if ((file_in = fopen(kOperandDataFileName, "r")) == 0) {
    perror("Unable to open operands data file.\n");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != kFileCommentCharacter) {
      current_operand = GenerateOperand(line, kFileDelimiter);
      if (current_operand == NULL) {
        fprintf(stderr,
                "Ignored operand or data syntax error in %s: Ignoring: %s\n",
                kOperandDataFileName, line);
      } else {
        operands.push_back(current_operand);
       if (verbose) {
         printf("Successfully processed operand: %s\n",
                current_operand->operand_value().c_str());
       }
      }
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close operations data file.\n");
    exit(1);
  }
  printf("Successfully processed %d operands.\n", operands.size());

  // Holds the instances of Assembly Objects
  std::vector<Assembly*> tests;

  // Generate Assembly Objects
  // Generate the Initial Baseline Test
  tests.push_back(GenerateBaselineTest(number_instructions, number_iterations));

  printf("Generating tests...\n");
  // By exhaustive means, generate all assembly tests.
  // TODO(caseyburkhardt): Determine a way to perm. the number of operands
  for (int i = 0; i < operations.size(); ++i) {
    if (operations[i]->min_operands() <= 0
        && operations[i]->max_operands() >= 0) {
      // Operation can have 0 operands.
      tests.push_back(GenerateTest(number_instructions, number_iterations, 0,
                                   operations[i], NULL));
    }
    if (operations[i]->min_operands() <= 1
        && operations[i]->max_operands() >= 1) {
      // Operation can have 1 operand.
      for (int j = 0; j < operands.size(); ++j) {
        Operand* pass_operands[] = {operands[j]};
        tests.push_back(GenerateTest(number_instructions, number_iterations, 1,
                                     operations[i], pass_operands));
      }
    }
    if (operations[i]->min_operands() <= 2
        && operations[i]->max_operands() >= 2) {
      // Operation can have 2 operands.
      for (int j = 0; j < operands.size(); ++j) {
        for (int k = 0; k < operands.size(); ++k) {
          Operand* pass_operands[] = {operands[j], operands[k]};
          tests.push_back(GenerateTest(number_instructions, number_iterations,
                                       2, operations[i], pass_operands));
        }
      }
    }
    if (operations[i]->min_operands() <= 3
        && operations[i]->max_operands() >= 3) {
      // Operation can have 3 operands.
      for (int j = 0; j < operands.size(); ++j) {
        for (int k = 0; k < operands.size(); ++k) {
          for (int l = 0; l < operands.size(); ++l) {
            Operand* pass_operands[] = {operands[j], operands[k], operands[l]};
            tests.push_back(GenerateTest(number_instructions, number_iterations,
                                         3, operations[i], pass_operands));
          }
        }
      }
    }
  }

  printf("Done.  Generated %d Tests.\n", tests.size());

  // Create Assembly Output Directory
  std::string OutputDirectoryName = GetOutputDirectoryName();
  if (mkdir(OutputDirectoryName.c_str(), 0777) == -1) {
    perror("Unable to create output directory.\n");
    exit(1);
  }

  printf("Writing %d tests to file system...\n", tests.size());

  // Write Assembly Files
  for (int i = 0; i < tests.size(); ++i) {
    file_name = OutputDirectoryName + "/" + tests[i]->file_name();
    if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
      perror("Unable to open output file.\n");
      exit(1);
    }
    fprintf(out_file, "%s", tests[i]->instruction_body().c_str());
    if (fclose(out_file) != 0) {
      perror("Unable to close output file.\n");
      exit(1);
    }
  }

  printf("Done.  Wrote %d tests to file system.\n", tests.size());
  printf("Writing index to file system...\n");

  // Write Index File
  file_name = OutputDirectoryName + "/" + kIndexFileName;
  if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
    perror("Unable to open index file.\n");
    exit(1);
  }
  for (int i = 0; i < tests.size(); ++i) {
    fprintf(out_file, "%s\n", tests[i]->file_name().c_str());
  }
  if (fclose(out_file) != 0) {
    perror("Unable to close index file.\n");
    exit(1);
  }

  printf("Done.  Wrote index file to file system.\n");
  printf("Writing test set data file to file system...\n");
  // Write test set data file
  // This will be used with the analysis tool to keep track of instruction and
  // iteration count values
  file_name = OutputDirectoryName + "/" + kTestSetDataFileName;
  if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
    perror("Unable to open test set data file.\n");
    exit(1);
  }
    fprintf(out_file, "%s\n%s%d\n%s%d", kTestSetDataFileHeader,
            kInstructionCountFlag, number_instructions, kIterationCountFlag,
            number_iterations);
  if (fclose(out_file) != 0) {
    perror("Unable to close test set data file.\n");
    exit(1);
  }

  printf("Done.  Wrote test set data file to file system.\n");
  printf("Writing makefile to file system.\n");

  // Write makefile
  file_name = OutputDirectoryName + "/" + kMakeFileName;
  if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
    perror("Unable to open makefile.\n");
    exit(1);
  }
  fprintf(out_file, "all:");
  for (int i = 0; i < tests.size(); ++i) {
    if (tests[i]->instruction_name() != "") {
      // Baseline Test
      fprintf(out_file, " %s%s_%s%s", kExecutableFileNamePrefix,
              tests[i]->instruction_name().c_str(),
              tests[i]->addressing_mode().c_str(), kExecutableFileNameSuffix);
    } else {
      fprintf(out_file, " %sbaseline%s", kExecutableFileNamePrefix,
              kExecutableFileNameSuffix);
    }
  }
  fprintf(out_file, "\n\n");
  for (int i = 0; i < tests.size(); ++i) {
    if (tests[i]->instruction_name() != "") {
      fprintf(out_file, " %s%s_%s%s: %s\n\tgcc %s -o %s%s_%s%s\n\n",
              kExecutableFileNamePrefix, tests[i]->instruction_name().c_str(),
              tests[i]->addressing_mode().c_str(), kExecutableFileNameSuffix,
              tests[i]->file_name().c_str(), tests[i]->file_name().c_str(),
              kExecutableFileNamePrefix, tests[i]->instruction_name().c_str(),
              tests[i]->addressing_mode().c_str(), kExecutableFileNameSuffix);
    } else {
      fprintf(out_file, " %sbaseline%s: %s\n\tgcc %s -o %sbaseline%s\n\n",
              kExecutableFileNamePrefix, kExecutableFileNameSuffix,
              tests[i]->file_name().c_str(), tests[i]->file_name().c_str(),
              kExecutableFileNamePrefix, kExecutableFileNameSuffix);
    }
  }
  if (fclose(out_file) != 0) {
    perror("Unable to close makefile.\n");
    exit(1);
  }

  // TODO(caseyburkhardt): Clear Vectors
  printf("Done.  All operations completed.\n");

  return(0);
}
