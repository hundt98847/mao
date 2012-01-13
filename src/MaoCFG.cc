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

#include "Mao.h"

// Class: BasicBlock
EntryIterator BasicBlock::EntryBegin() const {
  return EntryIterator(first_entry());
}

EntryIterator BasicBlock::EntryEnd() const {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return EntryIterator(entry);
}


ReverseEntryIterator BasicBlock::RevEntryBegin() const {
  return ReverseEntryIterator(last_entry());
}

ReverseEntryIterator BasicBlock::RevEntryEnd() const {
  MaoEntry *entry = first_entry();
  if (entry) {
    entry = entry->prev();
  }
  return ReverseEntryIterator(entry);
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

CFG *CFG::GetCFG(MaoUnit *mao, Function *function, bool conservative) {
  // If a CFG is associated with the function, and it was build with the
  // same flags, we can reuse it. Otherwise rebuild it.
  if (function->cfg() == NULL ||
      function->cfg()->conservative() != conservative) {
    // Build it!
    CFG *cfg = new CFG(mao);
    CreateCFG(mao, function, cfg, conservative);
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
MAO_DEFINE_OPTIONS(CFG, "Builds the control flow graph", 4) {
  OPTION_BOOL("callsplit", false, "Split Basic Blocks at call sites"),
  OPTION_BOOL("respect_orig_labels", false, "Create a BB whenever the "
              "input file has a label directive"),
  OPTION_BOOL("vcg", false, "Dump VCG after CFG construction"),
  OPTION_BOOL("collect_stats", false,
              "Collect and print a table with information about direct "
              "and indirect jumps for all processed CFGs."),
};


// --------------------------------------------------------------------
CFGBuilder::CFGBuilder(MaoUnit *mao_unit, Function *function, CFG *CFG,
                       bool conservative)
    : MaoFunctionPass("CFG", GetStaticOptionPass("CFG"), mao_unit, function),
      CFG_(CFG), next_id_(0), cfg_stat_(NULL) {
  MAO_ASSERT(CFG_ != NULL);
  split_basic_blocks_ = GetOptionBool("callsplit");
  respect_orig_labels_= GetOptionBool("respect_orig_labels");
  dump_vcg_ = GetOptionBool("vcg");
  if (conservative)
    respect_orig_labels_ = true;
  if (GetOptionBool("collect_stats")) {
    // check if a stat object already exists?
    if (unit_->GetStats()->HasStat("CFG")) {
      cfg_stat_ = static_cast<CFGStat *>(unit_->GetStats()->GetStat("CFG"));
    } else {
      cfg_stat_ = new CFGStat();
      unit_->GetStats()->Add("CFG", cfg_stat_);
    }
  }
  CFG_->set_conservative(conservative);
}



bool CFGBuilder::Go() {
  // These basic blocks are not mapped in the CFG because the labels are fake
  BasicBlock *source = CreateBasicBlock("<SOURCE>");
  BasicBlock *sink = CreateBasicBlock("<SINK>");
  BasicBlock *current, *previous;
  MaoEntry *last_entry = NULL;
  bool create_fall_through;

  // Induction variables for the loop
  previous = source;
  current = NULL;
  create_fall_through = true;

  // Main loop processing the IR entries
  for (EntryIterator e_iter = function_->EntryBegin();
       e_iter != function_->EntryEnd(); ++e_iter) {
    MaoEntry *entry = *e_iter;

    if (tracing_level() > 2) {
      fprintf(stderr, "CFG: Working on: ");
      entry->PrintEntry(stderr);
    }
    if (!BelongsInBasicBlock(entry)) {
      if (entry->IsDirective()) {
        DirectiveEntry *de = entry->AsDirective();
        if (current != NULL && de->IsDataDirective())
          current->FoundDataDirectives();
      }
      continue;
    }

    // Update the last entry in the basic block
    last_entry = entry;

    // If the current entry starts a new basic block, end the previous one
    if (current && entry->Type() == MaoEntry::LABEL &&
        (respect_orig_labels_ ||
         CFG_->FindBasicBlock(static_cast<LabelEntry*>(entry)->name())
         != NULL)) {
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

                for (EntryIterator entry_iter = target->EntryBegin();
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
bool CFG::GetJumptableTargets(LabelEntry *jump_table,
                              CFG::JumpTableTargets **out_targets) {
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
    EntryIterator e_iter = EntryIterator(jump_table);
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
      // Did we find a valid target?
      if (target_label == NULL) {
        // Nope, deallocate memory and return
        delete found_targets;
        *out_targets = NULL;
        return false;
      }
      found_targets->insert(target_label);
      ++e_iter;
    }

    // Add it to the map and return a pointer.
    labels_to_jumptargets_[jump_table] = found_targets;
    targets = found_targets;
  }
  *out_targets = targets;
  return true;
}


// A tail call is here defined as a indirect jump
// direcly after a leave instruction.
bool CFGBuilder::IsTailCall(InstructionEntry *entry) const {
  MaoEntry *prev = entry->prev();
  return (entry->IsIndirectJump() &&
          prev &&
          prev->IsInstruction() &&
          prev->AsInstruction()->op() == OP_leave);
}

// Identifies the pattern below. If it is found, it returns
// true and puts the label LBL in out_label.
// Assumes that *entry is a jump instruction.
//
//  jmp  .L112(,%rax,8)
bool CFGBuilder::IsTablePattern1(InstructionEntry *entry,
                                 LabelEntry **out_label) const {
  MAO_ASSERT(entry->IsIndirectJump());
  if (entry->NumOperands() == 1 &&
      entry->IsMemOperand(0) &&
      entry->HasDisplacement(0)) {
    const char *label_name = NULL;
    // Get the name of the label from the expression
    label_name = entry->GetSymbolnameFromExpression(
        entry->GetDisplacement(0));
    if (label_name != NULL) {
      *out_label = unit_->GetLabelEntry(label_name);
      MAO_ASSERT_MSG(*out_label != NULL,
                     "Unable to find label: %s", label_name);
      return true;
    }
  }
  return false;
}

// Identifies the pattern below. If it is found, it returns
// true and puts the label LBL in out_label.
//
//  movq .L112(,%rax,8), %REG
//  jmp  *%REG
bool CFGBuilder::IsTablePattern2(InstructionEntry *entry,
                                 LabelEntry **out_label) const {
  if (entry->IsIndirectJump() && entry->IsRegisterOperand(0)) {
    // Get the name of the label from the expression
    MaoEntry *prev = entry->prev();
    InstructionEntry *prev_inst = (prev && prev->IsInstruction())?
        prev->AsInstruction():NULL;
    if (entry->IsRegisterOperand(0) &&
        prev_inst &&
        prev_inst->IsOpMov() &&             // Is previous instruction a move?
        prev_inst->NumOperands() == 2 &&    // target register matches the jump
        prev_inst->IsRegisterOperand(1) &&  // register?
        prev_inst->IsMemOperand(0) &&
        prev_inst->GetRegisterOperand(1) == entry->GetRegisterOperand(0) &&
        prev_inst->HasDisplacement(0)) {
       // Now get the label from the expression, if we found any!
       const char *label_name = NULL;
       label_name = prev_inst->GetSymbolnameFromExpression(
           prev_inst->GetDisplacement(0));
       if (label_name) {
         *out_label = unit_->GetLabelEntry(label_name);
         if (*out_label == NULL) {
           // The label is not defined in this assembly file.
           return false;
         }
         return true;
       }
     }
  }
  return false;
}

// Identifies the pattern below. If it is found, it returns
// true and puts the label LBL in out_label.
//
// This pattern appears in PIC code for x86_64
// leaq LBL(%rip), %R_B         #
// (movl   %R_D, %R_B_small)/(movzbl %R_D, %R_B_small) # inst2 * optional
// movslq (%R_B, %R_I, 4), %R_I # inst1
// addq   %R_B, %R_I            # inst0
// jump *%R_I                   #
bool CFGBuilder::IsTablePattern3(InstructionEntry *entry,
                                 LabelEntry **out_label) const {
  if (entry->IsIndirectJump() && entry->IsRegisterOperand(0)) {
    const reg_entry *r_ri = entry->GetRegisterOperand(0);
    const reg_entry *r_rb = NULL;

    // get the five instructions:
    MaoEntry *prev_iter = entry->prev();
    // insts will be populated with the 5 previous instructions,
    // if they are instructions. Otherwise they will be set to null.
    // This makes it easy to check for the above mentioned pattern.
    InstructionEntry *insts[4] = {NULL, NULL, NULL, NULL};
    bool good_so_far = true;
    for (int i = 0; i < 4; ++i) {
      if (prev_iter && prev_iter->IsInstruction()) {
        insts[i] = prev_iter->AsInstruction();
        prev_iter = prev_iter->prev();
      } else {
        break;
      }
    }
    // Make sure they are all instructions
    good_so_far = good_so_far &&
        insts[0] != NULL &&
        insts[1] != NULL &&
        insts[2] != NULL &&
        insts[3] != NULL;

    // Check inst0
    good_so_far = good_so_far &&
        insts[0]->IsAdd() &&
        insts[0]->NumOperands() == 2 &&
        insts[0]->IsRegisterOperand(0) &&
        insts[0]->IsRegisterOperand(1) &&
        insts[0]->GetRegisterOperand(1) == r_ri;

    if (good_so_far) {
      r_rb = insts[0]->GetRegisterOperand(0);
    }

    // Check inst1
    good_so_far = good_so_far &&
        insts[1]->op() == OP_movslq &&
        insts[1]->NumOperands() == 2 &&
        insts[1]->IsRegisterOperand(1) &&
        insts[1]->GetRegisterOperand(1) == r_ri;

    // check for the optional mov instructino
    int current_inst = 2;
    if (good_so_far) {
      if ((insts[2]->IsOpMov() ||
           insts[2]->op() == OP_movzbl) &&
          insts[2]->NumOperands() == 2 &&
          insts[2]->IsRegisterOperand(1) &&
          insts[2]->GetRegisterOperand(1) != r_rb) {
        current_inst = 3;
      }
    }

    good_so_far = good_so_far &&
        insts[current_inst]->op() == OP_lea &&
        insts[current_inst]->NumOperands() == 2 &&
        insts[current_inst]->IsRegisterOperand(1) &&
        insts[current_inst]->GetRegisterOperand(1) == r_rb;

    if (good_so_far &&
        insts[current_inst]->HasDisplacement(0)) {
      const char *label_name = NULL;
      // get the label from the instruction
      label_name = insts[current_inst]->GetSymbolnameFromExpression(
          insts[current_inst]->GetDisplacement(0));
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

// Identifies the pattern below. If it is found, it returns
// true and puts the label LBL in out_label.
// *
// leaq LBL(%rip), %R_B         # ????
// *
// movslq (%R_B, %R_I, 4), %R_I # inst1
// addq   %R_B, %R_I            # inst0
// jump *%R_I                   #
// - Looks up until the end of the function for the leaq instruction.
// - makes sure that leaq is the only instruction the defines R_B
// - make sure that R_B is not live in. We want to be sure that
//   the leaq is the only way the value can get defined.
bool CFGBuilder::IsTablePattern4(InstructionEntry *entry,
                                 LabelEntry **out_label) const {
  if (entry->IsIndirectJump() && entry->IsRegisterOperand(0)) {
    const reg_entry *r_ri = entry->GetRegisterOperand(0);
    const reg_entry *r_rb = NULL;
    BitString rmask;

    // get the two instructions
    MaoEntry *prev_iter = entry->prev();
    // insts will be populated with the 5 previous instructions,
    // if they are instructions. Otherwise they will be set to null.
    // This makes it easy to check for the above mentioned pattern.
    InstructionEntry *insts[2] = {NULL, NULL};
    for (int i = 0; i < 2; ++i) {
      if (prev_iter && prev_iter->IsInstruction()) {
        insts[i] = prev_iter->AsInstruction();
        prev_iter = prev_iter->prev();
      } else {
        break;
      }
    }

    bool pattern_match =
        insts[0] != NULL &&
        insts[1] != NULL &&
        insts[0]->IsAdd() &&
        insts[0]->NumOperands() == 2 &&
        insts[0]->IsRegisterOperand(0) &&
        insts[0]->IsRegisterOperand(1) &&
        insts[0]->GetRegisterOperand(1) == r_ri;

    if (pattern_match) {
      r_rb = insts[0]->GetRegisterOperand(0);
      // Make sure that r_rb is not an input-register
      // according to the ABI.
      // The AMD64 ABI convertion used on Linux uses the following registers
      // %rdi, %rsi, %rdx, %rcv, %r8 and %r9 (and also their subregisters)
      BitString abi_calling_convention_mask = GetCallingConventionDefMask();
      rmask = GetMaskForRegister(r_rb);
      if (!(abi_calling_convention_mask & rmask).IsNull()) {
        Trace(3, "Found a conflict between input paramter register and r_rb");
        return false;
      }
    }
    // Check inst1
    pattern_match = pattern_match &&
        insts[1]->op() == OP_movslq &&
        insts[1]->NumOperands() == 2 &&
        insts[1]->IsRegisterOperand(1) &&
        insts[1]->GetRegisterOperand(1) == r_ri;

    // Loop through the function to look for any definitions of
    // r_b. If there is one, this is our jump table!

    if (pattern_match) {
      MaoEntry *e_iter;
      // TODO(martint): move through the whole function, not just up.
      e_iter = entry;
      Function *current_function = unit_->GetFunction(entry);
      InstructionEntry *def_inst = NULL;
      int num_def_inst = 0;
      while (e_iter != NULL && current_function == unit_->GetFunction(e_iter)) {
        // Check if instruction is defining register r_b
        if (e_iter->IsInstruction()) {
          InstructionEntry *i_entry = e_iter->AsInstruction();
          BitString imask = GetRegisterDefMask(i_entry);
          if (imask.IsUndef()) {
            return false;
          }
          // Now get the bitmask for register..
          if (!(imask & rmask).IsNull()) {
            // Conflict!
            def_inst = i_entry;
            ++num_def_inst;
          }
        }
        e_iter = e_iter->prev();
      }
      if (num_def_inst == 1) {
        MAO_ASSERT(def_inst != NULL);
        const char *label_name = NULL;
        label_name = def_inst->GetSymbolnameFromExpression(
            def_inst->GetDisplacement(0));
        if (label_name) {
          *out_label = unit_->GetLabelEntry(label_name);
          MAO_ASSERT_MSG(*out_label != NULL,
                         "Unable to find label: %s", label_name);
          return true;
        }
      }
    }
  }
  return false;
}

// Does this entry jump based on a jumptable? If so, return true and return the
// label identifying the jump table in out_label. Otherwise return false and
// return NULL in out_label.
bool CFGBuilder::IsTableBasedJump(InstructionEntry *entry,
                                  LabelEntry **out_label) const {
  *out_label = NULL;
  if (!entry->IsIndirectJump()) return false;

  if (IsTablePattern1(entry, out_label) ||
      IsTablePattern2(entry, out_label) ||
      IsTablePattern3(entry, out_label) ||
      IsTablePattern4(entry, out_label)) {
    return true;
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
                                  std::vector<MaoEntry *> *pattern) const {
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
  // The instruction following the sequence of movaps is also a possible
  // target of the indirect jump
  while (e && !e->IsInstruction())
    e = e->next();

  if (e)
    pattern->push_back(e);



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

  if (cfg_stat_ && insn_entry->IsIndirectJump()) {
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
      if (cfg_stat_) cfg_stat_->FoundDirectJump();
    }
  }

  // Is this a tail call?
  if (!processed && IsTailCall(insn_entry)) {
    if (cfg_stat_) cfg_stat_->FoundTailCall();
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
    CFG::JumpTableTargets *targets;
    if (CFG_->GetJumptableTargets(label_entry, &targets)) {
      MAO_ASSERT(targets);
      // Iterate over the targets and put them in the output iterator
      for (CFG::JumpTableTargets::const_iterator t_iter = targets->begin();
           t_iter != targets->end();
           ++t_iter) {
        *iter++ = (*t_iter)->name();
        processed = true;
      }
    } else {
      // We were unable to fully understand the target! Dont flag
      // if as processed..
      Trace(2, "Unable to identify the targets in jump table");
      CFG_->IncreaseNumUnresolvedJumps();
      if (cfg_stat_) cfg_stat_->FoundUnresolvedJump();
    }
    if (cfg_stat_ && processed) cfg_stat_->FoundJumpTablePattern();
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
            MaoUnit::BBNameGen::GetUniqueName(),
            function_,
            function_->GetSubSection());
        l->set_from_assembly(false);
        (*p_iter)->LinkBefore(l);
        *iter++ = l->name();
      }
      processed = true;
    }
    if (cfg_stat_ && processed) cfg_stat_->FoundVaargPattern();
  }

  if (insn_entry->IsIndirectJump() && !processed) {
    CFG_->IncreaseNumExternalJumps();
    CFG_->IncreaseNumUnresolvedJumps();
    if (cfg_stat_) cfg_stat_->FoundUnresolvedJump();
    if (tracing_level() >1) {
      Trace(2, "Unable to find targets for indirect jump.");

      // Print out info so we can locate the problem:
      std::string s;
      insn_entry->InstructionToString(&s);
      insn_entry->ProfileToString(&s);
      insn_entry->SourceInfoToString(&s);
      Trace(2, "%s", s.c_str());
    }
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

InstructionEntry *BasicBlock::GetFirstInstruction() const {
  MaoEntry *e = first_entry_;
  while (e) {
    if (e->IsInstruction()) return e->AsInstruction();
    if (e == last_entry_)
      return NULL;
    e = e->next();
  }
  return NULL;
}

InstructionEntry *BasicBlock::GetLastInstruction() const {
  MaoEntry *e = last_entry_;
  while (e) {
    if (e->IsInstruction()) return e->AsInstruction();
    if (e == first_entry_)
      return NULL;
    e = e->prev();
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

void CreateCFG(MaoUnit *mao_unit, Function *function, CFG *cfg,
               bool conservative) {
  CFGBuilder builder(mao_unit, function, cfg, conservative);
  builder.Go();
}

void InitCFG() {
  RegisterStaticOptionPass("CFG", new MaoOptionMap);
}


void CFGBuilder::CFGStat::Print(FILE *out) {
  if (number_of_direct_jumps_)
    fprintf(out, "CFG: Direct  jumps:      %7d\n", number_of_direct_jumps_);
  if (number_of_indirect_jumps_)
    fprintf(out, "CFG: Indirect jumps:     %7d (%d unresolved)\n",
            number_of_indirect_jumps_, number_of_unresolved_jumps_);
  if (number_of_jump_table_patterns_)
    fprintf(out, "CFG: Jump table patterns:%7d\n",
            number_of_jump_table_patterns_);
  if (number_of_vaarg_patterns_)
    fprintf(out, "CFG: VA_ARG patterns    :%7d\n", number_of_vaarg_patterns_);
  if (number_of_tail_calls_)
    fprintf(out, "CFG: Tail calls         :%7d\n", number_of_tail_calls_);
}
