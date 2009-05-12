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

//Option Management
//
static std::map<MaoOption *, MaoAction *> option_to_pass_map;

MaoAction *FindPass(MaoOption *arr) {
  MAO_ASSERT(arr);

  std::map<MaoOption *, MaoAction *>::iterator it = option_to_pass_map.find(arr);
  if (it == option_to_pass_map.end())
    return NULL;
  return (*it).second;
}


// MaoAction
//
MaoAction::MaoAction(const char *name, MaoOptions *mao_options,
                     MaoOption *options)
    : name_(name), options_(options),
      tracing_level_(mao_options ? (mao_options->verbose() ? 3 : 0) : 0),
      trace_file_(stderr), mao_options_(mao_options),
      db_vcg_(false), db_cfg_(false), da_vcg_(false), da_cfg_(false) {
  MAO_ASSERT_MSG(options,
                 "Option array is required for action constuction "
                 "(action: %s)", name);
  MAO_ASSERT_MSG(mao_options,
                 "Options manager required for each action "
                 "(action: %s)", name);
  option_to_pass_map[options_] = this;

  if (mao_options_)
    mao_options_->Reparse();
}

MaoAction::~MaoAction() {
  option_to_pass_map[options_] = NULL;
}

void MaoAction::Trace(unsigned int level, const char *fmt, ...) const {
  if (level > tracing_level()) return;

  fprintf(stderr, "[%s]\t", name());

  va_list argList;
  va_start(argList, fmt);
  vfprintf(stderr, fmt, argList);
  va_end(argList);

  fprintf(stderr, "\n");
  fflush(stderr);
}

void MaoAction::TraceC(unsigned int level, const char *fmt, ...) const {
  if (level > tracing_level()) return;

  fprintf(stderr, "[%s]\t", name());

  va_list argList;
  va_start(argList, fmt);
  vfprintf(stderr, fmt, argList);
  va_end(argList);
  fflush(stderr);
}

