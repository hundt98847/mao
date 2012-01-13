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

#include "Mao.h"

namespace {

PLUGIN_VERSION

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(TESTPLUG, "A test plugin pass", 1) {
  OPTION_STR("prefix", "plugin", "Prefix for messages"),
};

class TestPlugin : public MaoFunctionPass {
 public:
  TestPlugin(MaoOptionMap *options, MaoUnit *mao, Function *function)
      : MaoFunctionPass("TESTPLUG", options, mao, function) { }

  bool Go() {
    printf("%s: %s\n", GetOptionString("prefix"), function_->name().c_str());
    return true;
  }
};

REGISTER_PLUGIN_FUNC_PASS("TESTPLUG", TestPlugin );
}  // namespace
