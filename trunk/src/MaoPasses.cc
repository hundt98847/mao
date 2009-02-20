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
                 MaoOption *options, bool enabled) :
  name_(name), options_(options), enabled_(enabled),
  tracing_level_(mao_options ? (mao_options->verbose() ? 3 : 0) : 0),
  trace_file_(stderr), mao_options_(mao_options) {
  option_to_pass_map[options_] = this;
  if (mao_options_)
    mao_options_->Reparse();
  if (!pass_debug_action)
    pass_debug_action = new PassDebugAction(name);
  else
    pass_debug_action->set_pass_name(name);
  if (enabled_)
    Trace(1, "begin");
}

MaoPass::~MaoPass() {
  option_to_pass_map[options_] = NULL;
  if (enabled_)
    Trace(1, "end");
}


void MaoPass::Trace(unsigned int level, const char *fmt, ...) {
  MAO_ASSERT(level>0);
  if (level > tracing_level()) return;

  fprintf(stderr, "[%s]\t", name());

  va_list argList;
  va_start(argList, fmt);
  vfprintf(stderr, fmt, argList);
  va_end(argList);

  fprintf(stderr, "\n");
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

// Dummy pass to mark begin of compilation
class BeginPass : public MaoPass {
  public:
  BeginPass() : MaoPass("BEG", NULL, NULL, true) {
  }

  bool Go() {
    Trace(1, "Begin Compilation");
    return true;
  }
};

// Dummy pass to mark end of compilation
class EndPass : public MaoPass {
  public:
  EndPass() : MaoPass("END", NULL, NULL, true) {
  }

  bool Go() {
    Trace(1, "End Compilation");
    return true;
  }
};


// Static single instance of pass manager
static MaoPassManager  mao_pass_man;

// Assemble base pass ordering
MaoPassManager *InitPasses() {
  mao_pass_man.AddNewPass(new BeginPass());
  mao_pass_man.AddNewPass(new EndPass());

  return &mao_pass_man;
}


// Source Correlation
extern "C" {
  void as_where(char **namep, unsigned int *linep);
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


void ReadInput(int argc, char *argv[], MaoUnit *mao_unit) {
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
  mao_unit_->PrintIR(outfile, true, true, true);
  fclose(outfile);
  return true;
}
