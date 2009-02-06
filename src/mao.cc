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
#include "MaoCFG.h"
#include "MaoLoops.h"
#include "MaoRelax.h"


//==================================
// MAO Main Entry
//==================================
int main(int argc, const char *argv[]) {
  MaoOptions mao_options;
  MaoUnit    mao_unit(&mao_options);
  CFG        cfg(&mao_unit);

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
  mao_options.ProvideHelp();
  if (new_argc < 2)
    mao_options.ProvideHelp(true /* always */);
  register_mao_unit(&mao_unit);

  // Make Passes...
  MaoPassManager *mao_pass_man = InitPasses();

  // global init passes
  ReadInput(new_argc, new_argv);

  // for (function iterator....)
  //     TODO(rhundt): add loop over functinos
  CreateCFG(&mao_unit, &cfg);
  PerformLoopRecognition(&mao_unit, &cfg);

  // for (section iterator...)
  //     TODO(nvachhar): add loop over sections
  MaoRelaxer::SizeMap sizes;
  std::pair<bool, Section *> text_pair = mao_unit.FindOrCreateAndFind(".text");
  MAO_ASSERT(!text_pair.first);
  Section *section = text_pair.second;
  Relax(&mao_unit, section, &sizes);
  int section_size = 0;
  for (SectionEntryIterator iter = section->EntryBegin(&mao_unit);
       iter != section->EntryEnd(&mao_unit); ++iter) {
    MaoUnitEntryBase *entry = *iter;
    int size = sizes[entry];
    printf("%04x %02d:\t", section_size, size);
    section_size += size;
    entry->PrintEntry(stdout);
  }
  printf("Size of .text section: %d\n", section_size);

  // global finalization passes
  mao_pass_man->LinkPass(new AssemblyPass(&mao_options, &mao_unit));

  // run the passes
  mao_pass_man->Run();
  return 0;
}
