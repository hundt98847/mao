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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#include <iostream>

#include "MaoDebug.h"
#include "MaoUnit.h"


extern "C" const char *S_GET_NAME(symbolS *s);

//
// Class: MaoUnit
//

MaoUnit::MaoUnit() :
    current_subsection_(0),
    current_basicblock_(0) {
  // Default to no subsection selected
  // A default will be generated if necessary later on.
  entries_.clear();
  sub_sections_.clear();
  sections_.clear();
}

MaoUnit::~MaoUnit() {
  // Remove entries and free allocated memory
  for (std::vector<MaoUnitEntryBase *>::iterator iter =
           entries_.begin();
      iter != entries_.end(); ++iter) {
    delete *iter;
  }

  // Remove subsections and free allocated memory
  for (std::vector<SubSection *>::iterator iter =
           sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    delete *iter;
  }

  // Remove sections and free allocated memory
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
      iter != sections_.end(); ++iter) {
    delete iter->second;
  }

  // Remove basic blocks and free allocated memory
  for (std::vector<BasicBlock *>::iterator iter =
           basicblocks_.begin();
       iter != basicblocks_.end(); ++iter) {
    delete *iter;
  }

  // Remove basic blocks and free allocated memory
  for (std::vector<BasicBlockEdge *>::iterator iter =
           basicblock_edges_.begin();
       iter != basicblock_edges_.end(); ++iter) {
    delete *iter;
  }
}

void MaoUnit::PrintMaoUnit() const {
  PrintMaoUnit(stdout);
}

// Prints all entries in the MaoUnit
void MaoUnit::PrintMaoUnit(FILE *out) const {
  for (std::vector<MaoUnitEntryBase *>::const_iterator iter =
           entries_.begin();
      iter != entries_.end(); ++iter) {
    MaoUnitEntryBase *e = *iter;
    e->PrintEntry(out);
  }
}

void MaoUnit::PrintIR() const {
  PrintIR(stdout);
}


// TODO(martint): Re-factor the code and take out the different parts
void MaoUnit::PrintIR(FILE *out) const {
  unsigned int index = 0;
  // Print the entries
  for (std::vector<MaoUnitEntryBase *>::const_iterator e_iter =
           entries_.begin();
      e_iter != entries_.end(); ++e_iter) {
    MaoUnitEntryBase *e = *e_iter;
    fprintf(out, "[%5d][%c] ", index, e->GetDescriptiveChar());
    e->PrintIR(out);
    fprintf(out, "\n");
    index++;
  }

  // Print the sections
  index = 0;
  fprintf(out, "Sections : \n");
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
      iter != sections_.end(); ++iter) {
    Section *section = iter->second;
    fprintf(out,"[%3d] %s [", index, section->name());
    // Print out the subsections in this section as a list of indexes
    std::vector<subsection_index_t> *subsection_indexes = section->GetSubSectionIndexes();
    for (std::vector<subsection_index_t>::const_iterator si_iter =
             subsection_indexes->begin();
         si_iter != subsection_indexes->end(); ++si_iter) {
    fprintf(out, " %d", *si_iter);
    }
    fprintf(out, "]\n");
    index++;
  }

  // Print the subsections
  index = 0;
  fprintf(out, "Subsections : \n");
  for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    SubSection *ss = *iter;
    fprintf(out, "[%3d] [%d-%d]: %s (%s)\n", index,
            ss->first_entry_index(), ss->last_entry_index(), ss->name(),
            ss->creation_op());
    index++;
  }

  // Print the basic blocks
  fprintf(out, "Basic blocks:\n");
  for(unsigned int i = 0; i < basicblocks_.size(); i++){
    fprintf(out, "bb%d: ", i);
    BasicBlock *bb = basicblocks_[i];
    if (0 == bb) {
      fprintf(out, "<DELETED>");
    } else {
      fprintf(out, "BB [%d-%d]", bb->first_entry_index(), bb->last_entry_index());
    }
    fprintf(out, "\n");
  }

  // Print the edges
  fprintf(out, "Basic block edges:\n");
  for(unsigned int i = 0; i < basicblock_edges_.size(); i++){
    fprintf(out, "edge%d: ", i);
    BasicBlockEdge *bbe = basicblock_edges_[i];
    if (0 == bbe) {
      fprintf(out, "<DELETED>");
    } else {
      fprintf(out, " bb%d -> bb%d", bbe->source_index(), bbe->target_index());
    }
    fprintf(out, "\n");
  }


}

