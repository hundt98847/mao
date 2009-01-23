//
// Copyright 2008 Google Inc.
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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <iostream>

#include <stdio.h>
#include <string.h>

#include "ir.h"
#include "mao.h"
#include "MaoDebug.h"
#include "MaoOptions.h"
#include "MaoUnit.h"


void print_command_line_arguments(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
    printf("Arg %d: %s\n", i, argv[i]);
  }
}


// Remove the argument index (index_to_remove) from argv and decreases argc
void remove_arg(int *argc, char *argv[], int index_to_remove) {
  MAO_ASSERT(index_to_remove < *argc);
  MAO_ASSERT(index_to_remove >= 0);
  for (int i = index_to_remove; i < *argc; i++) {
    if ((i+1) < *argc) {
      argv[i] = argv[i+1];
    }
  }
  (*argc)--;
}


// Unprocessed flags are passed on to as_main (which is the GNU Assembler
// main function). Currently, this function handles the flags mao_o and mao_v
int process_command_line_arguments(int *argc, char *argv[],
                                   MaoOptions *in_out_options) {
  bool v_flag = false;
  bool o_flag = false;
  bool ir_flag = false;
  char *o_value = NULL;
  char *ir_value = NULL;
  int current_flag = 0;

  while (current_flag < *argc) {
    // check for matches
    if (strcmp(argv[current_flag], "-mao_v") == 0) {
      // we found a match
      remove_arg(argc, argv, current_flag);
      v_flag = true;
      continue;
    }
    if (strcmp(argv[current_flag], "-mao_o") == 0) {
      // make sure there is (atleast) one more argument
      MAO_ASSERT((current_flag+1) < *argc);
      o_value = argv[current_flag+1];
      // we found a match
      remove_arg(argc, argv, current_flag);
      remove_arg(argc, argv, current_flag);
      o_flag = true;
      continue;
    }
    if (strcmp(argv[current_flag], "-mao_ir") == 0) {
      // make sure there is (atleast) one more argument
      MAO_ASSERT((current_flag+1) < *argc);
      ir_value = argv[current_flag+1];
      // we found a match
      remove_arg(argc, argv, current_flag);
      remove_arg(argc, argv, current_flag);
      ir_flag = true;
      continue;
    }
    current_flag++;
  }

  if (o_flag) {
    in_out_options->set_assembly_output_file_name(o_value);
  }
  if (ir_flag) {
    in_out_options->set_ir_output_file_name(ir_value);
  }

  if (v_flag) {
    fprintf(stderr, "Mao version %s\n", MAO_VERSION);
    fprintf(stderr, "Usage: mao [-mao_o FILE] [-mao_ir FILE] [-mao_v]\n");
    fprintf(stderr,
      "  -mao_o FILE       Prints output to FILE.\n");
    fprintf(stderr,
      "  -mao_ir FILE      Prints the IR to FILE\n");
    fprintf(stderr,
      "  -mao_v            Prints version and usage info, then exits\n");
    exit(0);
  }
  return 0;
}

void ProcessIR(MaoUnit *maounit, MaoOptions *mao_options) {
  if (mao_options->write_assembly()) {
    FILE *outfile =  fopen(mao_options->assembly_output_file_name(), "w");
    MAO_ASSERT(outfile);
    fprintf(outfile, "# MaoUnit:\n");
    maounit->PrintMaoUnit(outfile);
    fprintf(outfile, "# Symbol table:\n");
    SymbolTable *symbol_table = maounit->GetSymbolTable();
    MAO_ASSERT(symbol_table);
    symbol_table->Print(outfile);
    fprintf(outfile, "# Done\n");
    if (outfile != stdout) {
      fclose(outfile);
    }
  }
  if (mao_options->write_ir()) {
    FILE *outfile =  fopen(mao_options->ir_output_file_name(), "w");
    MAO_ASSERT(outfile);
    maounit->PrintIR();
  }
}

int main(int argc, char *argv[]) {
  MaoOptions mao_options;
  MaoUnit maounit;

  MAO_TRACE("Starting");

  // Allows linking function to access our MaoUnit.
  register_mao_unit(&maounit);
  // Parse any mao-specific command line flags
  process_command_line_arguments(&argc, argv, &mao_options);
  int ret_val = as_main(argc, argv);

  ProcessIR(&maounit, &mao_options);

  maounit.BuildCFG();
//   maounit.PrintCFG();
//   maounit.PrintIR();

  MAO_TRACE("Finished");
  return ret_val;
}
