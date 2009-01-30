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


#include <stdio.h>

#include "MaoUnit.h"
#include "MaoDebug.h"
#include "tc-i386-helper.h"

struct frag *BuildFragments(MaoUnit *mao, Section *section) {
  struct frag **frag, *frag_head = NULL;

  for (SectionEntryIterator iter = section->EntryBegin(mao);
       iter != section->EntryEnd(mao); ++iter) {
    MaoUnitEntryBase *entry = *iter;
    switch (entry->Type()) {
      case MaoUnitEntryBase::INSTRUCTION: {
        X86InstructionSizeHelper size_helper(
            static_cast<InstructionEntry*>(entry)->
            instruction()->instruction());
        entry->PrintEntry(stdout);
        printf("Size: %d\n", size_helper.SizeOfInstruction());
        break;
      }
      case MaoUnitEntryBase::DIRECTIVE: {
        DirectiveEntry *directive = static_cast<DirectiveEntry*>(entry);
        printf("%s %s\n", directive->key().c_str(), directive->value().c_str());
        break;
      }
      case MaoUnitEntryBase::LABEL:
      case MaoUnitEntryBase::DEBUG:
        /* Nothing to do */
        break;
      case MaoUnitEntryBase::UNDEFINED:
      default:
        MAO_ASSERT(0);
    }
  }

  return frag_head;
}
