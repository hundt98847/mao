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
  const char **new_argv = new const char*[argc];
  int    new_argc = 0;

  mao_options.Parse(getenv("MAOOPTS"));
  for (int i = 0; i < argc; i++) {
    if (strncmp(argv[i], "-mao:", 5) == 0)
      mao_options.Parse(&argv[i][5]);
    else
      new_argv[new_argc++] = argv[i];
  }

  // Static Initialization
  mao_options.ProvideHelp();
  if (new_argc < 2)
    mao_options.ProvideHelp(true /* always */);
  register_mao_unit(&mao_unit);

  // Make Passes...
  MaoPassManager *mao_pass_man = InitPasses(&mao_options);

  // global init passes
  ReadInput(new_argc, new_argv, &mao_unit);

  // Create a CFG for each function
  for (MaoUnit::ConstFunctionIterator iter = mao_unit.ConstFunctionBegin();
       iter != mao_unit.ConstFunctionEnd();
       ++iter) {
    Function *function = *iter;
    function->set_lsg(PerformLoopRecognition(&mao_unit, function));

    // Optimization passes.
    PerformDeadCodeElimination(&mao_unit, CFG::GetCFG(&mao_unit, function));
    PerformNopKiller(&mao_unit, CFG::GetCFG(&mao_unit, function));
    PerformZeroExtensionElimination(&mao_unit,
                                    CFG::GetCFG(&mao_unit, function));
    PerformRedundantTestElimination(&mao_unit,
                                    CFG::GetCFG(&mao_unit, function));
    PerformRedundantMemMoveElimination(&mao_unit,
                                       CFG::GetCFG(&mao_unit, function));
    PerformMissDispElimination(&mao_unit,
                               CFG::GetCFG(&mao_unit, function));
    DoLoopAlign(&mao_unit, function);
  }

  // global finalization passes
  mao_pass_man->LinkPass(new AssemblyPass(&mao_options, &mao_unit));
  mao_pass_man->LinkPass(new DumpIrPass(&mao_unit));
  mao_pass_man->LinkPass(new DumpSymbolTablePass(&mao_unit));

  // run the passes
  mao_pass_man->Run();

  mao_unit.stat()->Print(stdout);
  if (mao_options.timer_print())
    mao_options.TimerPrint();
  return 0;
}