Section *MaoUnit::FindOrCreateAndFind(const char *section_name) {
  Section *section;
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name);
  if (it == sections_.end()) {
    // create it!
    section = new Section(section_name);
    sections_[section->name()] = section;
  } else {
    section = it->second;
  }
  MAO_ASSERT(section);
  return section;
}

// Called when found a new subsection reference in the assembly.
void MaoUnit::SetSubSection(const char *section_name,
                            unsigned int subsection_number,
                            const char *creation_op) {
  // Get (and possibly create) the section
  Section *section = FindOrCreateAndFind(section_name);
  MAO_ASSERT(section);
  // Create a new subsection, even if the same subsection number
  // have already been used.
  SubSection *subsection = new SubSection(subsection_number, section_name,
                                          creation_op);
  sub_sections_.push_back(subsection);
  section->AddSubSectionIndex(sub_sections_.size()-1);

  // set current_subsection!
  current_subsection_ = subsection;

  current_subsection_->set_first_entry_index(entries_.size());
  current_subsection_->set_last_entry_index(entries_.size());
}

// Add a new basic block. Return a pointer to it to be able
// to update it afterwards.
BasicBlock *MaoUnit::AddBasicblock(subsection_index_t start_index,
                                   subsection_index_t end_index) {
  BasicBlock *bb = new BasicBlock();
  bb->set_first_entry_index(start_index);
  bb->set_last_entry_index(end_index);
  basicblocks_.push_back(bb);
  return bb;
}


BasicBlockEdge *MaoUnit::AddBasicBlockEdge(basicblock_index_t source_index,
                                           basicblock_index_t target_index) {
  BasicBlock *bb;
  BasicBlockEdge *bbe = new BasicBlockEdge();
  bbe->set_source_index(source_index);
  bbe->set_target_index(target_index);
  basicblock_edges_.push_back(bbe);

  // Now update the basic blocks
  MAO_ASSERT(source_index < basicblocks_.size());
  bb = basicblocks_[source_index];
  MAO_ASSERT(bb);
  bb->AddOutEdge(bbe);
  MAO_ASSERT(target_index < basicblocks_.size());
  bb = basicblocks_[target_index];
  MAO_ASSERT(bb);
  bb->AddInEdge(bbe);

  return bbe;
}


// Add an entry to the MaoUnit list
bool MaoUnit::AddEntry(MaoUnitEntryBase *entry, bool create_default_section) {
  // Variables that _might_ get used.
  LabelEntry *label_entry;
  Symbol *symbol;

  entry_index_t number_of_entries_added = entries_.size();

  MAO_ASSERT(entry);

  // Create a subsection if necessary
  if (create_default_section && !current_subsection_) {
    SetSubSection(DEFAULT_SECTION_NAME, 0, DEFAULT_SECTION_CREATION_OP);
    MAO_ASSERT(current_subsection_);
  }

  // Check the type
  switch (entry->entry_type()) {
    case MaoUnitEntryBase::INSTRUCTION:
      break;
    case MaoUnitEntryBase::LABEL:
      // A Label will generate in a new symbol in the symbol table
      // TODO(martint): fix casting
      label_entry = (LabelEntry *)entry;
      symbol = symbol_table_.FindOrCreateAndFind(label_entry->name());
      MAO_ASSERT(symbol);
      if (0 != current_subsection_) {
        symbol->set_section_name(current_subsection_->name());
      }
      break;
    case MaoUnitEntryBase::DEBUG:
      break;
    case MaoUnitEntryBase::DIRECTIVE:
      break;
    case MaoUnitEntryBase::UNDEFINED:
      break;
    default:
      // should never happen. Catch all cases above.
      MAO_RASSERT(0);
  }

  // Add the entry to the compilation unit
  entries_.push_back(entry);
  if (current_subsection_) {
    current_subsection_->set_last_entry_index(number_of_entries_added);
  }

  // Update basic block information
  if (entry->BelongsInBasicBlock()) {
    if (! current_basicblock_) {
      // Assume the basic block is only one entry long. It is updated
      // later if we need to
      current_basicblock_ = AddBasicblock(number_of_entries_added,
                                          number_of_entries_added);
    } else {
      current_basicblock_->set_last_entry_index(number_of_entries_added);
    }
  }
  // This forces a new basic block to be created when the next
  // entry that belongs in a basic block is encountered.
  if (entry->EndsBasicBlock()) {
     current_basicblock_ = 0;
  }

  return true;
}


