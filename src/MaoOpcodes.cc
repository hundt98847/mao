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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#include "gen-opcodes-table.h"
#include "MaoDebug.h"
#include "MaoUtil.h"

#include <map>

static bool initialized = false;
static std::map<const char*, MaoOpcode, ltstr> name_hash;

MaoOpcode GetOpcode(const char *opcode) {
  if (!initialized) {
    int index = 0;
    while (MaoOpcodeTable[index].name) {
      name_hash[MaoOpcodeTable[index].name] = MaoOpcodeTable[index].opcode;
      ++index;
    }
    initialized = true;
  }
  MAO_ASSERT(OP_invalid != name_hash[opcode]);
  return name_hash[opcode];
}
