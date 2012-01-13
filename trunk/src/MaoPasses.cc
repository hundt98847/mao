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

#include <algorithm>
#include <map>
#include <string>

#include "Mao.h"

// MaoAction
//
MaoAction::MaoAction(const char *name, MaoOptionMap *options, MaoUnit *unit)
    : name_(name), options_(options), trace_file_(stderr), unit_(unit) {
  MAO_ASSERT_MSG(options,
                 "Option map is required for action constuction "
                 "(action: %s)", name);

  tracing_level_ = GetOptionInt("trace");
  da_vcg_ = GetOptionBool("da[vcg]");
  db_vcg_ = GetOptionBool("db[vcg]");
  da_cfg_ = GetOptionBool("da[cfg]");
  db_cfg_ = GetOptionBool("db[cfg]");

  // Populate the function_apply_to_funcs_
  MAO_ASSERT(GetOptionString("apply_to_funcs") != NULL);
  MaoUtil::Tokenize(GetOptionString("apply_to_funcs"),
                    function_filter_,
                    ",");
}

MaoAction::~MaoAction() { }

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

void MaoAction::TraceReplace(unsigned int level,
                             InstructionEntry *before,
                             InstructionEntry *after) {
  TraceC(level, "*** Replaced: ");
  if (tracing_level() >= level) before->PrintEntry(stderr);
  TraceC(level, "*** With    : ");
  if (tracing_level() >= level) after->PrintEntry(stderr);
}

MaoOptionValue MaoAction::FindOptionEntry(const char *name) {
  MAO_ASSERT(options_);
  MaoOptionMap::const_iterator iter = options_->find(std::string(name));
  MAO_ASSERT_MSG(iter != options_->end(), "Invalid options name: %s", name);
  return iter->second;
}

bool MaoAction::GetOptionBool(const char *name) {
  MaoOptionValue value = FindOptionEntry(name);
  return value.bval_;
}

const char *MaoAction::GetOptionString(const char *name) {
  MaoOptionValue value = FindOptionEntry(name);
  return value.cval_;
}

int MaoAction::GetOptionInt(const char *name) {
  MaoOptionValue value = FindOptionEntry(name);
  return value.ival_;
}

void MaoAction::TimerStart() {
  unit_->mao_options()->TimerStart(name_);
}