// Adds a common symbol
bool MaoUnit::AddCommSymbol(const char *name, unsigned int common_size,
                            unsigned int common_align) {
  // A common symbol is different as it allows
  // several definitions of the symbol.
  // See http://sourceware.org/binutils/docs-2.19/as/Comm.html#Comm
  Symbol *symbol;
  if ( !symbol_table_.Exists(name) ) {
    // If symbol does not exists,
    // insert it. with default properties
    symbol = symbol_table_.Add(name, new Symbol(name));
    symbol->set_symbol_type(OBJECT_SYMBOL);
  } else {
    // Get the symbol
    symbol = symbol_table_.Find(name);
  }
  // Set the attributes associated with common symbols
  symbol->set_common(true);
  if (symbol->common_size() < common_size) {
    symbol->set_common_size(common_size);
    MAO_ASSERT(symbol->size() < symbol->common_size());
    symbol->set_size(symbol->common_size());
  }
  if (symbol->common_align() < common_align) {
    symbol->set_common_align(common_align);
  }
  return true;
}

//
// Class: AsmInstruction
//

AsmInstruction::AsmInstruction(i386_insn *instruction) {
  MAO_ASSERT(instruction);
  instruction_ = CreateInstructionCopy(instruction);
}

AsmInstruction::~AsmInstruction() {
  MAO_ASSERT(instruction_);
  FreeInstruction();
}

const char *AsmInstruction::get_op() const {
  MAO_ASSERT(instruction_);
  MAO_ASSERT(instruction_->tm.name);
  return(instruction_->tm.name);
}

// This deallocates memory allocated in CreateInstructionCopy().
void AsmInstruction::FreeInstruction() {
  MAO_ASSERT(instruction_);
  for (unsigned int i = 0; i < instruction_->operands; i++) {
    if (IsImmediateOperand(instruction_, i)) {
      free(instruction_->op[i].imms);
    }
    if (IsMemOperand(instruction_, i)) {
      free(instruction_->op[i].disps);
    }
    if (IsRegisterOperand(instruction_, i)) {
      free((reg_entry *)instruction_->op[i].regs);
    }
  }
  for (unsigned int i = 0; i < 2; i++) {
    free((seg_entry *)instruction_->seg[i]);
  }
  free((reg_entry *)instruction_->base_reg);
  free((reg_entry *)instruction_->index_reg);
  free(instruction_);
}


// Given a register, create a copy to be used in our instruction
reg_entry *AsmInstruction::CopyRegEntry(const reg_entry *in_reg) {
  if (!in_reg)
    return 0;
  reg_entry *tmp_r;
  tmp_r = (reg_entry *)malloc(sizeof(reg_entry) );
  MAO_ASSERT(tmp_r);
  MAO_ASSERT(strlen(in_reg->reg_name) < kMaxRegisterNameLength);
  tmp_r->reg_name = strdup(in_reg->reg_name);
  tmp_r->reg_type = in_reg->reg_type;
  tmp_r->reg_flags = in_reg->reg_flags;
  tmp_r->reg_num = in_reg->reg_num;
  return tmp_r;
}


bool AsmInstruction::IsMemOperand(const i386_insn *instruction,
                                  const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.disp8
          || t.bitfield.disp16
          || t.bitfield.disp32
          || t.bitfield.disp32s
          || t.bitfield.disp64
          || t.bitfield.baseindex);
}

bool AsmInstruction::IsImmediateOperand(const i386_insn *instruction,
                                        const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
      return (t.bitfield.imm1
              || t.bitfield.imm8
              || t.bitfield.imm8s
              || t.bitfield.imm16
              || t.bitfield.imm32
              || t.bitfield.imm32s
              || t.bitfield.imm64);
}

