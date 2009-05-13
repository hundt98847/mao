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

#include <string>

#include "MaoUnit.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
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

bool BasicBlock::DirectlyPreceeds(const BasicBlock *basicblock) const {
  // Make sure that if they are linked, both point correctly!
  MAO_ASSERT(basicblock->last_entry()->next() == NULL ||
             basicblock->last_entry()->next() != first_entry() ||
             first_entry()->prev() == basicblock->last_entry());
  return (basicblock->last_entry()->next() != NULL &&
          basicblock->last_entry()->next() == first_entry());
}

bool BasicBlock::DirectlyFollows(const BasicBlock *basicblock) const {
  // Make sure that if they are linked, both point correctly!
  MAO_ASSERT(basicblock->first_entry()->prev() == NULL ||
             basicblock->first_entry()->prev() != last_entry() ||
             last_entry()->next() == basicblock->first_entry());
  return (basicblock->first_entry()->prev() != NULL &&
          basicblock->first_entry()->prev() == last_entry());
}

CFG *CFG::GetCFG(MaoUnit *mao, Function *function) {
  if (function->cfg() == NULL) {
    // Build it!
    CFG *cfg = new CFG(mao);
    CreateCFG(mao, function, cfg);
    function->set_cfg(cfg);
  }
  MAO_ASSERT(function->cfg());
  return function->cfg();
}

CFG *CFG::GetCFGIfExists(const MaoUnit *mao, Function *function) {
  return function->cfg();
}

void CFG::InvalidateCFG(Function *function) {
  // Memory is deallocated in the set_cfg routine.
  function->set_cfg(NULL);
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

  FORALL_CFG_BB(this, it) {
    fprintf(f, "node: { title: \"bb%d\" label: \"bb%d: %s\" %s",
            (*it)->id(), (*it)->id(), (*it)->label(),
            (*it)->id() < 2 ? "color: red" : "");
    fprintf(f, " info1: \"");

    FORALL_BB_ENTRY(it, entry) {
      if ( (*entry)->Type() == MaoEntry::INSTRUCTION ||
           (*entry)->Type() == MaoEntry::DIRECTIVE ||
           (*entry)->Type() == MaoEntry::LABEL) {
        std::string s;
        (*entry)->ToString(&s);
        // Escape strings for vcg.
        std::string::size_type pos = 0;
        while ((pos = s.find("\"", pos)) != std::string::npos) {
          s.replace(pos, 1, "\\\"");
          pos += 2;
        }
        fprintf(f, "%s", s.c_str());
      }
      fprintf(f, "\\n");
    }  // FORALL_BB_ENTRY

    fprintf(f, "\"}\n");
    for (BasicBlock::ConstEdgeIterator eiter = (*it)->BeginOutEdges();
         eiter != (*it)->EndOutEdges(); ++eiter) {
      fprintf(f, "edge: { sourcename: \"bb%d\" targetname: \"bb%d\" }\n",
              (*eiter)->source()->id(), (*eiter)->dest()->id() );
    }
  }  // FORALL_CFG_BB

  fprintf(f, "}\n");

  fclose(f);
}

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(CFG, 3) {
  OPTION_BOOL("callsplit", false, "Split Basic Blocks at call sites"),
  OPTION_BOOL("vcg", false, "Dump VCG after CFG construction"),
  OPTION_BOOL("stat", false, "Collect and print statistics about CFG"),
};


// --------------------------------------------------------------------
CFGBuilder::CFGBuilder(MaoUnit *mao_unit, Function *function, CFG *CFG)
    : MaoFunctionPass("CFG", GetStaticOptionPass("CFG"), mao_unit, function),
      CFG_(CFG), next_id_(0) {
  split_basic_blocks_ = GetOptionBool("callsplit");
  collect_stat_ = GetOptionBool("stat");
  dump_vcg_ = GetOptionBool("vcg");
  if (collect_stat_) {
    // check if a stat object already exists?
    if (unit_->GetStats()->HasStat("CFG")) {
      cfg_stat_ = static_cast<CFGStat *>(unit_->GetStats()->GetStat("CFG"));
    } else {
      cfg_stat_ = new CFGStat();
      unit_->GetStats()->Add("CFG", cfg_stat_);
    }
  }
}