void MaoAction::TimerStop() {
  unit_->mao_options()->TimerStop(name_);
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


// PassDebugAction
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
MaoPass::MaoPass(const char *name, MaoOptionMap *options, MaoUnit *unit)
  : MaoAction(name, options, unit), redundants(NULL)
{}

MaoPass::~MaoPass() { }

bool MaoPass::Run() {
  if (!pass_debug_action)
    pass_debug_action = new PassDebugAction(name());
  else
    pass_debug_action->set_pass_name(name());
  redundants = new std::list<MaoEntry *>();

  int ret = Go();

  // Now delete all the collected redundant instructions.
  for (std::list<MaoEntry *>::iterator it = redundants->begin();
       it != redundants->end(); ++it) {
    unit_->DeleteEntry(*it);
  }
  delete redundants;
  redundants = NULL;
  return ret;
}

// Allow marking of Entries for deletion after exit from Go()
void MaoPass::MarkInsnForDelete(MaoEntry *insn) {
  MAO_ASSERT(redundants);
  redundants->push_back(insn);
}

// MaoFunctionPass
//
MaoFunctionPass::MaoFunctionPass(const char *pname, MaoOptionMap *options,
                                 MaoUnit *unit, Function *function)
    : MaoPass(pname, options, unit), function_(function) { }

MaoFunctionPass::~MaoFunctionPass() { }

bool MaoFunctionPass::FunctionMatchFilter(const Function *function) const {
  // A match is found, if the function name exists in the list, _or_ no
  // filter was given (empty set).
  if (function_filter_.size() == 0 ||
      function_filter_.find(function->name()) != function_filter_.end()) {
    return true;
  } else {
    return false;
  }
}

bool MaoFunctionPass::Run() {
  // The run is considered successful, even if a filter blocked
  // this particular function.
  bool success = true;

  // Apply the pass if no filter is given, or if the filter
  // match the current function.
  if (FunctionMatchFilter(function_)) {
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

    success = MaoPass::Run();

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
  }
  return success;
}


// Small Passes
//

// ReadInputPass
//
// Read/parse the input asm file and generate the IR
//
MAO_DEFINE_OPTIONS(READ, "Reads the input assembly file", 1) {
  OPTION_BOOL("create_anonymous", false, "Create anonymous functions for "
              "instructions that are not part of regular functions."),
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

ReadInputPass::ReadInputPass(int argc, const char *argv[],
                             MaoOptionMap *options, MaoUnit *mao_unit)
    : MaoPass("READ", options, mao_unit),
      argc_(argc), argv_(argv) { }

bool ReadInputPass::Go() {
  // This line is not a nop.  The constructor links this to the list
  // of debug actions.
  SourceDebugAction spos;

  bool create_anonymous = GetOptionString("create_anonymous");

  // Use gas to parse input file.
  MAO_ASSERT(!as_main(argc_, const_cast<char**>(argv_)));
  unit_->FindFunctions(create_anonymous);
  // Now GAS have parsed it arguments, and we can find out the default
  // architecture and operand size.
  unit_->SetDefaultArch();
  return true;
}

// AssemblyPass
//
// Pass to dump out the IR in assembly format
//
MAO_DEFINE_OPTIONS(ASM, "Writes assembly output to file", 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to output assembly to."),
};

AssemblyPass::AssemblyPass(MaoOptionMap *options, MaoUnit *mao_unit)
    : MaoPass("ASM", options, mao_unit) { }

bool AssemblyPass::Go() {
  const char *output_file_name = GetOptionString("o");

  Trace(1, "Generate Assembly File: %s", output_file_name);

  FILE *outfile = fopen(output_file_name, "w");
  MAO_ASSERT(outfile);

  unit_->PrintMaoUnit(outfile);

  fclose(outfile);
  return true;
}



// DumpIrPass
//
// Pass to to dump out the IR in text format.
//
MAO_DEFINE_OPTIONS(IR, "Dumps the IR in text format", 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to dump IR to."),
};

DumpIrPass::DumpIrPass(MaoOptionMap *options, MaoUnit *mao_unit)
    : MaoPass("IR", options, mao_unit) { }

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
MAO_DEFINE_OPTIONS(SYMBOLTABLE, "Dumps the symbol table in text format", 1) {
  OPTION_STR("o", "/dev/stdout", "Filename to dump symboltable to."),
};

DumpSymbolTablePass::DumpSymbolTablePass(MaoOptionMap *options,
                                         MaoUnit *mao_unit)
    : MaoPass("SYMBOLTABLE", options, mao_unit) { }

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

// TestPass
//
// A pass that can (optionally) run CFG, LSG, Relaxer. Useful
// for testing
//
MAO_DEFINE_OPTIONS(TEST, "A test pass that can optionally run the CFG, LSG "\
                   "and the relaxer",  3) {
  OPTION_BOOL("cfg", false, "Run CFG pass (note that CFG runs automatically "
              "in the Relaxer and the LSG pass.)"),
  OPTION_BOOL("lsg", true, "Run LSG pass."),
  OPTION_BOOL("relax", true, "Run Relaxer pass."),
};

TestPass::TestPass(MaoOptionMap *options, MaoUnit *mao_unit,
                   Function *function)
  : MaoFunctionPass("TEST", options, mao_unit, function),
    cfg_(GetOptionBool("cfg")),
    lsg_(GetOptionBool("lsg")),
    relax_(GetOptionBool("relax")) {
}

bool TestPass::Go() {
  Trace(3,
        "Running TEST on function \"%s\" with options cfg=%d lsg=%d relax=%d",
        function_->name().c_str(), cfg_, lsg_, relax_);

  if (cfg_)
    CFG::GetCFG(unit_, function_);
  if (lsg_)
    LoopStructureGraph::GetLSG(unit_, function_);
  if (relax_)
    MaoRelaxer::GetSizeMap(unit_, function_->GetSection());

  return true;
}
REGISTER_FUNC_PASS("TEST", TestPass)

// MaoFunctionPassManager
//
// A pass to run function passes on all functions in the unit.
//
MAO_DEFINE_OPTIONS(PASSMAN, "A uber-pass that runs function passes on all "\
                   "functions in a file", 0) {
};

MaoFunctionPassManager::MaoFunctionPassManager(MaoOptionMap *options,
                                               MaoUnit *unit)
    : MaoPass("PASSMAN", options, unit) { }

bool MaoFunctionPassManager::Go() {
  // Run passes on functions.
  for (MaoUnit::ConstFunctionIterator func_iter = unit_->ConstFunctionBegin();
       func_iter != unit_->ConstFunctionEnd(); ++func_iter) {
    Function *function = *func_iter;
    for (std::list<MaoFunctionPassManager::ConfiguredPass>::iterator pass_iter =
             pass_list_.begin();
         pass_iter != pass_list_.end(); ++pass_iter) {
      PassCreator creator = pass_iter->first;
      MaoOptionMap *options = pass_iter->second;
      MaoFunctionPass *pass = creator(options, unit_, function);
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
void InitPasses() {
  // Static Option Passes
  RegisterStaticOptionPass("READ", new MaoOptionMap);
  InitCFG();
  InitRelax();
  InitLoops();
}

// Code to maintain the set of available passes
static RegisteredStaticOptionPassMap registered_static_option_passes;

RegisteredFunctionPassesMap& GetRegisteredFunctionPasses() {
  static RegisteredFunctionPassesMap *map =
      new RegisteredFunctionPassesMap();
  return *map;
}

RegisteredUnitPassesMap& GetRegisteredUnitPasses() {
  static RegisteredUnitPassesMap *map =
      new RegisteredUnitPassesMap();
  return *map;
}

void RegisterUnitPass(const char *name,
                      MaoPassManager::PassCreator creator) {
  RegisteredUnitPassesMap &map = GetRegisteredUnitPasses();
  map[name] = creator;
}

void RegisterFunctionPass(const char *name,
                          MaoFunctionPassManager::PassCreator creator) {
  RegisteredFunctionPassesMap &map = GetRegisteredFunctionPasses();
  map[name] = creator;
}

void RegisterStaticOptionPass(const char *name, MaoOptionMap *options) {
  registered_static_option_passes[name] = options;
}

MaoPassManager::PassCreator GetUnitPass(const char *name) {
  RegisteredUnitPassesMap &map = GetRegisteredUnitPasses();
  RegisteredUnitPassesMap::iterator iter =
      map.find(name);
  if (iter == map.end())
    return NULL;
  return iter->second;
}

MaoFunctionPassManager::PassCreator GetFunctionPass(const char *name) {
  RegisteredFunctionPassesMap &map = GetRegisteredFunctionPasses();
  RegisteredFunctionPassesMap::iterator iter =
      map.find(name);
  if (iter == map.end())
    return NULL;
  return iter->second;
}

MaoOptionMap *GetStaticOptionPass(const char *name) {
  RegisteredStaticOptionPassMap::iterator iter =
      registered_static_option_passes.find(name);
  if (iter == registered_static_option_passes.end())
    return NULL;
  return iter->second;
}

const RegisteredStaticOptionPassMap &GetStaticOptionPasses() {
  return registered_static_option_passes;
}

REGISTER_UNIT_PASS("ASM", AssemblyPass)
REGISTER_UNIT_PASS("IR", DumpIrPass)
REGISTER_UNIT_PASS("SYMBOLTABLE", DumpSymbolTablePass)