bool AsmInstruction::IsRegisterOperand(const i386_insn *instruction,
                                       const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.reg8
          || t.bitfield.reg16
          || t.bitfield.reg32
          || t.bitfield.reg64);
}

// Prints out the instruction. This is work-in-progress, but currently
// supports the assembly instructions found in the assembled version
// of mao. Please add functionality when unsupported instructions are found.
void AsmInstruction::PrintInstruction(FILE *out) const {
  int scale[] = { 1, 2, 4, 8 };

  if (((strcmp(instruction_->tm.name, "movsbl") == 0) ||
       (strcmp(instruction_->tm.name, "movswl") == 0) ||
       (strcmp(instruction_->tm.name, "movzbl") == 0) ||
       (strcmp(instruction_->tm.name, "movzwl") == 0) ||
       (strcmp(instruction_->tm.name, "movswl") == 0) ||
       (strcmp(instruction_->tm.name, "cmovl") == 0) ||
       (strcmp(instruction_->tm.name, "cmovnl") == 0) ||
       (strcmp(instruction_->tm.name, "cwtl") == 0) ||
       (strcmp(instruction_->tm.name, "cltd") == 0)) &&
       (instruction_->suffix == 'l')) {
    // these functions should not have the prefix written out as its already in
    // the name property.
    fprintf(out, "\t%s\t", instruction_->tm.name);
  } else {
    fprintf(out, "\t%s%c\t", instruction_->tm.name,
            (instruction_->suffix &&
             instruction_->tm.name[strlen(instruction_->tm.name)-1] != 'q') ?
            instruction_->suffix : ' ');
     // 'grep q\" i386-tbl.h' lists the instruction that ends with q.
  }

  for (unsigned int i =0; i < instruction_->operands; i++) {
    // Segment overrides are always placed in seg[0], even
    // if it applies to the second operand.
    if (IsMemOperand(instruction_, i) && instruction_->seg[0]) {
      fprintf(out, "%%%s:", instruction_->seg[0]->seg_name);
    }
    if (IsImmediateOperand(instruction_, i)) {
      if (instruction_->op[i].imms->X_op == O_constant) {
        // The cast allows us to compile and run for targets
        // configured to either 32 bit or 64 bit architectures.
        fprintf(out, "$%lld",
                (long long)instruction_->op[i].imms->X_add_number);
      }
      if (instruction_->op[i].imms->X_op == O_symbol) {
        fprintf(out, "$%s",
                S_GET_NAME(instruction_->op[i].imms->X_add_symbol));
        if (instruction_->op[i].imms->X_add_number) {
          fprintf(out, "%s%d",
                  (((int)instruction_->op[i].imms->X_add_number)<0)?"":"+",
                  (int)instruction_->op[i].imms->X_add_number);
        }
      }
    }
    // This case is now handles in the displacement
//     if (instruction_->tm.opcode_modifier.jump ||
//         instruction_->tm.opcode_modifier.jumpdword ||
//         instruction_->tm.opcode_modifier.jumpbyte) {
//       if (instruction_->op[i].disps->X_op == O_symbol)
//         fprintf(out, "%s",
//                 S_GET_NAME(instruction_->op[i].disps->X_add_symbol) );
//       else
//         fprintf(out, "*unk*");
//     }
    if (IsMemOperand(instruction_, i)) {
      // symbol?
      if (instruction_->types[i].bitfield.disp8 ||
          instruction_->types[i].bitfield.disp16 ||
          instruction_->types[i].bitfield.disp32 ||
          instruction_->types[i].bitfield.disp32s ||
          instruction_->types[i].bitfield.disp64) {
        if (instruction_->op[i].disps->X_op == O_symbol) {
          fprintf(out, "%s",
                  S_GET_NAME(instruction_->op[i].disps->X_add_symbol) );
        }
        if (instruction_->op[i].disps->X_op == O_symbol &&
            instruction_->op[i].disps->X_add_number != 0) {
          fprintf(out, "+");
        }
        if (instruction_->types[i].bitfield.jumpabsolute) {
          fprintf(out, "*");
        }
        if (instruction_->op[i].disps->X_add_number != 0) {
          // The cast allows us to compile and run for targets
          // configured to either 32 bit or 64 bit architectures.
          fprintf(out, "%lld",
                  (long long)instruction_->op[i].disps->X_add_number);
        }
      }
      if (instruction_->base_reg || instruction_->index_reg)
        fprintf(out, "(");
      if (instruction_->base_reg)
        fprintf(out, "%%%s", instruction_->base_reg->reg_name);
      if (instruction_->index_reg)
        fprintf(out, ",%%%s", instruction_->index_reg->reg_name);
      if (instruction_->log2_scale_factor)
        fprintf(out, ",%d", scale[instruction_->log2_scale_factor]);
      if (instruction_->base_reg || instruction_->index_reg)
        fprintf(out, ")");
    }
    if (instruction_->types[i].bitfield.acc) {
      // need to find out size in order to print the correct ACC register!
      if (instruction_->types[i].bitfield.byte )
        fprintf(out, "%%al");
      if (instruction_->types[i].bitfield.word )
        fprintf(out, "%%ax");
      if (instruction_->types[i].bitfield.dword )
        fprintf(out, "%%eax");
      if (instruction_->types[i].bitfield.qword )
        fprintf(out, "%%rax");
    }
// TODO(martint): fix floatacc
// if ((instruction_->types[i].bitfield.floatacc){
// }

    if (IsRegisterOperand(instruction_, i)) {
      if (instruction_->types[i].bitfield.jumpabsolute) {
        fprintf(out, "*");
      }
      fprintf(out, "%%%s", instruction_->op[i].regs->reg_name);
    }
    if (instruction_->types[i].bitfield.shiftcount) {
      // its a register name!
      fprintf(out, "%%%s", instruction_->op[i].regs->reg_name);
    }

    if (i < instruction_->operands-1)
      fprintf(out, ", ");
  }
}

