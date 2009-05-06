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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <algorithm>

#include "MaoDebug.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoRelax.h"

class PassDebugAction : public MaoDebugAction {
 public:
  explicit PassDebugAction(const char *pass_name) :
    pass_name_(pass_name) {}

  void set_pass_name(const char *name) { pass_name_ = name; }

  virtual void Invoke(FILE *output) {
    fprintf(output, "***   Last pass:  %s\n", pass_name_);
  }

 private:
  const char *pass_name_;
};

static PassDebugAction *pass_debug_action = NULL;

static std::map<MaoOption *, MaoPass *> option_to_pass_map;

MaoPass *FindPass(MaoOption *arr) {
  MAO_ASSERT(arr);

  std::map<MaoOption *, MaoPass *>::iterator it = option_to_pass_map.find(arr);
  if (it == option_to_pass_map.end())
    return NULL;
  return (*it).second;
}


MaoPass::MaoPass(const char *name, MaoOptions *mao_options,
                 MaoOption *options, bool enabled, const CFG *cfg) :
  name_(name), options_(options), enabled_(enabled),
  tracing_level_(mao_options ? (mao_options->verbose() ? 3 : 0) : 0),
  trace_file_(stderr), mao_options_(mao_options), timed_(false),
  cfg_(cfg),
  db_vcg_(false), db_cfg_(false), da_vcg_(false), da_cfg_(false) {
  MAO_ASSERT_MSG(options,
                 "Option array is required for pass constuction "
                 "(pass: %s)", name);
  MAO_ASSERT_MSG(mao_options,
                 "Options Mgr required for each pass "
                 "(pass: %s)", name);
  option_to_pass_map[options_] = this;
  if (mao_options_)
    mao_options_->Reparse();
  if (!pass_debug_action)
    pass_debug_action = new PassDebugAction(name);
  else
    pass_debug_action->set_pass_name(name);

  if (db_cfg_ && cfg_)
    cfg_->Print(stderr);
  if (db_vcg_ && cfg_) {
    char buff[1024];
    sprintf(buff, "dump.db.%s.vcg", name_);
    cfg_->DumpVCG(buff);
  }

  if (enabled_)
    Trace(3, "begin");
}

MaoPass::~MaoPass() {
  option_to_pass_map[options_] = NULL;
  if (timed_)
    TimerStop();
  if (da_cfg_ && cfg_)
    cfg()->Print(stderr);
  if (da_vcg_ && cfg_) {
    char buff[1024];
    sprintf(buff, "dump.da.%s.vcg", name_);
    cfg_->DumpVCG(buff);
  }
  if (enabled_)
    Trace(3, "end");
}


void MaoPass::TimerStart() {
  if (mao_options_)
    mao_options_->TimerStart(name_);
}

void MaoPass::TimerStop() {
  if (mao_options_)
    mao_options_->TimerStop(name_);
}

void MaoPass::Trace(unsigned int level, const char *fmt, ...) const {
  if (level > tracing_level()) return;

  fprintf(stderr, "[%s]\t", name());

  va_list argList;
  va_start(argList, fmt);
  vfprintf(stderr, fmt, argList);
  va_end(argList);

  fprintf(stderr, "\n");
  fflush(stderr);
}

void MaoPass::TraceC(unsigned int level, const char *fmt, ...) const {
  if (level > tracing_level()) return;

  fprintf(stderr, "[%s]\t", name());

  va_list argList;
  va_start(argList, fmt);
  vfprintf(stderr, fmt, argList);
  va_end(argList);
  fflush(stderr);
}


MaoOption *MaoPass::FindOptionEntry(const char *name) {
  MAO_ASSERT(options_);

  int index = 0;
  while (options_[index].name()) {
    if (!strcasecmp(name, options_[index].name()))
      return &options_[index];
    ++index;
  }

  MAO_ASSERT_MSG(false, "Invalid options name: %s", name);
  return NULL;
}


bool MaoPass::GetOptionBool(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->bval_;
}

const char *MaoPass::GetOptionString(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->cval_;
}

int MaoPass::GetOptionInt(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->ival_;
}

void MaoPass::set_db(const char *str) {
  if (!strcasecmp(str, "cfg"))
    db_cfg_ = true;
  else
  if (!strcasecmp(str, "vcg"))
    db_vcg_ = true;
}

void MaoPass::set_da(const char *str) {
  if (!strcasecmp(str, "cfg"))
    da_cfg_ = true;
  else
  if (!strcasecmp(str, "vcg"))
    da_vcg_ = true;
}

MAO_OPTIONS_DEFINE(BEG, 0) {
};

// Dummy pass to mark begin of compilation
class BeginPass : public MaoPass {
  public:
  explicit BeginPass(MaoOptions *opts) : MaoPass("BEG", opts, MAO_OPTIONS(BEG),
                                                 true) {
  }

  bool Go() {
    Trace(1, "Begin Compilation");
    return true;
  }
};

MAO_OPTIONS_DEFINE(END, 0) {
};

// Dummy pass to mark end of compilation
class EndPass : public MaoPass {
  public:
  explicit EndPass(MaoOptions *opts) : MaoPass("END", opts, MAO_OPTIONS(END),
                                               true) {
  }

  bool Go() {
    Trace(1, "End Compilation");
    return true;
  }
};


MAO_OPTIONS_DEFINE(READ, 0) {
};

