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
#include "MaoCFG.h"
#include "MaoOptions.h"



// Class: BasicBlock

SectionEntryIterator BasicBlock::EntryBegin() {
  return SectionEntryIterator(first_entry());
}

SectionEntryIterator BasicBlock::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return SectionEntryIterator(entry);
}

void BasicBlock::AddEntry(MaoEntry *entry) {
  if (NULL == first_entry_) {
    first_entry_ = entry;
  }
  last_entry_ = entry;
}




void CFG::Print(FILE *out) const {
  // TODO(nvachhar): Emit a text representation of the CFG
}

void CFG::DumpVCG(const char *fname) const {
  MAO_ASSERT(fname);
  FILE *f = fopen(fname, "w");
  if (!f) return;

  fprintf(f,
          "graph: { title: \"CFG\" \n"
          "splines: yes\n"
          "layoutalgorithm: dfs\n"
          "\n"
          "node.color: lightyellow\n"
          "node.textcolor: blue\n"
          "edge.arrowsize: 15\n");

  for (BBVector::const_iterator biter = basic_blocks_.begin();
       biter != basic_blocks_.end(); ++biter) {
    fprintf(f, "node: { title: \"bb%d\" label: \"bb%d: %s\" %s",
            (*biter)->id(), (*biter)->id(), (*biter)->label(),
            (*biter)->id() < 2 ? "color: red" : "");

#if 0  // TODO(nvachhar): Fix this to properly escape characters
    fprintf(f, "info1: \"");
    for (MaoUnit::EntryIterator it = (*biter)->BeginEntries();
         it != (*biter)->EndEntries(); ++it) {
      if ( (*it)->Type() == MaoEntry::INSTRUCTION ||
           (*it)->Type() == MaoEntry::LABEL)
        (*it)->PrintEntry(f);
      else
        fprintf(f, "...\n");
    }
#endif
    fprintf(f, "}\n");

    for (BasicBlock::ConstEdgeIterator eiter = (*biter)->BeginOutEdges();
         eiter != (*biter)->EndOutEdges(); ++eiter) {
      fprintf(f, "edge: { sourcename: \"bb%d\" targetname: \"bb%d\" }\n",
              (*eiter)->source()->id(), (*eiter)->dest()->id() );
    }
  }

  fprintf(f, "}\n");

  fclose(f);
}

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(CFG, 2) {
  OPTION_BOOL("callsplit", false, "Split Basic Blocks at call sites"),
  OPTION_BOOL("vcg", false, "Dump VCG after CFG construction"),
};


// --------------------------------------------------------------------
CFGBuilder::CFGBuilder(MaoUnit *mao_unit, MaoOptions *mao_options,
                       Function *function, CFG *CFG)
  : MaoPass("CFG", mao_unit->mao_options(), MAO_OPTIONS(CFG), true),
    mao_unit_(mao_unit), function_(function), CFG_(CFG),
    next_id_(0) {
  split_basic_blocks_ = GetOptionBool("callsplit");
  dump_vcg_ = GetOptionBool("vcg");
}