// From an instruction given by gas, allocate new memory and populate the
// members.
i386_insn *AsmInstruction::CreateInstructionCopy(i386_insn *in_inst) {
  i386_insn *new_inst = (i386_insn *)malloc(sizeof(i386_insn) );
  MAO_ASSERT(new_inst);

  // Template related members
  new_inst->tm = in_inst->tm;
  new_inst->suffix = in_inst->suffix;
  new_inst->operands = in_inst->operands;
  new_inst->reg_operands = in_inst->reg_operands;
  new_inst->disp_operands = in_inst->disp_operands;
  new_inst->mem_operands = in_inst->mem_operands;
  new_inst->imm_operands = in_inst->imm_operands;

  // Types
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    for (unsigned int j = 0; j < OTNumOfUints; j++) {
      new_inst->types[i].array[j] = in_inst->types[i].array[j];
    }
  }

  // Ops
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    // Copy the correct part of the op[i] union
    if (IsImmediateOperand(in_inst, i)) {
      new_inst->op[i].imms = (expressionS *)malloc(sizeof(expressionS) );
      MAO_ASSERT(new_inst->op[i].imms);
      new_inst->op[i].imms->X_op = in_inst->op[i].imms->X_op;

      // TODO(martint): Check if we need to allocate memory for these instead
      // of just blindly copying them.
      new_inst->op[i].imms->X_add_number = in_inst->op[i].imms->X_add_number;
      new_inst->op[i].imms->X_add_symbol = in_inst->op[i].imms->X_add_symbol;
    }

