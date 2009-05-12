//
// Copyright 2009 Google Inc.
//
// This program is free software; you can redistribute it and/or to
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
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"

extern "C" {
  void Init();
}

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(TESTPLUG, 1) {
  OPTION_STR("prefix", "plugin", "Prefix for messages"),
};

class TestPlugin : public MaoFunctionPass {
 public:
  TestPlugin(MaoUnit *mao, Function *function)
      : MaoFunctionPass("TESTPLUG", mao->mao_options(), MAO_OPTIONS(TESTPLUG),
                        mao, function) { }

  bool Go() {
    printf("%s: %s\n", GetOptionString("prefix"), function_->name().c_str());
    return true;
  }
};


// External Entry Point
//
void Init() {
  RegisterFunctionPass(
      "TESTPLUG",
      MaoFunctionPassManager::GenericPassCreator<TestPlugin>);
}
