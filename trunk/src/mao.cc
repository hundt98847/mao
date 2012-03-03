//
// Copyright 2012 Google Inc.
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

#include "Mao.h"

//==================================
// MAO Main Entry
//==================================
int main(int argc, const char *argv[]) {
  InitPasses();

  MaoOptions mao_options;

  // Parse any mao-specific command line flags (start with --mao=)
  const char **new_argv = new const char*[argc];
  int    new_argc = 0;
  int    gas_help_requested = false;

  mao_options.Parse(argv[0], getenv("MAOOPTS"));
  for (int i = 0; i < argc; i++) {
    if (strncmp(argv[i], "--help", 6) == 0) {
      gas_help_requested = true;
    }

    if (strncmp(argv[i], "--mao=", 6) == 0)
      mao_options.Parse(argv[0], &argv[i][6]);
    else
      new_argv[new_argc++] = argv[i];
  }

  // If gas help is requested, make sure the MAO options are printed first.
  if (gas_help_requested && !mao_options.help()) {
    mao_options.ProvideHelp(false, true);
  }

  // Static Initialization
  mao_options.ProvideHelp(!gas_help_requested);

  if (gas_help_requested) {
    fprintf(stdout, "\nAssembler specific options:\n\n");
  }

  InitRegisters();

  MaoUnit mao_unit(&mao_options);
  RegisterMaoUnit(&mao_unit);

  MaoPassManager mao_pass_man(&mao_unit);
  mao_pass_man.LinkPass(new ReadInputPass(new_argc, new_argv,
                                          GetStaticOptionPass("READ"),
                                          &mao_unit));

  // Reparse the arguments now that all the dynamic passes have been
  // loaded.  This will initialize the pass manager with the desired
  // passes for execution.
  mao_options.Reparse(&mao_unit, &mao_pass_man);

  // run the passes
  mao_pass_man.Run();

  mao_unit.GetStats()->Print(stdout);
  if (mao_options.timer_print())
    mao_options.TimerPrint();
  return 0;
}