//     if (in_inst->tm.opcode_modifier.jump ||
//         in_inst->tm.opcode_modifier.jumpdword ||
//         in_inst->tm.opcode_modifier.jumpbyte) {
//       // TODO(martint): make sure the full contents off disps is copied
//         new_inst->op[i].disps = (expressionS *)malloc(sizeof(expressionS) );
//         new_inst->op[i].disps->X_add_symbol =
//            in_inst->op[i].disps->X_add_symbol; // ERROR
//         new_inst->op[i].disps->X_add_number =
//            in_inst->op[i].disps->X_add_number; // ERROR
//         new_inst->op[i].disps->X_op = in_inst->op[i].disps->X_op;
//     }

    if (IsMemOperand(in_inst, i)) {
      if (in_inst->op[i].disps) {
        new_inst->op[i].disps = (expressionS *)malloc(sizeof(expressionS));
        MAO_ASSERT(new_inst->op[i].disps);
        new_inst->op[i].disps->X_add_symbol =
            in_inst->op[i].disps->X_add_symbol;
        new_inst->op[i].disps->X_add_number =
            in_inst->op[i].disps->X_add_number;
        new_inst->op[i].disps->X_op = in_inst->op[i].disps->X_op;
      } else {
        new_inst->op[i].disps = 0;
      }
    }
    if (IsRegisterOperand(in_inst, i)) {
      new_inst->op[i].regs = CopyRegEntry(in_inst->op[i].regs);
    }
    if (in_inst->types[i].bitfield.shiftcount) {
      new_inst->op[i].regs = CopyRegEntry(in_inst->op[i].regs);
    }
  }
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    new_inst->flags[i]= in_inst->flags[i];
  }
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    new_inst->reloc[i] = in_inst->reloc[i];
  }
  new_inst->base_reg = CopyRegEntry(in_inst->base_reg);
  new_inst->index_reg = CopyRegEntry(in_inst->index_reg);
  new_inst->log2_scale_factor = in_inst->log2_scale_factor;

  // Segment overrides
  for (unsigned int i = 0; i < 2; i++) {
    if (in_inst->seg[i]) {
      seg_entry *tmp_seg = (seg_entry *)malloc(sizeof(seg_entry) );
      MAO_ASSERT(tmp_seg);
      MAO_ASSERT(strlen(in_inst->seg[i]->seg_name) < MAX_SEGMENT_NAME_LENGTH);
      tmp_seg->seg_name = strdup(in_inst->seg[i]->seg_name);
      tmp_seg->seg_prefix = in_inst->seg[i]->seg_prefix;
      new_inst->seg[i] = tmp_seg;
    } else {
      new_inst->seg[i] = 0;
    }
  }

  // Prefixes
  new_inst->prefixes = in_inst->prefixes;
  for (unsigned int i = 0; i < 2; i++) {
    new_inst->prefix[i] = in_inst->prefix[i];
  }

  new_inst->rm = in_inst->rm;
  new_inst->rex = in_inst->rex;
  new_inst->sib = in_inst->sib;
  new_inst->drex = in_inst->drex;
  new_inst->vex = in_inst->vex;

  return new_inst;
}

bool AsmInstruction::EndsBasicBlock() const {
  // TODO(martint): Find out what instruction that change control
  // flow.
  if (instruction_->tm.opcode_modifier.jump ||
      instruction_->tm.opcode_modifier.jumpdword ||
      instruction_->tm.opcode_modifier.jumpbyte) {
    return true;
  }
  return false;
}

//
// Class: Directive
//
Directive::Directive(const char *key, const char *value) {
  MAO_ASSERT(key);
  MAO_ASSERT(value);
  MAO_ASSERT(strlen(key) < MAX_DIRECTIVE_NAME_LENGTH);
  key_ = strdup(key);
  MAO_ASSERT(strlen(value) < MAX_DIRECTIVE_NAME_LENGTH);
  value_ = strdup(value);
}

Directive::~Directive() {
  MAO_ASSERT(key_);
  free(key_);
  MAO_ASSERT(value_);
  free(value_);
}

const char *Directive::key() {
  MAO_ASSERT(key_);
  return key_;
};

const char *Directive::value() {
  MAO_ASSERT(value_);
  return value_;
}

//
// Class: Label
//

Label::Label(const char *name) {
  MAO_ASSERT(name);
  MAO_ASSERT(strlen(name) < MAX_SEGMENT_NAME_LENGTH);
  name_ = strdup(name);
}

Label::~Label() {
  MAO_ASSERT(name_);
  free(name_);
}

const char *Label::name() const {
  MAO_ASSERT(name_);
  return name_;
}

//
// Class: MaoUnitEntryBase
//

MaoUnitEntryBase::MaoUnitEntryBase(unsigned int line_number,
                                   const char *line_verbatim) :
    line_number_(line_number) {
  if (line_verbatim) {
    MAO_ASSERT(strlen(line_verbatim) < MAX_VERBATIM_ASSEMBLY_STRING_LENGTH);
    line_verbatim_ = strdup(line_verbatim);
  } else {
    line_verbatim_ = 0;
  }
}

MaoUnitEntryBase::~MaoUnitEntryBase() {
}


