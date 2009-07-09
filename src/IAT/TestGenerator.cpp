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
#include <string>
#include <sstream>

#include "./TestGenerator.h"
#include "./Assembly.h"

int main(int argc, char* argv[]) {
  int number_iterations = 0;
  int number_instructions = 0;
  // n is being declared here because we are only running one test at this time.
  // There will be a method later that determines this automagically determines
  // and sets this value based on a number of criteria.
  // TODO(caseyburkhardt): Programmatically generate number of tests
  const int n = 1;

  // Command Line Argument Parsing
  // TODO(caseyburkhardt): Add argument for path to op-codes table
  // TODO(caseyburkahrdt): Add argument to display help menu
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

  // Holds the instances of Assembly Objects
  Assembly tests[n];

  // Generate Assembly Objects
  // Only a single assembly object is being generated because only a single
  // test is being generated at this time.  This simply sets up the structure
  // to support multiple tests.  In the future, instructions will be determined
  // from the op-codes table.
  // TODO(caseyburkhardt): Support multiple tests-instructions in op-codes table

  // In the future, tests will also be generated exhaustively for all reasonable
  // addressing modes.  The runner script will deal with compilation errors from
  // unsupported operands to the assembly instructions.
  // TODO(caseyburkhardt): Exhaustively create tests for all addressing modes
  for (int i = 0; i < n; ++i) {
    tests[i].set_instruction_name("addl");
    tests[i].set_addressing_mode("1");
    string file_name = tests[i].instruction_name() + "_" +
        tests[i].addressing_mode() + ".s";
    tests[i].set_file_name(file_name);

    // Append the instructions to the assembly object
    tests[i].append_instructions(GetBodyPrefix());
    tests[i].append_instructions(GetBodyMain(tests[i], number_instructions,
                                             number_iterations));
    tests[i].append_instructions(GetBodySuffix());
    tests[i].set_generation_complete(true);
  }

  // Create Assembly Output Directory
  if (mkdir(kOutputDirectoryName.c_str(), 0777) == -1) {
    perror("Unable to create output directory.\n");
    exit(1);
  }

  // Write Assembly Files
  for (int i = 0; i < n; ++i) {
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

  // Write Index File
  FILE *out_file;
  string file_name = kOutputDirectoryName + "/" + kIndexFileName;
  if ((out_file = fopen(file_name.c_str(), "w")) == 0) {
    perror("Unable to open index file.\n");
    exit(1);
  }
  for (int i = 0; i < n; ++i) {
    fprintf(out_file, "%s\n", tests[i].file_name().c_str());
  }
  if (fclose(out_file) != 0) {
    perror("Unable to close index file.\n");
    exit(1);
  }

  return(0);
}

int ParseCommandLineInt(const string arg, const string flag) {
  string str = arg;
  str = str.substr(flag.length());
  return atoi(str.c_str());
}

// This function determines the output direcotry named based on the current
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
  for (int i = 0; i < result.length(); ++i) {
    if (result[i] == ' ' || result[i] == ':')
      result[i] = '_';
  }
  return result.substr(0, result.length() - 1);
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

string GetBodyMain(Assembly obj, int number_instructions,
                   int number_iterations) {
  string result;

  // Convert number_iterations to a character array
  char charValue[kMaxBufferSize];
  sprintf(charValue, "%d", number_iterations);

  for (int i = 0; i < number_instructions; ++i) {
    result = result + " " + obj.instruction_name() + "  $1, -8(%rbp)" + "\n";
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
