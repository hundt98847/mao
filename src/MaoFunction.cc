//
// Copyright 2010 Google Inc.
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

//
// Class: Function
//

int Function::GetNumInstructions() {
  unsigned num_instrns = 0;
  FORALL_FUNC_ENTRY(this, entry) {
    if ((*entry)->IsInstruction()) {
      ++num_instrns;
    }
  }
  return num_instrns;
}

EntryIterator Function::EntryBegin() {
  return EntryIterator(first_entry());
}

EntryIterator Function::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return EntryIterator(entry);
}


void Function::Print() {
  Print(stdout);
}

void Function::Print(FILE *out) {
  fprintf(out, "Function: %s\n", name().c_str());
  for (EntryIterator iter = EntryBegin();
       iter != EntryEnd();
       ++iter) {
    MaoEntry *entry = *iter;
    entry->PrintEntry(out);
  }
}

void Function::set_cfg(CFG *cfg) {
  // Deallocate any previous CFG.
  if (cfg_ != NULL) {
    delete cfg_;
  }
  cfg_ = cfg;
}

void Function::set_lsg(LoopStructureGraph *lsg) {
  // Deallocate any previous LSG.
  if (lsg_ != NULL) {
    delete lsg_;
  }
  lsg_ = lsg;
}
