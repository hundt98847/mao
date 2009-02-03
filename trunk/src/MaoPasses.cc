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

static std::map<MaoOption *, MaoPass *> option_to_pass_map;

MaoPass *FindPass(MaoOption *arr) {
  MAO_ASSERT(arr);

  std::map<MaoOption *, MaoPass *>::iterator it = option_to_pass_map.find(arr);
  if (it == option_to_pass_map.end())
    return NULL;
  return (*it).second;
}


MaoPass::MaoPass(const char *name, MaoOptions *mao_options,
                 MaoOption *options) :
  name_(name), options_(options), enabled_(true),
  tracing_level_(mao_options ? (mao_options->verbose() ? 3 : 0) : 0),
  trace_file_(stderr), mao_options_(mao_options) {
  option_to_pass_map[options_] = this;
  if (mao_options_)
    mao_options_->Reparse();
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
  BeginPass() : MaoPass("BEG", NULL, NULL) {
  }

  bool Go() {
    Trace(1, "Begin Compilation");
    return true;
  }
};

// Dummy pass to mark end of compilation
class EndPass : public MaoPass {
  public:
  EndPass() : MaoPass("END", NULL, NULL) {
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

void ReadInput(int argc, char *argv[]) {
  ReadInputPass reader(argc, argv);
}
