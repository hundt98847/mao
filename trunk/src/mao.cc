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
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <stdio.h>
#include <string.h>

#include "ir.h"
#include "mao.h"
#include "MaoDebug.h"
#include "MaoOptions.h"
#include "MaoUnit.h"
#include "MaoPasses.h"


// Unprocessed flags are passed on to as_main (which is the GNU Assembler
// main function). Everything else is handed to the MAO Option processor
//
static int ProvideHelp(MaoOptions *mao_options) {
  if (mao_options->help()) {
    fprintf(stderr,
            "Mao %s\n",
            MAO_VERSION);
    fprintf(stderr,
            "Usage: mao [-mao:mao-options]* "
            "[regular-assembler-options]* input-file \n"
            "\n\nwith 'mao-options' being one of:\n\n"
            "-h          display this help text\n"
            "-v          verbose\n"
            "-ofname     specify assembler output file\n"
            "\n"
            "PHASE=[phase-options]\n"
            "\n\nwith 'phase-options' being one of:\n\n"
            "db          dump IR before phase\n"
            "da          dump IR before phase\n"
            "db|da[type] dump something before phase,"
            " with 'type' being one of ir, cfg, vcg, mem, time\n"
            );
    exit(0);
  }
  return 0;
}


// AssemblyPass
//
// Pass to dump out the IR in assembly format
//
class AssemblyPass : public MaoPass {
public:
  AssemblyPass(MaoOptions *mao_options, MaoUnit *mao_unit) :
    MaoPass("ASM"), mao_unit_(mao_unit), mao_options_(mao_options) {
  }

  bool Go() {
    if (mao_options_->write_assembly()) {
      Trace(1, "Generate Assembly File: %s",
            mao_options_->assembly_output_file_name());

      FILE *outfile =
        mao_options_->output_is_stdout() ? stdout :
        mao_options_->output_is_stderr() ? stderr :
        fopen(mao_options_->assembly_output_file_name(), "w");
      MAO_ASSERT(outfile);

      fprintf(outfile, "# MaoUnit:\n");
      mao_unit_->PrintMaoUnit(outfile);

      fprintf(outfile, "# Symbol table:\n");
      SymbolTable *symbol_table = mao_unit_->GetSymbolTable();
      MAO_ASSERT(symbol_table);
      symbol_table->Print(outfile);

      if (outfile != stdout)
        fclose(outfile);
    }

    return true;
  }

private:
  MaoUnit    *mao_unit_;
  MaoOptions *mao_options_;
};


// ReadAsmPass
//
// Read/parse the input asm file and generate the IR
//
class ReadInputPass : public MaoPass {
public:
  ReadInputPass(int argc, char *argv[]) :
    MaoPass("READ"), argc_(argc), argv_(argv) {
  }

  bool Go() {
    // Call Gas main routine, building up the IR.
    // Gas will return 0 on success.
    Trace(1, "Read Input");
    return !as_main(argc_, argv_);
  }
private:
  int argc_;
  char **argv_;
};

// CreateCFG
//
// Create a CFG, TODO(rhundt): Make it per function
//
class CreateCFGPass : public MaoPass {
public:
  CreateCFGPass(MaoOptions *mao_options, MaoUnit *mao_unit) :
    MaoPass("CFG"), mao_unit_(mao_unit), mao_options_(mao_options) {
  }

  bool Go() {
    Trace(1, "Build CFG");
    mao_unit_->BuildCFG();

    return true;
  }
private:
  MaoUnit    *mao_unit_;
  MaoOptions *mao_options_;
};


// DumpIrPass
//
// Pass to to dump out the IR in IR format
//
class DumpIrPass : public MaoPass {
public:
  DumpIrPass(MaoOptions *mao_options, MaoUnit *mao_unit) :
    MaoPass("IR"), mao_unit_(mao_unit), mao_options_(mao_options) {
  }
  bool Go() {
    if (mao_options_->write_ir()) {
      Trace(1, "Generate IR Dump File: %s",
            mao_options_->ir_output_file_name());

      FILE *outfile =  fopen(mao_options_->ir_output_file_name(), "w");
      MAO_ASSERT(outfile);
      mao_unit_->PrintIR();
    }
    return true;
  }

private:
  MaoUnit    *mao_unit_;
  MaoOptions *mao_options_;
};


//==================================
// MAO Main Entry
//==================================
int main(int argc, const char *argv[]) {
  MaoOptions mao_options;
  MaoUnit    mao_unit;

  // Parse any mao-specific command line flags (start with -mao:)
  char **new_argv = new char*[argc];
  int    new_argc = 0;

  for (int i = 0; i < argc; i++) {
    if (strncmp(argv[i], "-mao:", 5) == 0)
      mao_options.Parse(const_cast<char*>(&argv[i][5]));
    else
      new_argv[new_argc++] = const_cast<char*>(argv[i]);
  }

  // Static Initialization
  ProvideHelp(&mao_options);
  register_mao_unit(&mao_unit);

  // Make Passes...
  MaoPassManager *mao_pass_man = InitPasses();
  #define PASS(x) mao_pass_man->LinkPass(new x)

  // global init passes
  PASS(ReadInputPass(new_argc, new_argv));

  // function specific passes
  // TODO(rhundt): add loop over functinos
  PASS(CreateCFGPass(&mao_options, &mao_unit));

  // global finalization passes
  PASS(AssemblyPass(&mao_options, &mao_unit));

  // run the passes
  mao_pass_man->Run();
  return 0;
}
