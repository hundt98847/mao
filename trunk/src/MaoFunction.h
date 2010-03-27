//
// Copyright 2010 Google Inc.
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

#ifndef MAOFUNCTION_H_
#define MAOFUNCTION_H_

#include "MaoCFG.h"
#include "MaoEntry.h"
#include "MaoLoops.h"
#include "MaoSection.h"
#include "MaoTypes.h"

// Function class
// A function is defined as a sequence of instructions from a
// label matching a symbol with the type Function to the next function,
// or to the end of the section.
class Function {
 public:
  explicit Function(const std::string &name, const FunctionID id,
                    SubSection *subsection) :
      name_(name), id_(id), first_entry_(NULL), last_entry_(NULL),
      subsection_(subsection), cfg_(NULL), lsg_(NULL) {}

  ~Function() {
    // Deallocate memory.
    set_cfg(NULL);
    set_lsg(NULL);
  }

  void set_first_entry(MaoEntry *entry) { first_entry_ = entry;}
  void set_last_entry(MaoEntry *entry) {
    last_entry_ = entry;
    if (last_entry_->next())
      end_entry_ = last_entry_->next();
    else
      // TODO(rhundt): insert dummy node, but for now:
      end_entry_ = last_entry_;
  }
  MaoEntry *first_entry() const { return first_entry_;}
  MaoEntry *last_entry() const { return last_entry_;}
  MaoEntry *end_entry() const { return end_entry_;}

  const std::string name() const {return name_;}
  FunctionID id() const {return id_;}

  int GetNumInstructions() const;
  // TODO(martint): Reconsider iterator name.
  EntryIterator EntryBegin();
  EntryIterator EntryEnd();

  void Print();
  void Print(FILE *out);

  Section *GetSection() {
    MAO_ASSERT(subsection_);
    return subsection_->section();
  }

  SubSection *GetSubSection() {
    MAO_ASSERT(subsection_);
    return subsection_;
  }


 private:
  // These methods are to be used by the respective analyses to cache
  // analysis results.
  CFG *cfg() const {return cfg_;}
  // Sets the CFG (NULL for no one) for a function.
  void set_cfg(CFG *cfg);
  friend CFG *CFG::GetCFG(MaoUnit *mao, Function *function, bool);
  friend CFG *CFG::GetCFGIfExists(const MaoUnit *mao, Function *function);
  friend void CFG::InvalidateCFG(Function *function);

  LoopStructureGraph *lsg() const {return lsg_;}
  // Sets the Loop Structure Graph (NULL for no one) for a function.
  void set_lsg(LoopStructureGraph *lsg);
  friend LoopStructureGraph *LoopStructureGraph::GetLSG(MaoUnit *mao,
                                                        Function *function,
                                                        bool conservative);

  // Name of the function, as given by the function symbol.
  const std::string name_;

  // The uniq id of this function.
  const FunctionID id_;

  // Pointers to the first and last entry of the function.
  MaoEntry *first_entry_;
  MaoEntry *last_entry_;
  MaoEntry *end_entry_;

  // Pointer to subsection that this function starts in.
  SubSection *subsection_;

  /////////////////////////////////////////
  // members populated by analysis passes

  // Pointer to CFG, if one is build for the function.
  CFG *cfg_;
  // Pointer to Loop Structure Graph, if one is build for the function
  LoopStructureGraph *lsg_;
};

// Convenience macros
#define FORALL_FUNC_ENTRY(func, entry) \
  for (MaoEntry *entry = func->first_entry(); \
       entry != func->end_entry(); \
       entry = entry->next())


#endif // MAOFUNCTION_H_