void CFGBuilder::Build() {
  // These basic blocks are not mapped in the CFG because the labels are fake
  BasicBlock *source = CreateBasicBlock("<SOURCE>");
  BasicBlock *sink = CreateBasicBlock("<SINK>");
  BasicBlock *current, *previous;
  MaoEntry *last_entry;
  bool create_fall_through;

  // Induction variables for the loop
  previous = source;
  current = NULL;
  create_fall_through = true;

  // Main loop processing the IR entries
  for (SectionEntryIterator e_iter = function_->EntryBegin();
       e_iter != function_->EntryEnd(); ++e_iter) {
    MaoEntry *entry = *e_iter;

    if (tracing_level() > 2) {
      fprintf(stderr, "CFG: Working on: ");
      entry->PrintEntry(stderr);
    }
    if (!BelongsInBasicBlock(entry))
      continue;

    // Update the last entry in the basic block
    last_entry = entry;

    // If the current entry starts a new basic block, end the previous one
    if (current && entry->Type() == MaoEntry::LABEL &&
        CFG_->FindBasicBlock(static_cast<LabelEntry*>(entry)->name()) != NULL) {
      create_fall_through = true;
      previous = current;
      current = NULL;
    }

    // If there is no current basic block, find or create one
    if (!current) {
      const char *label;
      if (entry->Type() == MaoEntry::LABEL)
        label = static_cast<LabelEntry *>(entry)->name();
      else
        label = MaoUnit::BBNameGen::GetUniqueName();

      if ((current = CFG_->FindBasicBlock(label)) == NULL) {
        current = CreateBasicBlock(label);
        CFG_->MapBasicBlock(current);
      }

      if (create_fall_through)
        Link(previous, current, true);
      create_fall_through = false;
    }

    // Add the entry to the current basic block
    current->AddEntry(entry);

    // If the current entry is a label, update the map of labels
    if (entry->Type() == MaoEntry::LABEL)
      label_to_bb_map_[static_cast<LabelEntry *>(entry)->name()] = current;

    // Check to see if this operation creates out edges
    int inserted_edges = 0;
    if (entry->IsControlTransfer() && !entry->IsCall()) {
      typedef const char *Label;
      typedef std::vector<Label> LabelVector;
      LabelVector targets;
      GetTargets(entry, std::back_insert_iterator<LabelVector>(targets));
      for (LabelVector::iterator iter = targets.begin(); iter != targets.end();
           ++iter) {
        Label label = *iter;
        BasicBlock *target = NULL;

        // A NULL label means unknown target
        if (label == NULL) {
          // TODO(nvachhar): This should deal with indirect branches
          MAO_ASSERT(false);
        } else {
          target = CFG_->FindBasicBlock(label);
          if (target == NULL) {
            CFG::LabelToBBMap::iterator target_ptr =
                label_to_bb_map_.find(label);
            if (target_ptr == label_to_bb_map_.end()) {
              target = CreateBasicBlock(label);
              CFG_->MapBasicBlock(target);
            } else {
              target = target_ptr->second;
              if (strcmp(label, target->label())) {
                bool current_is_target = (target == current);
                target = BreakUpBBAtLabel(target,
                                          mao_unit_->GetLabelEntry(label));

                for (SectionEntryIterator entry_iter = target->EntryBegin();
                     entry_iter != target->EntryEnd(); ++entry_iter) {
                  MaoEntry *temp_entry = *entry_iter;
                  if (temp_entry->Type() == MaoEntry::LABEL) {
                    LabelEntry *temp_label =
                        static_cast<LabelEntry *>(temp_entry);
                    label_to_bb_map_[temp_label->name()] = target;
                  }
                }

                // The new BB may need to become the current BB
                if (current_is_target)
                  current = target;
              }
            }
          }
        }

        // Insert edges
        Link(current, target, false);
        inserted_edges++;
      }
    }

    // Check to see if this entry ends the current basic block
    if (EndsBasicBlock(entry)) {
      create_fall_through = entry->HasFallThrough();
      previous = current;
      current = NULL;

      if (inserted_edges == 0 && !create_fall_through)
        Link(previous, sink, true);
    }
  }

  if (create_fall_through)
    Link(previous, sink, true);

  // Handle the case where a function ends with a basic block that
  // does not end in a jump instruction.
  if (current != NULL && last_entry->HasFallThrough())
    Link(current, sink, true);

  if (dump_vcg_)
    CFG_->DumpVCG("cfg.vcg");
}


BasicBlock *CFGBuilder::BreakUpBBAtLabel(BasicBlock *bb, LabelEntry *label) {
  BasicBlock *new_bb = CreateBasicBlock(label->name());
  CFG_->MapBasicBlock(new_bb);

  // Remap the pointers
  new_bb->set_first_entry(label);
  new_bb->set_last_entry(bb->last_entry());
  bb->set_last_entry(label->prev());

  // Move all the out edges
  for (BasicBlock::EdgeIterator edge_iter = bb->BeginOutEdges();
       edge_iter != bb->EndOutEdges();
       edge_iter = bb->EraseOutEdge(edge_iter)) {
    BasicBlockEdge *edge = *edge_iter;
    edge->set_source(new_bb);
    new_bb->AddOutEdge(edge);
  }

  // Link the two basic blocks with a fall through edge
  Link(bb, new_bb, true);

  return new_bb;
}


#include "MaoRelax.h"

void CreateCFG(MaoUnit *mao_unit, Function *function, CFG *cfg) {
  CFGBuilder builder(mao_unit, mao_unit->mao_options(), function, cfg);
  builder.set_timed();
  builder.Build();
}