void MaoUnitEntryBase::Spaces(unsigned int n, FILE *outfile) const {
  for (unsigned int i = 0; i < n; i++) {
    fprintf(outfile, " ");
  }
}

//
// Class: LabelEntry
//

LabelEntry::LabelEntry(const char *name, unsigned int line_number,
                       const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  label_ = new Label(name);
}

LabelEntry::~LabelEntry() {
  delete label_;
}

void LabelEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(label_);
  fprintf(out, "%s:", label_->name() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void LabelEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(label_);
  fprintf(out, "%s", label_->name() );
}


MaoUnitEntryBase::EntryType LabelEntry::entry_type() const {
  return LABEL;
}

const char *LabelEntry::name() {
  return label_->name();
}

//
// Class: DirectiveEntry
//

DirectiveEntry::DirectiveEntry(const char *key, const char *value,
                               unsigned int line_number,
                               const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  directive_ = new Directive(key, value);
}

DirectiveEntry::~DirectiveEntry() {
  delete directive_;
}

void DirectiveEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "\t%s\t%s", directive_->key(), directive_->value() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void DirectiveEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "%s %s", directive_->key(),
          directive_->value());
}


MaoUnitEntryBase::EntryType DirectiveEntry::entry_type() const {
  return DIRECTIVE;
}


//
// Class: DebugEntry
//

DebugEntry::DebugEntry(const char *key, const char *value,
                       unsigned int line_number, const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  directive_ = new Directive(key, value);
}

DebugEntry::~DebugEntry() {
  delete directive_;
}

void DebugEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "\t%s\t%s", directive_->key(), directive_->value() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void DebugEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(directive_);
  fprintf(out, "%s %s", directive_->key(),
          directive_->value());
}


MaoUnitEntryBase::EntryType DebugEntry::entry_type() const {
  return DEBUG;
}


//
// Class: InstructionEntry
//

InstructionEntry::InstructionEntry(i386_insn *instruction,
                                   unsigned int line_number,
                                   const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  instruction_ = new AsmInstruction(instruction);
}

InstructionEntry::~InstructionEntry() {
  delete instruction_;
}

void InstructionEntry::PrintEntry(FILE *out) const {
  instruction_->PrintInstruction(out);
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}


void InstructionEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(instruction_);
  fprintf(out, "%s", instruction_->get_op());
}


MaoUnitEntryBase::EntryType InstructionEntry::entry_type() const {
  return INSTRUCTION;
}


//
// Class: Section
//

Section::Section(const char *name) {
  MAO_ASSERT(name);
  MAO_ASSERT(strlen(name) < MAX_SEGMENT_NAME_LENGTH);
  name_ = strdup(name);
}

const char *Section::name() const {
  MAO_ASSERT(name_);
  return name_;
}

Section::~Section() {
  MAO_ASSERT(name_);
  free(name_);
}

void Section::AddSubSectionIndex(subsection_index_t index) {
  sub_section_indexes_.push_back(index);
}

//
// Class: SubSection
//

SubSection::SubSection(unsigned int subsection_number, const char *name,
                       const char *creation_op) :
    number_(subsection_number),
    first_entry_index_(0),
    last_entry_index_(0) {
  MAO_ASSERT(creation_op);
  creation_op_ = strdup(creation_op);  // this way free() works
  MAO_ASSERT(name);
  name_ = strdup(name);
}

unsigned int SubSection::number() const {
  return number_;
}

const char *SubSection::name() const {
  return name_;
}

const char *SubSection::creation_op() const {
  MAO_ASSERT(creation_op_);
  return creation_op_;
}

SubSection::~SubSection() {
  MAO_ASSERT(name_);
  free(name_);

  MAO_ASSERT(creation_op_);
  free(creation_op_);
}

//
// Class: BasicBlock
//

BasicBlock::BasicBlock() {
  //  Constructor
  in_edges_.clear();
  out_edges_.clear();
}

BasicBlock::~BasicBlock() {
  //  Destructor
}

void BasicBlock::AddInEdge(BasicBlockEdge *edge) {
  MAO_ASSERT(edge);
  in_edges_.push_back(edge);
}

void BasicBlock::AddOutEdge(BasicBlockEdge *edge) {
  MAO_ASSERT(edge);
  out_edges_.push_back(edge);
}

//
// Class: BasicBlockEdge
//