// ReadAsmPass
//
// Read/parse the input asm file and generate the IR
//
class ReadInputPass : public MaoPass {
 public:
  ReadInputPass(int argc, const char *argv[], MaoUnit *mao_unit)
      : MaoPass("READ", mao_unit->mao_options(), MAO_OPTIONS(READ), true) {
    set_timed();
    // Use gas to parse input file.
    MAO_ASSERT(!as_main(argc, const_cast<char**>(argv)));
    mao_unit->FindFunctions();
  }
};

MAO_OPTIONS_DEFINE(ASM, 0) {
};

// AssemblyPass
//
// Pass to dump out the IR in assembly format
//
AssemblyPass::AssemblyPass(MaoOptions *mao_options, MaoUnit *mao_unit)
    : MaoPass("ASM", mao_options, MAO_OPTIONS(ASM), true), mao_unit_(mao_unit),
      mao_options_(mao_options) {}

bool AssemblyPass::Go() {
  if (mao_options_->write_assembly()) {
    Trace(1, "Generate Assembly File: %s",
          mao_options_->assembly_output_file_name());

    FILE *outfile =
      mao_options_->output_is_stdout() ? stdout :
      mao_options_->output_is_stderr() ? stderr :
      fopen(mao_options_->assembly_output_file_name(), "w");
    MAO_ASSERT(outfile);

    // Print out the code that makes sure that the symbol
    // table is in the original order
    PrintAsmSymbolHeader(outfile);

    fprintf(outfile, "# MaoUnit:\n");
    mao_unit_->PrintMaoUnit(outfile);

    if (outfile != stdout)
      fclose(outfile);
  }
  return true;
}



// Static single instance of pass manager
static MaoPassManager  mao_pass_man;

// Assemble base pass ordering
MaoPassManager *InitPasses(MaoOptions *opts) {
  mao_pass_man.AddNewPass(new BeginPass(opts));
  mao_pass_man.AddNewPass(new EndPass(opts));

  return &mao_pass_man;
}



// TODO(martint): Finish this function and remove the redundant
// directives from the IR (.type, .comm, .globl, ..)
void AssemblyPass::PrintAsmSymbolHeader(FILE *out) {
  // List of sections:
  //   List of symbols in sections

  // List of global symbols
  // List of comm symbols

  // List of .type directives

  return;
}


class SourceDebugAction : public MaoDebugAction {
 public:
  SourceDebugAction() {}

  virtual void Invoke(FILE *output) {
    char *filename;
    unsigned int linep;
    as_where(&filename, &linep);

    if (filename) {
      fprintf(output, "***   Processing: %s, line: %d\n", filename, linep);
    }
  }
};


void ReadInput(int argc, const char *argv[], MaoUnit *mao_unit) {
  SourceDebugAction spos;
  ReadInputPass reader(argc, argv, mao_unit);
}


// DumpIrPass

MAO_OPTIONS_DEFINE(IR, 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to dump IR to."),
};

//
// Pass to to dump out the IR in text format.
//
DumpIrPass::DumpIrPass(MaoUnit *mao_unit)
    : MaoPass("IR", mao_unit->mao_options(), MAO_OPTIONS(IR), false),
      mao_unit_(mao_unit) {
}

bool DumpIrPass::Go() {
  const char *ir_output_filename = GetOptionString("o");

  Trace(1, "Generate IR Dump File: %s", ir_output_filename);
  FILE *outfile = fopen(ir_output_filename, "w");

  MAO_ASSERT_MSG(outfile, "Unable to open %s for writing\n",
                 ir_output_filename);
  mao_unit_->PrintIR(outfile, true, true, true, true);
  fclose(outfile);
  return true;
}


// DumpSymbolTablePass

MAO_OPTIONS_DEFINE(SYMBOLTABLE, 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to dump symboltable to."),
};

//
// Pass to to dump out the symbol table in text format.
//
DumpSymbolTablePass::DumpSymbolTablePass(MaoUnit *mao_unit)
    : MaoPass("SYMBOLTABLE", mao_unit->mao_options(),
              MAO_OPTIONS(SYMBOLTABLE), false),
      mao_unit_(mao_unit) {
}

bool DumpSymbolTablePass::Go() {
  const char *symboltable_output_filename = GetOptionString("o");

  Trace(1, "Generate Symboltable Dump File: %s", symboltable_output_filename);
  FILE *outfile = fopen(symboltable_output_filename, "w");

  MAO_ASSERT_MSG(outfile, "Unable to open %s for writing\n",
                 symboltable_output_filename);
  fprintf(outfile, "# Symbol table:\n");
  mao_unit_->GetSymbolTable()->Print(outfile);
  fclose(outfile);
  return true;
}

// TestCFGPass

MAO_OPTIONS_DEFINE(TESTCFG, 0) {
};

//
// A pass that runs the CFG, even though no pass needs it.
// Useful for debugging.
//
TestCFGPass::TestCFGPass(MaoUnit *mao_unit, Function *function)
    : MaoPass("TESTCFG", mao_unit->mao_options(), MAO_OPTIONS(TESTCFG), false) {
  if (enabled()) {
    CFG::GetCFG(mao_unit, function);
  }
}

// TestRelaxPass

MAO_OPTIONS_DEFINE(TESTRELAX, 0) {
};

//
// A pass that runs the Relax, even though no pass needs it.
// Useful for debugging.
//
TestRelaxPass::TestRelaxPass(MaoUnit *mao_unit, Function *function)
    : MaoPass("TESTRELAX", mao_unit->mao_options(),
              MAO_OPTIONS(TESTRELAX), false) {
  if (enabled()) {
    MaoRelaxer::InvalidateSizeMap(function->GetSection());
    MaoRelaxer::GetSizeMap(mao_unit, function->GetSection());
  }
}
