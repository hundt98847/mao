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
#include <time.h>
#include <math.h>
#include <string>
#include <sstream>

#include "./TestGenerator.h"

int main(int argc, char* argv[]) {
  // Variables used for array size declaration
  int number_iterations = 0;
  int number_instructions = 0;
  int number_operations = 0;
  int number_operands = 0;
  int number_tests = 0;
  int number_tests_generated = 0;

  // Variables and Objects used for file operations
  string file_name;
  FILE *file_in;
  char line[kMaxBufferSize];
  int current_index = 0;

  // Command Line Argument Parsing
  // TODO(caseyburkahrdt): Add argument to display help menu
  // TODO(caseyburkhardt): Add argument to trigger verbose mode
  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], kInstructionCountFlag,
                strlen(kInstructionCountFlag)) == 0) {
      number_instructions = ParseCommandLineInt(argv[i], kInstructionCountFlag);
    } else if (strncmp(argv[i], kIterationCountFlag,
                    strlen(kIterationCountFlag)) == 0) {
      number_iterations = ParseCommandLineInt(argv[i], kIterationCountFlag);
    } else {
      printf("Ignoring Unknown Command Line Argument: %s\n", argv[i]);
    }
  }

  // Check (and replace if necessary) Command Line Argument Values
  if (number_instructions <= 0) {
    number_instructions = kDefaultInstructionCount;
    printf("Instruction Count Flag Missing or Invalid - Defaulting to: %d\n",
           kDefaultInstructionCount);
  } else {
    printf("Instruction Count Set to: %d\n", number_instructions);
  }
  if (number_iterations <= 0) {
    number_iterations = kDefaultIterationCount;
    printf("Iteration Count Flag Missing or Invalid - Defaulting to: %d\n",
           kDefaultIterationCount);
  } else {
    printf("Iteration Count Set to: %d\n", number_iterations);
  }

  // Process Operations
  // Determines the number of Operation Objects by counting the number of non-
  // commented lines in the operations data file, kOperationDataFileName
  // This is an estimated value as some lines may not be processed.
  number_operations = CountUncommentedLines(kOperationDataFileName,
                                            kFileCommentCharacter);

  // Holds the instances of Operation Objects
  Operation operations[number_operations];

  // Set all members for all Operation Objects
  file_name = kOperationDataFileName;
  current_index = 0;

  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open operations data file.\n");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != kFileCommentCharacter) {
      operations[current_index] = GenerateOperation(line, kFileDelimiter);
      if (operations[current_index].operation_name() == "") {
        printf("Ignored operation or data syntax error in %s: Ignoring: %s\n",
               kOperationDataFileName, line);
      } else {
        printf("Successfully processed operation: %s\n",
               operations[current_index].operation_name().c_str());
        ++current_index;
      }
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close operations data file.\n");
    exit(1);
  }
  number_operations = current_index;
  printf("Successfully proceed %d operations.\n", number_operations);


  // Process Operands
  // Determines the number of Operand Objects by counting the number of non-
  // commented lines in the operands data file, kOperandDataFileName
  // This is an estimated value as some lines may not be processed.
  number_operands = CountUncommentedLines(kOperandDataFileName,
                                            kFileCommentCharacter);

  // Holds the instances of Operand Objects
  Operand operands[number_operands];

  // Set all members for all Operation Objects
  current_index = 0;
  file_name = kOperandDataFileName;

  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open operands data file.\n");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != kFileCommentCharacter) {
      operands[current_index] = GenerateOperand(line, kFileDelimiter);
      if (operands[current_index].operand_value() == "") {
        printf("Ignored operand or data syntax error in %s: Ignoring: %s\n",
               kOperandDataFileName, line);
      } else {
        printf("Successfully processed operand: %s\n",
               operands[current_index].operand_value().c_str());
        ++current_index;
      }
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close operations data file.\n");
    exit(1);
  }
  number_operands = current_index;
  printf("Successfully proceed %d operands.\n", number_operands);

  // Before Assembly Objects can be generated, the total number of objects to
  // be generated must first be determined.
  number_tests = DetermineTestCount(number_operations, number_operands,
                                    operations, operands);

  // Holds the instances of Assembly Objects
  Assembly tests[number_tests];

  // Generate Assembly Objects
  // Generate the Initial Baseline Test
  tests[0] = GenerateBaselineTest(number_instructions, number_iterations);
  ++number_tests_generated;

  printf("Generating %d tests...\n", number_tests);

  // By exhaustive means, generate all assembly tests.
  for (int i = 0; i < number_operations; ++i) {
    if (operations[i].min_operands() <= 0
        && operations[i].max_operands() >= 0) {
      // Operation can have 0 operands.
      tests[number_tests_generated] = GenerateTest(number_instructions,
                                                   number_iterations, 0,
                                                   operations[i], NULL);
      ++number_tests_generated;
    }
    if (operations[i].min_operands() <= 1
        && operations[i].max_operands() >= 1) {
      // Operation can have 1 operand.
      for (int j = 0; j < number_operands; ++j) {
        Operand pass_operands[] = {operands[j]};
        tests[number_tests_generated] = GenerateTest(number_instructions,
                                                     number_iterations, 1,
                                                     operations[i],
                                                     pass_operands);
        ++number_tests_generated;
      }
    }
    if (operations[i].min_operands() <= 2
        && operations[i].max_operands() >= 2) {
      // Operation can have 2 operands.
      for (int j = 0; j < number_operands; ++j) {
        for (int k = 0; k < number_operands; ++k) {
          Operand pass_operands[] = {operands[j], operands[k]};
          tests[number_tests_generated] = GenerateTest(number_instructions,
                                                       number_iterations, 2,
                                                       operations[i],
                                                       pass_operands);
          ++number_tests_generated;
        }
      }
    }
    if (operations[i].min_operands() <= 2
        && operations[i].max_operands() >= 2) {
      // Operation can have 3 operands.
      for (int j = 0; j < number_operands; ++j) {
        for (int k = 0; k < number_operands; ++k) {
          for (int l = 0; l < number_operands; ++l) {
            Operand pass_operands[] = {operands[j], operands[k], operands[l]};
            tests[number_tests_generated] = GenerateTest(number_instructions,
                                                         number_iterations, 3,
                                                         operations[i],
                                                         pass_operands);
            ++number_tests_generated;
          }
        }
      }
    }
  }

  printf("Done.  Generated %d Tests.\n", number_tests_generated);

  // Create Assembly Output Directory
  if (mkdir(kOutputDirectoryName.c_str(), 0777) == -1) {
    perror("Unable to create output directory.\n");
    exit(1);
  }

  printf("Writing %d tests to file system...\n", number_tests_generated);

  // Write Assembly Files
  for (int i = 0; i < number_tests_generated; ++i) {
    FILE *out_file;
    string file_name = kOutputDirectoryName + "/" + tests[i].file_name();
    if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
      perror("Unable to open output file.\n");
      exit(1);
    }
    fprintf(out_file, "%s", tests[i].instruction_body().c_str());
    if (fclose(out_file) != 0) {
      perror("Unable to close output file.\n");
      exit(1);
    }
    tests[i].set_output_complete(true);
  }

  printf("Done.  Wrote %d tests to file system.\n", number_tests_generated);
  printf("Writing index to file system.\n");

  // Write Index File
  FILE *out_file;
  file_name = kOutputDirectoryName + "/" + kIndexFileName;
  if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
    perror("Unable to open index file.\n");
    exit(1);
  }
  for (int i = 0; i < number_tests_generated; ++i) {
    fprintf(out_file, "%s\n", tests[i].file_name().c_str());
  }
  if (fclose(out_file) != 0) {
    perror("Unable to close index file.\n");
    exit(1);
  }

  printf("Done.  All operations completed.\n");

  return(0);
}

