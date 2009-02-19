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
MAO_OPTIONS_DEFINE(CFG, 1) {
  OPTION_BOOL("callsplit", false, "Split Basic Blocks at call sites"),
};


// --------------------------------------------------------------------
CFGBuilder::CFGBuilder(MaoUnit *mao_unit, MaoOptions *mao_options,
                       Section *section, CFG *CFG)
  : MaoPass("CFG", mao_unit->mao_options(), MAO_OPTIONS(CFG), true),
    mao_unit_(mao_unit), section_(section), CFG_(CFG), next_id_(0) {
  split_basic_blocks_ = GetOptionBool("callsplit");
}



void CFGBuilder::Build() {
  // These basic blocks are not mapped in the CFG because the labels are fake
  BasicBlock *source = CreateBasicBlock("<SOURCE>");
  BasicBlock *sink = CreateBasicBlock("<SINK>");
  BasicBlock *current, *previous;
  bool create_fall_through;

  // Induction variables for the loop
  previous = source;
  current = NULL;
  create_fall_through = true;

  // Main loop processing the IR entries
  // TODO(nvachhar): This should iterate over a given function
  for (SectionEntryIterator e_iter = section_->EntryBegin();
       e_iter != section_->EntryEnd(); ++e_iter) {
    MaoEntry *entry = *e_iter;

    if (tracing_level() > 2) {
      fprintf(stderr, "CFG: Working on: ");
      entry->PrintEntry(stderr);
    }
    if (!BelongsInBasicBlock(entry))
      continue;

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
        BasicBlock *target;

        // A NULL label means unknown target
        if (label == NULL) {
          // TODO(nvachhar): This should deal with indirect branches
          MAO_ASSERT(false);
        } else {
          target = CFG_->FindBasicBlock(label);
          if (target == NULL) {
            CFG::LabelToBBMap::iterator target_ptr = label_to_bb_map_.find(label);
            if (target_ptr == label_to_bb_map_.end()) {
              target = CreateBasicBlock(label);
              CFG_->MapBasicBlock(target);
            } else {
              target = target_ptr->second;
              if (strcmp(label, target->label())) {
                bool current_is_target = (target == current);
                target = BreakUpBBAtLabel(target,
                                          mao_unit_->GetLabelEntry(label));

                // Update label_to_bb_map_ to reflect the split.
                for (MaoUnit::EntryIterator entry_iter = target->BeginEntries();
                     entry_iter != target->EndEntries(); ++entry_iter) {
                  MaoEntry *temp_entry = *entry_iter;
                  if (temp_entry->Type() == MaoEntry::LABEL) {
                    LabelEntry *temp_label = static_cast<LabelEntry *>(temp_entry);
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
}

#include "MaoRelax.h"

void CreateCFG(MaoUnit *mao_unit, CFG *cfg) {
  Section *section = mao_unit->GetSection(".text");
  if (!section) return;
  CFGBuilder builder(mao_unit, mao_unit->mao_options(), section, cfg);
  builder.Build();
}