bool CFGBuilder::Go() {
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
      if (entry->Type() == MaoEntry::LABEL) {
        // If the next entry is a .size directive, then the function ends there
        // In that case a basic block should not be created for this label
        MaoEntry *next_entry = entry->next();
        if ((next_entry != NULL) &&
            (next_entry->Type() == MaoEntry::DIRECTIVE) &&
            (next_entry->AsDirective()->op() == DirectiveEntry::SIZE))
          break;

        label = static_cast<LabelEntry *>(entry)->name();
      } else {
        label = MaoUnit::BBNameGen::GetUniqueName();
      }

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
    if (entry->IsInstruction() &&
        entry->AsInstruction()->IsControlTransfer() &&
        !entry->AsInstruction()->IsCall()) {
      typedef const char *Label;
      typedef std::vector<Label> LabelVector;
      LabelVector targets;
      bool va_arg_targets;
      GetTargets(entry, std::back_insert_iterator<LabelVector>(targets),
                 &va_arg_targets);
      for (LabelVector::iterator iter = targets.begin(); iter != targets.end();
           ++iter) {
        Label label = *iter;
        BasicBlock *target = NULL;

        // A NULL label means unknown target
        if (label == NULL) {
          MAO_ASSERT_MSG(false, "Unable to find target for branch.");
        } else {
          target = CFG_->FindBasicBlock(label);
          if (target == NULL) {
            CFG::LabelToBBMap::iterator target_ptr =
                label_to_bb_map_.find(label);
            if (target_ptr == label_to_bb_map_.end()) {
              // Create a new basic block for the label found as a jump target.
              // Here we can check if this label is defined in this basic block
              // Check if the label_entry exists in MAO UNIT
              LabelEntry *target_label = unit_->GetLabelEntry(label);
              if (target_label == NULL) {
                CFG_->IncreaseNumExternalJumps();
              } else {
                // Check if the label exists, but is in another function
                if (unit_->GetFunction(target_label) != function_) {
                  CFG_->IncreaseNumExternalJumps();
                }
              }
              target = CreateBasicBlock(label);
              CFG_->MapBasicBlock(target);
            } else {
              target = target_ptr->second;
              if (strcmp(label, target->label())) {
                bool current_is_target = (target == current);
                target = BreakUpBBAtLabel(target,
                                          unit_->GetLabelEntry(label));
                MAO_ASSERT_MSG(target != NULL,
                               "Unable to find label: %s", label);

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
        if (va_arg_targets) {
          target->set_chained_indirect_jump_target(true);
        }

        // Insert edges
        Link(current, target, false);
        inserted_edges++;
      }
    }

    // Check to see if this entry ends the current basic block
    if (EndsBasicBlock(entry)) {
      create_fall_through = (entry->IsInstruction() &&
                             entry->AsInstruction()->HasFallThrough());
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
  if (current != NULL && last_entry->IsInstruction() &&
      last_entry->AsInstruction()->HasFallThrough())
    Link(current, sink, true);

  if (dump_vcg_) {
    // Use the function name..
    if (strlen(function_->name().c_str()) <= 1024-1-4) {
      char filename[1024];
      sprintf(filename, "%s.vcg", function_->name().c_str());
      CFG_->DumpVCG(filename);
    }
  }

  return true;
}

bool CFGBuilder::BelongsInBasicBlock(const MaoEntry *entry) {
  switch (entry->Type()) {
    case MaoEntry::INSTRUCTION: return true;
    case MaoEntry::LABEL: return true;
    case MaoEntry::DIRECTIVE: return false;
    case MaoEntry::UNDEFINED:
    default:
      MAO_ASSERT(false);
      return false;
  }
}

bool CFGBuilder::EndsBasicBlock(MaoEntry *entry) {
  if (entry->IsInstruction()) {
    InstructionEntry *insn = entry->AsInstruction();
    bool has_fall_through = insn->HasFallThrough();
    bool is_control_transfer = insn->IsControlTransfer();
    bool is_call = insn->IsCall();

    // TODO(nvachhar): Parameterize this to decide whether calls end BBs
    return (is_control_transfer && !is_call) || !has_fall_through;
  }
  return false;
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

// Given a label at the start of a jump-table, return the targets found
// in this jumptable.
CFG::JumpTableTargets *CFG::GetJumptableTargets(LabelEntry *jump_table) {
  JumpTableTargets *targets = NULL;
  // We have two possibilities. 1. it is already processed and stored in the
  // map. 2. We have to manually go through the jump-table, and then add it to
  // our map.
  LabelsToJumpTableTargets::iterator iter =
      labels_to_jumptargets_.find(jump_table);
  if (iter != labels_to_jumptargets_.end()) {
    // Case 1: it has already been processed, and exists in our map
    targets = iter->second;
  } else {
    // Case 2: Parse the jumpt able at the jump_table label

    // Move forward while the entries match the jump-table pattern.
    JumpTableTargets *found_targets = new JumpTableTargets();
    found_targets->clear();
    SectionEntryIterator e_iter = SectionEntryIterator(jump_table);
    // Move past the label to the first entry in the jump table.
    ++e_iter;
    while (true) {
      // The jump table ends at the following conditions
      if (*e_iter == NULL ||            // End of section.
          !(*e_iter)->IsDirective()) {  // No more directives
        break;
      }
      DirectiveEntry *de = (*e_iter)->AsDirective();
      // Does this directive look like its part of a jump table?
      if (!de->IsJumpTableEntry()) {
        break;
      }

      // Now we need to extract the target from the jumptable entry.
      LabelEntry *target_label = de->GetJumpTableTarget();
      found_targets->insert(target_label);
      ++e_iter;
    }

    // Add it to the map and return a pointer.
    labels_to_jumptargets_[jump_table] = found_targets;
    targets = found_targets;
  }
  return targets;
}


// A tail call is here defined as a indirect jump
// direcly after a leave instruction.
bool CFGBuilder::IsTailCall(InstructionEntry *entry) {
  MaoEntry *prev = entry->prev();
  return (entry->IsIndirectJump() &&
          prev &&
          prev->IsInstruction() &&
          prev->AsInstruction()->op() == OP_leave);
}

// Does this entry jump based on a jumptable? If so, return true and return the
// label identifying the jump table in out_label. Otherwise return false and
// return NULL in out_label.
// e.g.:
//  jmp  .L112(,%rax,8)
// or:
//  movq .L112(,%rax,8), %REG
//  jmp  *%REG
bool CFGBuilder::IsTableBasedJump(InstructionEntry *entry,
                                  LabelEntry **out_label) {
  *out_label = NULL;
  if (!entry->IsIndirectJump()) return false;

  const char *label_name = NULL;

  //  jmp  .L112(,%rax,8)
  if (entry->IsMemOperand(0)) {
    // Get the name of the label from the expression
    label_name = entry->GetSymbolnameFromExpression(
        entry->instruction()->op[0].disps);
    if (label_name != NULL) {
      *out_label = unit_->GetLabelEntry(label_name);
      MAO_ASSERT_MSG(*out_label != NULL,
                     "Unable to find label: %s", label_name);
      return true;
    }
  }

  //  movq .L112(,%rax,8), %REG
  //  jmp  *%REG
  if (entry->IsIndirectJump() && entry->IsRegisterOperand(0)) {
    // Get the name of the label from the expression
    MaoEntry *prev = entry->prev();
    InstructionEntry *prev_inst = (prev && prev->IsInstruction())?
        prev->AsInstruction():NULL;
     if (entry->IsRegisterOperand(0) &&
        prev_inst &&
        prev_inst->IsOpMov() &&    // Is previous instruction a move?
        prev_inst->NumOperands() == 2 &&   // target register matches the jump
        prev_inst->IsRegisterOperand(1) &&  // register?
        prev_inst->IsMemOperand(0) &&
        prev_inst->GetRegisterOperand(1) == entry->GetRegisterOperand(0)) {
       // Now get the label from the expression, if we found any!
       label_name = prev_inst->GetSymbolnameFromExpression(
           prev_inst->instruction()->op[0].disps);
       if (label_name) {
         *out_label = unit_->GetLabelEntry(label_name);
         MAO_ASSERT_MSG(*out_label != NULL,
                        "Unable to find label: %s", label_name);
         return true;
       }
     }
  }
  return false;
}


// Does this entry jump based on a vaarg style jump?
// e.g.:
// Loop for va_arg patterns!
//      jmp     *%REG
// <optional label>:
//      movaps  <xmm register>, IMM(%rax)
//      movaps  <xmm register>, IMM(%rax)
//      movaps  <xmm register>, IMM(%rax)
//      ....
// If so, return true and put the movaps instruction in the pattern variable.
bool CFGBuilder::IsVaargBasedJump(InstructionEntry *entry,
                                  std::vector<MaoEntry *> *pattern) {
  pattern->clear();
  if (!entry->IsIndirectJump()) return false;
  if (!entry->IsRegisterOperand(0)) return false;

  // Possible Vaarg based jump found. Check for xmm based move instructions
  MaoEntry *e = entry->next();
  if (e && e->IsLabel())
    e = e->next();
  while (e &&
         e->IsInstruction() &&
         e->AsInstruction()->op() == OP_movaps) {
    pattern->push_back(e);
    e = e->next();
  }
  return pattern->size() > 1;
}



// va_arg_targets is an output variable that signals if the targets
// are part of a va_arg style pattern and the target basic blocks
// need to be flagged.
template <class OutputIterator>
void CFGBuilder::GetTargets(MaoEntry *entry, OutputIterator iter,
                            bool *va_arg_targets) {
  *va_arg_targets = false;

  bool processed = false;
  MAO_ASSERT(entry->Type() == MaoEntry::INSTRUCTION);
  InstructionEntry *insn_entry = static_cast<InstructionEntry *>(entry);

  if (collect_stat_ && insn_entry->IsIndirectJump()) {
    cfg_stat_->FoundIndirectJump();
  }

  // Is this a "normal" direct branch instruction.
  // TODO(martint): Should we care about direct tail-calls here?
  if (!processed) {
    if (!insn_entry->IsCall() && !insn_entry->IsReturn() &&
        !insn_entry->IsIndirectJump()) {
      // Direct branch instructions
      *iter++ = insn_entry->GetTarget();
      processed = true;
      if (collect_stat_) cfg_stat_->FoundDirectJump();
    }
  }

  // Is this a tail call?
  if (!processed && IsTailCall(insn_entry)) {
    if (collect_stat_) cfg_stat_->FoundTailCall();
    // No edges added in this case.
    processed = true;
  }

  // Pattern one: Look for jump tables
  LabelEntry *label_entry;
  if (!processed && IsTableBasedJump(insn_entry, &label_entry)) {
    //  label_entry points to the table, if it is identified.
    MAO_ASSERT(label_entry != NULL);
    // Given the start of the jump-table, get the list of possible targets
    // in this jump table.
    CFG::JumpTableTargets *targets = CFG_->GetJumptableTargets(label_entry);
    // Iterate over the targets and put them in the output iterator
    for (CFG::JumpTableTargets::const_iterator t_iter = targets->begin();
         t_iter != targets->end();
         ++t_iter) {
      *iter++ = (*t_iter)->name();
      processed = true;
    }
    if (collect_stat_ && processed) cfg_stat_->FoundJumpTablePattern();
  }

  // Pattern two: Look for va_arg patterns!
  std::vector<MaoEntry *> pattern;
  if (!processed && IsVaargBasedJump(insn_entry, &pattern)) {
    *va_arg_targets = true;
    // Pattern now holds all the possible targets
    for (std::vector<MaoEntry *>::iterator p_iter = pattern.begin();
        p_iter != pattern.end();
        ++p_iter) {
      // Add all our outputs, and create new labels if necessary
      if ((*p_iter)->prev() &&
          (*p_iter)->prev()->IsLabel()) {
        *iter++ = (*p_iter)->prev()->AsLabel()->name();
      } else {
        // Add a label before p_iter if necessary
        LabelEntry *l = unit_->CreateLabel(
            MaoUnit::BBNameGen::GetUniqueName());
        l->set_from_assembly(false);
        (*p_iter)->LinkBefore(l);
        *iter++ = l->name();
      }
      processed = true;
    }
    if (collect_stat_ && processed) cfg_stat_->FoundVaargPattern();
  }

  if (insn_entry->IsIndirectJump() && !processed) {
    CFG_->IncreaseNumExternalJumps();
    Trace(2, "Unable to find targets for indirect jump.");
  }
}


int BasicBlock::NumEntries() {
  int num = 0;
  MaoEntry *first = first_entry();
  MaoEntry *last  = last_entry();
  while (first) {
    ++num;
    if (first == last) break;
    first = first->next();
  }
  return num;
}

InstructionEntry *BasicBlock::GetFirstInstruction() {
  MaoEntry *e = first_entry_;
  while (e) {
    if (e->IsInstruction()) return e->AsInstruction();
    if (e == last_entry_)
      return NULL;
    e = e->next();
  }
  return NULL;
}
void BasicBlock::Print(FILE *f, MaoEntry *last) {
  MaoEntry *e = first_entry_;
  if (!last)
    last = last_entry_;

  while (e != last) {
    e->PrintEntry(f);
    e = e->next();
  }
  e->PrintEntry(f);
}

void BasicBlock::Print(FILE *f, MaoEntry *from, MaoEntry *to) {
  while (from != to) {
    from->PrintEntry(f);
    from = from->next();
  }
  to->PrintEntry(f);
}

void CreateCFG(MaoUnit *mao_unit, Function *function, CFG *cfg) {
  CFGBuilder builder(mao_unit, function, cfg);
  builder.Go();
}

void InitCFG() {
  RegisterStaticOptionPass("CFG", new MaoOptionMap);
}