// This function returns the integer value of a command line argument, given the
// actual argument and anticipated argument flag
int ParseCommandLineInt(const string arg, const string flag) {
  string str = arg;
  str = str.substr(flag.length());
  return atoi(str.c_str());
}

// This function determines the output directory named based on the current
// date and time of the system clock.  Due to some undefined behavior with the
// asctime function, the last character needed to be truncated, and all ' 's and
// ':'s were replaced with '_'s.
string GetOutputDirectoryName() {
  time_t raw_time;
  string result;
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
int CountUncommentedLines(string file_name, char comment_char) {
  int result = 0;
  FILE *file_in;
  char line[kMaxBufferSize];

  if ((file_in = fopen(file_name.c_str(), "r")) == 0) {
    perror("Unable to open operations data file.\n");
    exit(1);
  }
  while (fgets(line, sizeof(line), file_in) != NULL) {
    if (line[0] != comment_char) {
      ++result;
    }
  }
  if (fclose(file_in) != 0) {
    perror("Unable to close operations data file.\n");
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
// If the sanity check passes, the function will return fully provisioned object
// of type Operation, otherwise it will return an Operand object with a
// NULL ("") operation_name_ member.
Operation GenerateOperation(char data[], const char delimiter[]) {
  // The resulting operation object
  Operation result;

  // String array to hold tokenized data from a single line of the operations
  // data file.
  string args[kArgumentsInOperationDataFile];
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
    return result;
  }
  // Check Argument 1: Operation Status
  // Value of "test" will result in sanity check passing.  Other values will
  // cause failure.  "ignore" values also cause failure because the object
  // isn't needed anyway.
  if (args[1] == "") {
    return result;
  } else if (args[1].compare("test") != 0) {
    return result;
  }
  // Check Argument 2: Minimum Number of Operands
  if (args[2] == "") {
    return result;
  }
  min_operands = atoi(args[2].c_str());
  if (min_operands < kAbsoluteMinimumOperands ||
      min_operands > kAbsoluteMaximumOperands) {
    return result;
  }
  // Check Argument 3: Maximum Number of Operands
  if (args[3] == "") {
    return result;
  }
  max_operands = atoi(args[3].c_str());
  if (max_operands < min_operands ||
      max_operands > kAbsoluteMaximumOperands) {
    return result;
  }
  // All sanity checks passed.
  // Add data from file to Operation Object
  result.set_operation_name(args[0]);
  result.set_min_operands(min_operands);
  result.set_max_operands(max_operands);
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
// If the sanity check passes, the function will return fully provisioned object
// of type Operand, otherwise it will return an Operand object with a NULL ("")
// operand_value_ member.
Operand GenerateOperand(char data[], const char delimiter[]) {
  // The resulting operand object
  Operand result;

  // String array to hold tokenized data from a single line of the operations
  // data file.
  string args[kArgumentsInOperandDataFile];
  // Min/Max Operands Value

  // Populate the string array by tokenizing the line.
  // First argument
  args[0] = strtok(data, delimiter);
  // Following Arguments
  for (int i = 1; i < kArgumentsInOperandDataFile; ++i) {
    args[i] = strtok(NULL, delimiter);
  }

  // Check Argument 0: Operand Value
  if (args[0] == "") {
    return result;
  }
  // Check Argument 1: Operand Status
  // Value of "test" will result in sanity check passing.  Other values will
  // cause failure.  "ignore" values also cause failure because the object
  // isn't needed anyway.
  if (args[1] == "") {
    return result;
  } else if (args[1].compare("test") != 0) {
    return result;
  }
  // Check Argument 2: Operand Type
  if (args[2] == "") {
    return result;
  }

  // All sanity checks passed.
  // Add data from file to Operation Object
  result.set_operand_value(args[0]);
  result.set_operand_type(args[2]);
  return result;
}

Assembly GenerateBaselineTest(int number_instructions, int number_iterations) {
  Assembly result;
  // Set Baseline Data Members
  result.set_instruction_name("");
  result.set_addressing_mode("");
  result.set_file_name(kBaselineFileName);

  // Append the instructions to the Baseline Test
  result.append_instructions(GetBodyPrefix());
  result.append_instructions(GetBodyMain(result, number_instructions,
                                           number_iterations));
  result.append_instructions(GetBodySuffix());
  result.set_generation_complete(true);

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

// Generates an Assembly Object
Assembly GenerateTest(int number_instructions, int number_iterations,
                      int number_operands, Operation& operation,
                      Operand operands[]) {
  Assembly result;
  string addressing_mode = "";

  // Set Assembly data members
  result.set_instruction_name(operation.operation_name());
  result.set_operation(operation);
  result.set_operands(operands, number_operands);

  // Add some string formatting tricks to fix null character bug

  for (int i = 0; i < number_operands; i++) {
    addressing_mode += operands[i].operand_type();
    addressing_mode = addressing_mode.substr(0, addressing_mode.length() - 1);
    addressing_mode += "_";
  }
  addressing_mode = addressing_mode.substr(0, addressing_mode.length() - 1);
  result.set_addressing_mode(addressing_mode);

  if (number_operands > 0) {
    result.set_file_name(result.instruction_name() + "_"
                       + result.addressing_mode() + ".s");
  } else {
    result.set_file_name(result.instruction_name() + ".s");
  }

  printf("Generating Test: %s\n", result.file_name().c_str());

  // Append the instructions to the assembly object
  result.append_instructions(GetBodyPrefix());
  result.append_instructions(GetBodyMain(result, number_instructions,
                                           number_iterations));
  result.append_instructions(GetBodySuffix());
  result.set_generation_complete(true);
  return result;
}

string GetBodyPrefix() {
  return "  .file \"test1.c\"\n"
      "  .text\n"
      ".globl main\n"
      "  .type main, @function\n"
      "main:\n"
      ".LFB2:\n"
      "  pushq %rbp\n"
      ".LCFI0:\n"
      "  movq  %rsp, %rbp\n"
      ".LCFI1:\n"
      "  movl  $0, -8(%rbp)\n"
      "  movl  $0, -4(%rbp)\n"
      "  jmp .L2\n"
      ".L3:";
}

string GetBodyMain(Assembly& obj, int number_instructions,
                   int number_iterations) {
  string result;

  // Convert number_iterations to a character array
  char charValue[kMaxBufferSize];
  sprintf(charValue, "%d", number_iterations);

  if (obj.instruction_name() != "") {
    for (int i = 0; i < number_instructions; ++i) {
      result = result + " " + obj.instruction_name();
      if (obj.number_operands() == 0) {
        result = result + "\n";
      } else if (obj.number_operands() == 1) {
        Operand operand1 = *obj.operand(0);
        result = result + "  " + operand1.operand_value() + "\n";
      } else if (obj.number_operands() == 2) {
        Operand operand1 = *obj.operand(0);
        Operand operand2 = *obj.operand(1);
        result = result + "  " + operand1.operand_value() + ", "
        + operand2.operand_value() + "\n";
      } else if (obj.number_operands() == 3) {
        Operand operand1 = *obj.operand(0);
        Operand operand2 = *obj.operand(1);
        Operand operand3 = *obj.operand(2);
        result = result + "  " + operand1.operand_value() + ", "
        + operand2.operand_value() + ", " + operand3.operand_value()
        + "\n";
      }
    }
  }
  result = result + "  add  $1, -4(%rbp)\n"
      ".L2:\n"
      "cmpl  $" + charValue + ", -4(%rbp)";
  return result;
}

string GetBodySuffix() {
  return "  jle .L3\n"
      "  leave\n"
      "  ret\n"
      ".LFE2:\n"
      "  .size main, .-main\n"
      "  .section  .eh_frame,\"a\",@progbits\n"
      ".Lframe1:\n"
      "  .long .LECIE1-.LSCIE1\n"
      ".LSCIE1:\n"
      "  .long 0x0\n"
      "  .byte 0x1\n"
      "  .string \"zR\"\n"
      "  .uleb128 0x1\n"
      "  .sleb128 -8\n"
      "  .byte 0x10\n"
      "  .uleb128 0x1\n"
      "  .byte 0x3\n"
      "  .byte 0xc\n"
      "  .uleb128 0x7\n"
      "  .uleb128 0x8\n"
      "  .byte 0x90\n"
      "  .uleb128 0x1\n"
      "  .align 8\n"
      ".LECIE1:\n"
      ".LSFDE1:\n"
      "  .long .LEFDE1-.LASFDE1\n"
      ".LASFDE1:\n"
      "  .long .LASFDE1-.Lframe1\n"
      "  .long .LFB2\n"
      "  .long .LFE2-.LFB2\n"
      "  .uleb128 0x0\n"
      "  .byte 0x4\n"
      "  .long .LCFI0-.LFB2\n"
      "  .byte 0xe\n"
      "  .uleb128 0x10\n"
      "  .byte 0x86\n"
      "  .uleb128 0x2\n"
      "  .byte 0x4\n"
      "  .long .LCFI1-.LCFI0\n"
      "  .byte 0xd\n"
      "  .uleb128 0x6\n"
      "  .align 8\n"
      ".LEFDE1:\n"
      "  .ident  \"GCC: (GNU) 4.2.4 (Ubuntu 4.2.4-1ubuntu3)\"\n"
      "  .section  .note.GNU-stack,\"\",@progbits";
}