MaoOption *MaoAction::FindOptionEntry(const char *name) {
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


bool MaoAction::GetOptionBool(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->bval_;
}

const char *MaoAction::GetOptionString(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->cval_;
}

int MaoAction::GetOptionInt(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->ival_;
}

void MaoAction::TimerStart() {
  if (mao_options_)
    mao_options_->TimerStart(name_);
}

void MaoAction::TimerStop() {
  if (mao_options_)
    mao_options_->TimerStop(name_);
}

void MaoAction::set_db(const char *str) {
  if (!strcasecmp(str, "cfg"))
    db_cfg_ = true;
  else
  if (!strcasecmp(str, "vcg"))
    db_vcg_ = true;
}

void MaoAction::set_da(const char *str) {
  if (!strcasecmp(str, "cfg"))
    da_cfg_ = true;
  else
  if (!strcasecmp(str, "vcg"))
    da_vcg_ = true;
}


//PassDebugAction
//
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


// MaoPass
//
MaoPass::MaoPass(const char *name, MaoOptions *mao_options, MaoOption *options,
                 MaoUnit *unit)
    : MaoAction(name, mao_options, options), unit_(unit) { }

MaoPass::~MaoPass() { }

bool MaoPass::Run() {
  if (!pass_debug_action)
    pass_debug_action = new PassDebugAction(name());
  else
    pass_debug_action->set_pass_name(name());
  return Go();
}


// MaoFunctionPass
//
MaoFunctionPass::MaoFunctionPass(const char *pname, MaoOptions *mao_options,
                                 MaoOption *options, MaoUnit *unit,
                                 Function *function)
    : MaoPass(pname, mao_options, options, unit), function_(function) { }

MaoFunctionPass::~MaoFunctionPass() { }

bool MaoFunctionPass::Run() {
  if (db_cfg_) {
    CFG *cfg = CFG::GetCFG(unit_, function_);
    cfg->Print(stderr);
  }

  if (db_vcg_) {
    char buff[13 + strlen(name()) + 1 + strlen(function_->name().c_str())];
    sprintf(buff, "dump.db.%s.%s.vcg", name(), function_->name().c_str());
    CFG *cfg = CFG::GetCFG(unit_, function_);
    cfg->DumpVCG(buff);
  }

  bool success = MaoPass::Run();

  if (da_cfg_) {
    CFG *cfg = CFG::GetCFG(unit_, function_);
    cfg->Print(stderr);
  }

  if (da_vcg_) {
    char buff[13 + strlen(name()) + 1 + strlen(function_->name().c_str())];
    sprintf(buff, "dump.da.%s.%s.vcg", name(), function_->name().c_str());
    CFG *cfg = CFG::GetCFG(unit_, function_);
    cfg->DumpVCG(buff);
  }

  return success;
}


// Small Passes
//

// ReadInputPass
//
// Read/parse the input asm file and generate the IR
//
MAO_OPTIONS_DEFINE(READ, 0) {
};

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

ReadInputPass::ReadInputPass(int argc, const char *argv[], MaoUnit *mao_unit)
    : MaoPass("READ", mao_unit->mao_options(), MAO_OPTIONS(READ), mao_unit),
      argc_(argc), argv_(argv) { }

bool ReadInputPass::Go() {
  // This line is not a nop.  The constructor links this to the list
  // of debug actions.
  SourceDebugAction spos;

  // Use gas to parse input file.
  MAO_ASSERT(!as_main(argc_, const_cast<char**>(argv_)));
  unit_->FindFunctions();
  return true;
}

// AssemblyPass
//
// Pass to dump out the IR in assembly format
//
MAO_OPTIONS_DEFINE(ASM, 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to output assembly to."),
};

AssemblyPass::AssemblyPass(MaoUnit *mao_unit)
    : MaoPass("ASM", mao_unit->mao_options(), MAO_OPTIONS(ASM), mao_unit) { }

bool AssemblyPass::Go() {
  const char *output_file_name = GetOptionString("o");

  Trace(1, "Generate Assembly File: %s", output_file_name);

  FILE *outfile = fopen(output_file_name, "w");
  MAO_ASSERT(outfile);

  // Print out the code that makes sure that the symbol
  // table is in the original order
  PrintAsmSymbolHeader(outfile);

  fprintf(outfile, "# MaoUnit:\n");
  unit_->PrintMaoUnit(outfile);

fclose(outfile);
  return true;
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


// DumpIrPass
//
// Pass to to dump out the IR in text format.
//
MAO_OPTIONS_DEFINE(IR, 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to dump IR to."),
};

DumpIrPass::DumpIrPass(MaoUnit *mao_unit)
    : MaoPass("IR", mao_unit->mao_options(), MAO_OPTIONS(IR), mao_unit) { }

bool DumpIrPass::Go() {
  const char *ir_output_filename = GetOptionString("o");

  Trace(1, "Generate IR Dump File: %s", ir_output_filename);
  FILE *outfile = fopen(ir_output_filename, "w");

  MAO_ASSERT_MSG(outfile, "Unable to open %s for writing\n",
                 ir_output_filename);
  unit_->PrintIR(outfile, true, true, true, true);
  fclose(outfile);
  return true;
}


// DumpSymbolTablePass
//
// Pass to to dump out the symbol table in text format.
//
MAO_OPTIONS_DEFINE(SYMBOLTABLE, 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to dump symboltable to."),
};

DumpSymbolTablePass::DumpSymbolTablePass(MaoUnit *mao_unit)
    : MaoPass("SYMBOLTABLE", mao_unit->mao_options(),
              MAO_OPTIONS(SYMBOLTABLE), mao_unit) { }

bool DumpSymbolTablePass::Go() {
  const char *symboltable_output_filename = GetOptionString("o");

  Trace(1, "Generate Symboltable Dump File: %s", symboltable_output_filename);
  FILE *outfile = fopen(symboltable_output_filename, "w");

  MAO_ASSERT_MSG(outfile, "Unable to open %s for writing\n",
                 symboltable_output_filename);
  fprintf(outfile, "# Symbol table:\n");
  unit_->GetSymbolTable()->Print(outfile);
  fclose(outfile);
  return true;
}

// TestCFGPass
//
// A pass that runs the CFG, even though no pass needs it.
// Useful for debugging.
//
MAO_OPTIONS_DEFINE(TESTCFG, 0) {
};

TestCFGPass::TestCFGPass(MaoUnit *mao_unit, Function *function)
    : MaoFunctionPass("TESTCFG", mao_unit->mao_options(), MAO_OPTIONS(TESTCFG),
                      mao_unit, function) { }

bool TestCFGPass::Go() {
  CFG::GetCFG(unit_, function_);
  return true;
}

// TestRelaxPass
//
// A pass that runs the Relax, even though no pass needs it.
// Useful for debugging.
//
MAO_OPTIONS_DEFINE(TESTRELAX, 0) {
};

TestRelaxPass::TestRelaxPass(MaoUnit *mao_unit, Function *function)
    : MaoFunctionPass("TESTRELAX", mao_unit->mao_options(),
                      MAO_OPTIONS(TESTRELAX), mao_unit, function) { }

bool TestRelaxPass::Go() {
  //MaoRelaxer::InvalidateSizeMap(function_->GetSection());
  MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
  return true;
}


// MaoFunctionPassManager
//
// A pass to run function passes on all passes in the unit.
//
MAO_OPTIONS_DEFINE(PASSMAN, 0) {
};

MaoFunctionPassManager::MaoFunctionPassManager(MaoUnit *unit)
    : MaoPass("PASSMAN", unit->mao_options(), MAO_OPTIONS(PASSMAN), unit) { }

bool MaoFunctionPassManager::Go() {
  // Run passes on functions.
  for (MaoUnit::ConstFunctionIterator func_iter = unit_->ConstFunctionBegin();
       func_iter != unit_->ConstFunctionEnd(); ++func_iter) {
    Function *function = *func_iter;
    for (std::list<PassCreator>::iterator pass_iter = pass_list_.begin();
         pass_iter != pass_list_.end(); ++pass_iter) {
      PassCreator creator = (*pass_iter);
      MaoFunctionPass *pass = creator(unit_, function);
      pass->TimerStart();
      MAO_ASSERT(pass->Run());
      pass->TimerStop();
      delete pass;
    }
  }
  return true;
}

// Other utility methods
//

// Assemble base pass ordering
void InitPasses(MaoOptions *opts) {
  // Unit passes
  InitProfileAnnotation();

  RegisterUnitPass("ASM", MaoPassManager::GenericPassCreator<AssemblyPass>);
  RegisterUnitPass("IR", MaoPassManager::GenericPassCreator<DumpIrPass>);
  RegisterUnitPass("SYMBOLTABLE",
                   MaoPassManager::GenericPassCreator<DumpSymbolTablePass>);


  // Function passes
  InitDCE();
  InitNopKiller();
  InitNopinizer();
  InitZEE();
  InitRedundantTestElimination();
  InitRedundantMemMoveElimination();
  InitMissDispElimination();
  InitLongInstructionSplit();
  InitLoopAlign();
  InitBranchSeparate();
  InitAddAddElimination();
  InitAlignTinyLoops16();

  RegisterFunctionPass(
      "TESTCFG", MaoFunctionPassManager::GenericPassCreator<TestCFGPass>);
  RegisterFunctionPass(
      "TESTRELAX", MaoFunctionPassManager::GenericPassCreator<TestRelaxPass>);
}

// Code to maintain the set of available passes
typedef std::map<const char *,
                 MaoFunctionPassManager::PassCreator,
                 ltstr> RegisteredFunctionPassesMap;
typedef std::map<const char *,
                 MaoPassManager::PassCreator,
                 ltstr> RegisteredUnitPassesMap;
static RegisteredUnitPassesMap     registered_unit_passes;
static RegisteredFunctionPassesMap registered_function_passes;

void RegisterUnitPass(const char *name,
                      MaoPassManager::PassCreator creator) {
  registered_unit_passes[name] = creator;
}

void RegisterFunctionPass(const char *name,
                          MaoFunctionPassManager::PassCreator creator) {
  registered_function_passes[name] = creator;
}

MaoPassManager::PassCreator GetUnitPass(const char *name) {
  RegisteredUnitPassesMap::iterator iter =
      registered_unit_passes.find(name);
  if (iter == registered_unit_passes.end())
    return NULL;
  return iter->second;
}

MaoFunctionPassManager::PassCreator GetFunctionPass(const char *name) {
  RegisteredFunctionPassesMap::iterator iter =
      registered_function_passes.find(name);
  if (iter == registered_function_passes.end())
    return NULL;
  return iter->second;
}
