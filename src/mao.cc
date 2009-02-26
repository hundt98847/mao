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
#include "MaoLoopAlign.h"
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
  ReadInput(new_argc, new_argv, &mao_unit);

  // Create a CFG for each function
  for (MaoUnit::ConstFunctionIterator iter = mao_unit.ConstFunctionBegin();
       iter != mao_unit.ConstFunctionEnd();
       ++iter) {
    Function *function = *iter;
    MAO_ASSERT(function->cfg() == NULL);
    function->set_cfg(new CFG(&mao_unit));
    CreateCFG(&mao_unit, function, function->cfg());
  }

  // Find all the loops for each function
  for (MaoUnit::ConstFunctionIterator iter = mao_unit.ConstFunctionBegin();
       iter != mao_unit.ConstFunctionEnd();
       ++iter) {
    Function *function = *iter;
    MAO_ASSERT(function->cfg() != NULL);
    // Memory for loop structure is allocated in the function.
    function->set_lsg(PerformLoopRecognition(&mao_unit, function->cfg(),
                                             function->name().c_str()));
  }


  // Relax the .text sectino, and add a pointer in each
  // function to the sizemap
  MaoRelaxer::SizeMap sizes;
  Section *section = mao_unit.GetSection(".text");
  MAO_ASSERT(section);
  Relax(&mao_unit, section, &sizes);

  for (MaoUnit::ConstFunctionIterator iter = mao_unit.ConstFunctionBegin();
       iter != mao_unit.ConstFunctionEnd();
       ++iter) {
    Function *function = *iter;
    function->set_sizes(&sizes);
  }


  // Perform the loop alignment optimization on all functions
  for (MaoUnit::ConstFunctionIterator iter = mao_unit.ConstFunctionBegin();
       iter != mao_unit.ConstFunctionEnd();
       ++iter) {
    Function *function = *iter;
    DoLoopAlign(&mao_unit, function);
  }


//   int section_size = 0;
//   for (SectionEntryIterator iter = section->EntryBegin();
//        iter != section->EntryEnd(); ++iter) {
//     MaoEntry *entry = *iter;
//     int size = sizes[entry];
//     printf("%04x %02d:\t", section_size, size);
//     section_size += size;
//     entry->PrintEntry(stdout);
//   }
//   printf("Size of .text section: %d\n", section_size);

  // global finalization passes
  mao_pass_man->LinkPass(new AssemblyPass(&mao_options, &mao_unit));
  mao_pass_man->LinkPass(new DumpIrPass(&mao_unit));
  mao_pass_man->LinkPass(new DumpSymbolTablePass(&mao_unit));

  // run the passes
  mao_pass_man->Run();

  mao_unit.stat()->Print(stdout);
  return 0;
}
