//
// Copyright 2009 and later Google Inc.
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
// along with this program; if not, write to the
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef MAORELAX_H_
#define MAORELAX_H_

#include "MaoUnit.h"
#include "MaoDebug.h"
#include "tc-i386-helper.h"

#include <map>

class MaoRelaxer {
 public:
  typedef std::map<MaoEntry *, int> SizeMap;

  MaoRelaxer() {}
  void Relax(MaoUnit *mao, Section *section, SizeMap *size_map);

 private:
  typedef std::map<struct frag *, MaoEntry *> FragToEntryMap;

  static struct frag *BuildFragments(
      MaoUnit *mao, Section *section, SizeMap *size_map,
      FragToEntryMap *relax_map);

  static struct frag *EndFragmentInstruction(
      InstructionEntry *entry, struct frag *frag, bool new_frag);

  static struct frag *EndFragmentAlign(
      bool code, int alignment, int max, struct frag *frag, bool new_frag);

  static struct frag *EndFragmentLeb128(
      const DirectiveEntry::Operand *value, bool is_signed,
      struct frag *frag, bool new_frag);

  static struct frag *HandleSpace(
      DirectiveEntry *entry, int mult, struct frag *frag,
      bool new_frag, SizeMap *size_map, FragToEntryMap *relax_map);

  static void HandleString(
      DirectiveEntry *entry, int multiplier, bool null_terminate,
      struct frag *frag, SizeMap *size_map);

  static int StringSize(
      DirectiveEntry *entry, int multiplier, bool null_terminate);

  static struct frag *FragVar(
      relax_stateT type, int var, relax_substateT subtype,
      symbolS *symbol, offsetT offset, char *opcode, struct frag *frag,
      bool new_frag);

  static void FragInitOther(struct frag *frag);

  static struct frag *NewFragment() {
    struct frag *frag =
        static_cast<struct frag *>(calloc(1, sizeof(struct frag)));
    MAO_ASSERT(frag);
    return frag;
  }

  static void FreeFragments(struct frag *fragments) {
    struct frag *next;
    for (next = fragments->fr_next; fragments; fragments = next) {
      next = fragments->fr_next;
      free(fragments);
    }
  }
};

// External entry point
void Relax(MaoUnit *mao, Section *section, MaoRelaxer::SizeMap *size_map);

#endif  // MAORELAX_H_
