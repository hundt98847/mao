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

#include "MaoDebug.h"
#include "MaoPasses.h"


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
  while (options_[index].name) {
    if (!strcasecmp(name, options_[index].name))
      return &options_[index];
    ++index;
  }

  MAO_ASSERT_MSG(false, "Invalid options name: %s", name);
  return NULL;
}


bool MaoPass::GetOptionBool(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->bval;
}

const char *MaoPass::GetOptionString(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->cval;
}

int MaoPass::GetOptionInt(const char *name) {
  MaoOption *opt = FindOptionEntry(name);
  return opt->ival;
}

// Dummy pass to mark begin of compilation
class BeginPass : public MaoPass {
  public:
  BeginPass() : MaoPass("BEG", NULL) {
  }

  bool Go() {
    Trace(1, "Begin Compilation");
    return true;
  }
};

// Dummy pass to mark end of compilation
class EndPass : public MaoPass {
  public:
  EndPass() : MaoPass("END", NULL) {
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
