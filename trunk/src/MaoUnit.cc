//
// Copyright 2008 Google Inc.
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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "MaoDebug.h"
#include "MaoUnit.h"

#include <iostream>
#include <sstream>

extern "C" const char *S_GET_NAME(symbolS *s);

//
// Class: MaoUnit
//

// Default to no subsection selected
// A default will be generated if necessary later on.
MaoUnit::MaoUnit(MaoOptions *mao_options)
    : current_subsection_(0), mao_options_(mao_options) {
  entry_vector_.clear();
  entry_list_.clear();
  sub_sections_.clear();
  sections_.clear();
}

MaoUnit::~MaoUnit() {
  // Remove entries and free allocated memory
  for (std::list<MaoUnitEntryBase *>::iterator iter =
           entry_list_.begin();
       iter != entry_list_.end(); ++iter) {
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
}

void MaoUnit::PrintMaoUnit() const {
  PrintMaoUnit(stdout);
}

// Prints all entries in the MaoUnit
void MaoUnit::PrintMaoUnit(FILE *out) const {
  for (std::list<MaoUnitEntryBase *>::const_iterator iter =
           entry_list_.begin();
       iter != entry_list_.end(); ++iter) {
    MaoUnitEntryBase *e = *iter;
    e->PrintEntry(out);
  }
}

void MaoUnit::PrintIR() const {
  PrintIR(stdout);
}


void MaoUnit::PrintIR(FILE *out) const {
  // Print the entries
  for (std::list<MaoUnitEntryBase *>::const_iterator e_iter =
           entry_list_.begin();
       e_iter != entry_list_.end(); ++e_iter) {
    MaoUnitEntryBase *e = *e_iter;
    fprintf(out, "[%5d][%c] ", e->index(), e->GetDescriptiveChar());
    if (MaoUnitEntryBase::INSTRUCTION == e->Type()) {
      fprintf(out, "\t");
    }
    e->PrintIR(out);
    fprintf(out, "\n");
  }

  // Print the sections
  unsigned int index = 0;
  fprintf(out, "Sections : \n");
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
       iter != sections_.end(); ++iter) {
    Section *section = iter->second;
    fprintf(out, "[%3d] %s [", index, section->name());
    // Print out the subsections in this section as a list of indexes
    std::vector<subsection_index_t> *subsection_indexes =
        section->GetSubSectionIndexes();
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
    fprintf(out, "[%3d] [%d-%d]: %s\n", index,
            ss->first_entry_index(), ss->last_entry_index(),
            ss->name().c_str());
    index++;
  }
}

std::pair<bool, Section *> MaoUnit::FindOrCreateAndFind(
    const char *section_name) {
  bool new_section = false;
  Section *section;
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name);
  if (it == sections_.end()) {
    // create it!
    section = new Section(section_name);
    sections_[section->name()] = section;
    new_section = true;
  } else {
    section = it->second;
  }
  MAO_ASSERT(section);
  return std::make_pair(new_section, section);
}

// Called when found a new subsection reference in the assembly.
bool MaoUnit::SetSubSection(const char *section_name,
                            unsigned int subsection_number) {
  // Get (and possibly create) the section
  std::pair<bool, Section *> section_pair = FindOrCreateAndFind(section_name);
  Section *section = section_pair.second;
  MAO_ASSERT(section);
  // Create a new subsection, even if the same subsection number
  // have already been used.
  SubSection *subsection = new SubSection(subsection_number, section_name);
  sub_sections_.push_back(subsection);
  section->AddSubSectionIndex(sub_sections_.size()-1);

  // set current_subsection!
  current_subsection_ = subsection;

  current_subsection_->set_first_entry_index(entry_list_.size());
  current_subsection_->set_last_entry_index(entry_list_.size());

  return section_pair.first;
}

LabelEntry *MaoUnit::GetLabelEntry(const char *label_name) const {
  std::map<const char *, LabelEntry *>::const_iterator iter =
      labels_.find(label_name);
  MAO_ASSERT(iter != labels_.end());
  return iter->second;
}

MaoUnitEntryBase *MaoUnit::GetEntryByID(const entry_index_t id) const {
  MAO_ASSERT(id < entry_vector_.size());
  MAO_ASSERT(entry_vector_[id] != 0);
  return entry_vector_[id];
}

bool MaoUnit::AddEntry(MaoUnitEntryBase *entry, bool build_sections,
                       bool create_default_section) {
  return AddEntry(entry, build_sections, create_default_section,
                  entry_list_.end());
}

// Add an entry to the MaoUnit list
bool MaoUnit::AddEntry(MaoUnitEntryBase *entry, bool build_sections,
                       bool create_default_section,
                       std::list<MaoUnitEntryBase *>::iterator list_iter) {
  // Variables that _might_ get used.
  LabelEntry *label_entry;
  Symbol *symbol;

  // next free ID for the entry
  entry_index_t entry_index = entry_vector_.size();

  MAO_ASSERT(entry);
  entry->set_index(entry_index);

  // Create a subsection if necessary
  if (build_sections && (create_default_section && !current_subsection_)) {
    SetSubSection(DEFAULT_SECTION_NAME, 0);
    MAO_ASSERT(current_subsection_);
  }

  // Check the type
  switch (entry->Type()) {
    case MaoUnitEntryBase::INSTRUCTION:
      break;
    case MaoUnitEntryBase::LABEL:
      // A Label will generate in a new symbol in the symbol table
      label_entry = (LabelEntry *)entry;
      MAO_ASSERT(labels_.insert(std::make_pair(label_entry->name(),
                                               label_entry)).second);
      symbol = symbol_table_.FindOrCreateAndFind(label_entry->name());
      MAO_ASSERT(symbol);
      // TODO(martint): The codes does not currently set the correct
      //                section for all labels in the symboltable.
      break;
    case MaoUnitEntryBase::DEBUG:
      break;
    case MaoUnitEntryBase::DIRECTIVE:
      break;
    case MaoUnitEntryBase::UNDEFINED:
      break;
    default:
      // should never happen. Catch all cases above.
      MAO_RASSERT_MSG(0, "Entry type not recognised.");
  }

  // Add the entry to the compilation unit
  entry_vector_.push_back(entry);
  entry_list_.insert(list_iter, entry);

  // Update subsection information
  if (build_sections && current_subsection_) {
    current_subsection_->set_last_entry_index(entry_index);
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

int MaoUnit::BBNameGen::i = 0;
const char *MaoUnit::BBNameGen::GetUniqueName() {
  char *buff = strdup(".mao_label_XXXX");
  MAO_ASSERT(i <= 9999);
  sprintf(buff, ".mao_label_%04d", i);
  i++;
  return buff;
}

//
// Class: AsmInstruction
//

AsmInstruction::AsmInstruction(i386_insn *instruction, MaoOpcode opcode)
  : op_(opcode) {
  MAO_ASSERT(instruction);
  MAO_ASSERT(op_ != OP_invalid);
  instruction_ = CreateInstructionCopy(instruction);
}

AsmInstruction::~AsmInstruction() {
  MAO_ASSERT(instruction_);
  FreeInstruction();
}

const char *AsmInstruction::GetOp() const {
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
          || t.bitfield.reg64
          || t.bitfield.floatreg
          || t.bitfield.regxmm);
}

void AsmInstruction::PrintImmediateOperand(FILE *out,
                                           const enum bfd_reloc_code_real reloc,
                                           const expressionS *expr) const {
  switch (expr->X_op) {
    case O_constant:
      /* X_add_number (a constant expression).  */
      fprintf(out, "$%lld",
              (long long)expr->X_add_number);
      break;
    case O_symbol:
      /* X_add_symbol + X_add_number.  */
      if (expr->X_add_symbol) {
        fprintf(out, "$%s%s+",
                S_GET_NAME(expr->X_add_symbol),
                GetRelocString(reloc));
      }
      fprintf(out, "%lld",
              (long long)expr->X_add_number);
      break;
    case O_subtract:
      fprintf(out, "$(");
      /* (X_add_symbol - X_op_symbol) + X_add_number.  */
      if (expr->X_add_symbol || expr->X_op_symbol) {
        fprintf(out, "(");
      }
      if (expr->X_add_symbol) {
        fprintf(out, "%s%s",
                S_GET_NAME(expr->X_add_symbol),
                GetRelocString(reloc));
      }
      if (expr->X_op_symbol) {
        fprintf(out, "-%s",
                S_GET_NAME(expr->X_op_symbol));
      }
      if (expr->X_add_symbol || expr->X_op_symbol) {
        fprintf(out, ")+");
      }
      fprintf(out, "%lld)",
              (long long)expr->X_add_number);
      break;
    default:
      MAO_ASSERT_MSG(0, "Unable to print unsupported expression");
  }
  return;
}

const char *AsmInstruction::GetRelocString(
  const enum bfd_reloc_code_real reloc) const {
  switch (reloc) {
    case BFD_RELOC_X86_64_PLT32:
      return "@PLT";
    case BFD_RELOC_32_PCREL:
      return "@GOTPCREL";
    case BFD_RELOC_X86_64_TLSLD:
      return "@TLSLD";
    case BFD_RELOC_X86_64_TLSGD:
      return "@TLSGD";
    case BFD_RELOC_X86_64_DTPOFF32:
      return "@DTPOFF";
    case BFD_RELOC_NONE:  // found in "leaq    .LC0(%rip), %rcx"
      return "";
    default:
      MAO_ASSERT_MSG(false, "Unable to find info about reloc: %d (%d)", reloc);
      break;
  }
  return "";
}

// segment-override:signed-offset(base,index,scale)
void AsmInstruction::PrintMemoryOperand(FILE                  *out,
                                        const i386_operand_type &operand_type,
                                        const enum bfd_reloc_code_real reloc,
                                        const expressionS     *expr,
                                        const char            *segment_override,
                                        const bool            jumpabsolute)
    const {
  int scale[] = { 1, 2, 4, 8 };

  if (jumpabsolute) {
    fprintf(out, "*");
  }

  // segment-override:
  if (segment_override) {
    fprintf(out, "%%%s:", segment_override);
  }

  if (operand_type.bitfield.disp8 ||
      operand_type.bitfield.disp16 ||
      operand_type.bitfield.disp32 ||
      operand_type.bitfield.disp32s ||
      operand_type.bitfield.disp64) {
    // Signed-offset:
    switch (expr->X_op) {
      case O_constant:
        /* X_add_number (a constant expression).  */
        fprintf(out, "(%lld)",
                (long long)expr->X_add_number);
        break;
      case O_symbol:
        fprintf(out, "(");
        /* X_add_symbol + X_add_number.  */
        if (expr->X_add_symbol) {
          fprintf(out, "%s%s+",
                  S_GET_NAME(expr->X_add_symbol),
                  GetRelocString(reloc));
        }
        fprintf(out, "%lld", (long long)expr->X_add_number);
        fprintf(out, ")");
        break;
        /* (X_add_symbol - X_op_symbol) + X_add_number.  */
      case O_subtract:
        if (expr->X_add_symbol || expr->X_op_symbol) {
          fprintf(out, "(");
        }
        if (expr->X_add_symbol) {
          fprintf(out, "%s%s",
                  S_GET_NAME(expr->X_add_symbol),
                  GetRelocString(reloc));
        }
        // When GOTPCREL is used, the secnod symbol is implicit and
        // should not be printed.
        if (reloc != BFD_RELOC_32_PCREL) {
          if (expr->X_op_symbol) {
            fprintf(out, "-%s",
                    S_GET_NAME(expr->X_op_symbol));
          }
        }
        if (expr->X_add_symbol || expr->X_op_symbol) {
          fprintf(out, ")+");
        }
        fprintf(out, "%lld",
                (long long)expr->X_add_number);
        break;
      default:
        MAO_ASSERT_MSG(0, "Unable to print unsupported expression: %d",
                       expr->X_op);
    }
  }

  // (base,index,scale)
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

  return;
}



bool AsmInstruction::PrintSuffix() const {
  if (instruction_->suffix == 0) {
    return false;
  }
  const MaoOpcode opcode_has_l_suffix[] =  {
    OP_movsbl, OP_movswl, OP_movzbl, OP_movzwl, OP_movswl, OP_cmovl, OP_cmovnl,
    OP_cwtl, OP_cltd
  };
  const MaoOpcode opcode_has_w_suffix[] =  {
    OP_cbtw
  };
  const MaoOpcode opcode_has_b_suffix[] =  {
    OP_setb
  };
  if ((instruction_->suffix == 'l') &&
      IsInList(op(), opcode_has_l_suffix,
               sizeof(opcode_has_l_suffix)/sizeof(MaoOpcode))) {
    return false;
  }
  if ((instruction_->suffix == 'w') &&
      IsInList(op(), opcode_has_w_suffix,
               sizeof(opcode_has_w_suffix)/sizeof(MaoOpcode))) {
    return false;
  }
  if ((instruction_->suffix == 'b') &&
      IsInList(op(), opcode_has_b_suffix,
               sizeof(opcode_has_b_suffix)/sizeof(MaoOpcode))) {
    return false;
  }
  if (instruction_->suffix == 'q' &&
      instruction_->tm.name[strlen(instruction_->tm.name)-1] == 'q') {
    return false;
  }


  return true;
}

// Prints out the instruction.
// Goal is to make it print instructions generated by gcc
void AsmInstruction::PrintInstruction(FILE *out) const {
  // Prefixes
  fprintf(out, "\t");
  if (instruction_->prefixes > 0) {
    for (unsigned int i = 0;
         i < sizeof(instruction_->prefix)/sizeof(unsigned char);
         ++i) {
      if (instruction_->prefix[i] != 0) {
        switch (instruction_->prefix[i]) {
          case REPE_PREFIX_OPCODE:
            fprintf(out, "repz ");
            break;
          case REX_OPCODE+0:
          case REX_OPCODE+1:
          case REX_OPCODE+2:
          case REX_OPCODE+3:
          case REX_OPCODE+4:
          case REX_OPCODE+5:
          case REX_OPCODE+6:
          case REX_OPCODE+7:
          case REX_OPCODE+8:
          case REX_OPCODE+9:
          case REX_OPCODE+10:
          case REX_OPCODE+11:
          case REX_OPCODE+12:
          case REX_OPCODE+13:
          case REX_OPCODE+14:
          case REX_OPCODE+15:
            // do nothing: example: movq    %rsp, %rbp
            break;
          case DATA_PREFIX_OPCODE:
            // do nothing: example: cmpw    %cx, %ax
            break;
          default:
            MAO_ASSERT_MSG(false, "Unknown prefix found 0x%x\n",
                           instruction_->prefix[i]);
        }
      }
    }
  }

  // Do not print suffixes that are already in the template
  if (!PrintSuffix()) {
    fprintf(out, "%s\t", instruction_->tm.name);
  } else {
    fprintf(out, "%s%c\t", instruction_->tm.name, instruction_->suffix);
  }

  // Loop over operands
  for (unsigned int i = 0; i < instruction_->operands; ++i) {
    // IMMEDIATE
    // Immext means that an opcode modifier is encoded
    // in the instruction structure as an extra operand,
    // even though its not one!
    if (!instruction_->tm.opcode_modifier.immext &&
        IsImmediateOperand(instruction_, i)) {
      PrintImmediateOperand(out, instruction_->reloc[i],
                            instruction_->op[i].imms);
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

    // MEMORY OPERANDS

    // Segment overrides are always placed in seg[0], even
    // if it applies to the second operand.
    if (IsMemOperand(instruction_, i)) {
      PrintMemoryOperand(out,
                       instruction_->types[i],
                       instruction_->reloc[i],
                       instruction_->op[i].disps,
                       instruction_->seg[0]?instruction_->seg[0]->seg_name:0,
                       instruction_->types[i].bitfield.jumpabsolute ||
                       instruction_->tm.operand_types[i].bitfield.jumpabsolute);
    }

    // ACC register
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

    // ACC register
    if (instruction_->types[i].bitfield.floatacc) {
        fprintf(out, "%%st");
    }
    // TODO(martint): fix floatacc
    // if ((instruction_->types[i].bitfield.floatacc)...

    // REGISTERS

    // Segment register
    if (instruction_->types[i].bitfield.sreg2) {
      switch (instruction_->rm.reg) {
        case 0:
          fprintf(out, "%%es");
          break;
        case 1:
          fprintf(out, "%%cs");
          break;
        case 2:
          fprintf(out, "%%ss");
          break;
        case 3:
          fprintf(out, "%%ds");
          break;
        default:
          fprintf(stderr, "Unable to find segement regsiter sreg2%d\n",
                  instruction_->rm.reg);
          MAO_ASSERT(false);
      }
    }
    if (instruction_->types[i].bitfield.sreg3) {
      switch (instruction_->rm.reg) {
        case 4:
          fprintf(out, "%%fs");
          break;
        case 5:
          fprintf(out, "%%gs");
          break;
        default:
          fprintf(stderr, "Unable to find segement regsiter sreg3%d\n",
                  instruction_->rm.reg);
          MAO_ASSERT(false);
      }
    }

    // XMM registers
    if (instruction_->types[i].bitfield.regmmx) {
      if (instruction_->tm.operand_types[i].bitfield.regmmx) {
        fprintf(out, "%%mm%d", instruction_->rm.reg);
      } else if (instruction_->tm.operand_types[i].bitfield.regxmm) {
        fprintf(out, "%%xmm%d", instruction_->rm.reg);
      }
    }


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

    //   if (in_inst->tm.opcode_modifier.jump ||
    //       in_inst->tm.opcode_modifier.jumpdword ||
    //       in_inst->tm.opcode_modifier.jumpbyte) {
    //     // TODO(martint): make sure the full contents off disps is copied
    //       new_inst->op[i].disps = (expressionS *)malloc(sizeof(expressionS));
    //       new_inst->op[i].disps->X_add_symbol =
    //          in_inst->op[i].disps->X_add_symbol; // ERROR
    //       new_inst->op[i].disps->X_add_number =
    //          in_inst->op[i].disps->X_add_number; // ERROR
    //       new_inst->op[i].disps->X_op = in_inst->op[i].disps->X_op;
    //   }

    if (IsMemOperand(in_inst, i)) {
      if (in_inst->op[i].disps) {
        new_inst->op[i].disps = (expressionS *)malloc(sizeof(expressionS));
        MAO_ASSERT(new_inst->op[i].disps);
        new_inst->op[i].disps->X_add_symbol =
            in_inst->op[i].disps->X_add_symbol;
        new_inst->op[i].disps->X_add_number =
            in_inst->op[i].disps->X_add_number;
        new_inst->op[i].disps->X_op = in_inst->op[i].disps->X_op;
        new_inst->op[i].disps->X_op_symbol =
            in_inst->op[i].disps->X_op_symbol;
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
      seg_entry *tmp_seg = (seg_entry *)malloc(sizeof(seg_entry));
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
  for (unsigned int i = 0; i < MAX_PREFIXES; i++) {
    new_inst->prefix[i] = in_inst->prefix[i];
  }

  new_inst->rm = in_inst->rm;
  new_inst->rex = in_inst->rex;
  new_inst->sib = in_inst->sib;
  new_inst->drex = in_inst->drex;
  new_inst->vex = in_inst->vex;

  return new_inst;
}

bool AsmInstruction::IsInList(MaoOpcode opcode, const MaoOpcode list[],
                              const unsigned int number_of_elements) const {
  for (unsigned int i = 0; i < number_of_elements; i++) {
    if (opcode == list[i])
      return true;
  }
  return false;
}

const MaoOpcode cond_jumps[] = {
  // Conditional jumps.
  OP_jo,  OP_jno, OP_jb,   OP_jc,  OP_jnae, OP_jnb,  OP_jnc, OP_jae, OP_je,
  OP_jz,  OP_jne, OP_jnz,  OP_jbe, OP_jna,  OP_jnbe, OP_ja,  OP_js,  OP_jns,
  OP_jp,  OP_jpe, OP_jnp,  OP_jpo, OP_jl,   OP_jnge, OP_jnl, OP_jge, OP_jle,
  OP_jng,  OP_jnle, OP_jg,

  // jcxz vs. jecxz is chosen on the basis of the address size prefix.
  OP_jcxz, OP_jecxz, OP_jecxz, OP_jrcxz,

  // loop variants
  OP_loop, OP_loopz, OP_loope, OP_loopnz, OP_loopne
};

bool AsmInstruction::HasFallThrough() const {
  // TODO(martint): Get this info from the i386_insn structure
  //                or complete the list
  if (IsReturn()) return false;
  if (!HasTarget()) return true;
  if (IsCall()) return true;
  if (IsInList(op(), cond_jumps, sizeof(cond_jumps)/sizeof(MaoOpcode))) {
    return true;
  }

  return false;
}

bool AsmInstruction::HasTarget() const {
  // TODO(martint): Get this info from the i386_insn structure
  //                or complete the list
  const MaoOpcode insn[] = {OP_jmp, OP_ljmp};
  if (IsInList(op(), insn, sizeof(insn)/sizeof(MaoOpcode)))
    return true;
  if (IsInList(op(), cond_jumps, sizeof(cond_jumps)/sizeof(MaoOpcode)))
    return true;

  return false;
}

const char *AsmInstruction::GetTarget() const {
  //
  for (unsigned int i =0; i < instruction_->operands; i++) {
    if (IsMemOperand(instruction_, i)) {
      // symbol?
      if (instruction_->types[i].bitfield.disp8 ||
          instruction_->types[i].bitfield.disp16 ||
          instruction_->types[i].bitfield.disp32 ||
          instruction_->types[i].bitfield.disp32s ||
          instruction_->types[i].bitfield.disp64) {
        if (instruction_->op[i].disps->X_op == O_symbol) {
          return S_GET_NAME(instruction_->op[i].disps->X_add_symbol);
        }
      }
    }
  }
  return "<UKNOWN>";
}

bool AsmInstruction::IsCall() const {
  const MaoOpcode calls[] = {
    OP_call, OP_lcall, OP_vmcall, OP_syscall, OP_vmmcall
  };
  return IsInList(op(), calls, sizeof(calls)/sizeof(MaoOpcode));
}

bool AsmInstruction::IsReturn() const {
  const MaoOpcode rets[] = {
    OP_ret, OP_lret, OP_retf, OP_iret, OP_sysret
  };
  return IsInList(op(), rets, sizeof(rets)/sizeof(MaoOpcode));
}


//
// Class: MaoUnitEntryBase
//

MaoUnitEntryBase::MaoUnitEntryBase(unsigned int line_number,
                                   const char *line_verbatim) :
    line_number_(line_number),
    index_(0) {
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

void LabelEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(name_);
  fprintf(out, "%s:", name_);
  fprintf(out, "\t # [%d]\t%s", line_number(),
          line_verbatim() ? line_verbatim() : "");
  fprintf(out, "\n");
}

void LabelEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(name_);
  fprintf(out, "%s", name_);
}


//
// Class: DirectiveEntry
//

const char *const DirectiveEntry::kOpcodeNames[NUM_OPCODES] = {
  ".file",
  ".section",
  ".globl",
  ".local",
  ".weak",
  ".type",
  ".size",
  ".byte",
  ".word",
  ".long",
  ".quad",
  ".rva",
  ".ascii",
  ".string",
  ".string16",
  ".string32",
  ".string64",
  ".sleb128",
  ".uleb128",
  ".p2align",
  ".p2alignw",
  ".p2alignl",
  ".space",
  ".ds.b",
  ".ds.w",
  ".ds.l",
  ".ds.d",
  ".ds.x",
  ".comm",
  ".ident",
};

void DirectiveEntry::PrintEntry(::FILE *out) const {
  std::string operands;
  fprintf(out, "\t%s\t%s", GetOpcodeName(),
          OperandsToString(&operands).c_str());
  fprintf(out, "\t # [%d]\t%s", line_number(),
          line_verbatim() ? line_verbatim() : "");
  fprintf(out, "\n");
}

void DirectiveEntry::PrintIR(::FILE *out) const {
  std::string operands;
  fprintf(out, "%s %s", GetOpcodeName(), OperandsToString(&operands).c_str());
}

const std::string &DirectiveEntry::OperandsToString(std::string *out) const {
  for (OperandVector::const_iterator iter = operands_.begin();
       iter != operands_.end(); ++iter) {
    if (iter != operands_.begin())
      out->append(", ");
    OperandToString(**iter, out);
  }

  return *out;
}

const char *DirectiveEntry::GetDotOrSymbol(symbolS *symbol) const {
  const char *s = S_GET_NAME(symbol);
  MAO_ASSERT(s);
  if (strcmp(s, "L0\001") == 0) {
    return ".";
  } else {
    return s;
  }
}

const std::string &DirectiveEntry::OperandExpressionToString(
    const expressionS *expr, std::string *out) const {
  switch (expr->X_op) {
    // SUPPORTED
      /* X_add_number (a constant expression).  */
    case O_constant:
      {
        std::ostringstream int_string;
        int_string << expr->X_add_number;
        out->append(int_string.str());
      }
      break;
      /* X_add_symbol + X_add_number.  */
    case O_symbol:
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol)
                     << "+";
        }
        exp_string << expr->X_add_number;
        out->append(exp_string.str());
      }
      break;
      /* (X_add_symbol + X_op_symbol) + X_add_number.  */
    case O_add:
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol)
                     << "+";
        }
        if (expr->X_op_symbol) {
          exp_string << GetDotOrSymbol(expr->X_op_symbol)
                     << "+";
        }
        exp_string << expr->X_add_number;
        out->append(exp_string.str());
      }
      break;
      /* (X_add_symbol - X_op_symbol) + X_add_number.  */
    case O_subtract:
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol)
                     << "-";
        }
        if (expr->X_op_symbol) {
          exp_string << GetDotOrSymbol(expr->X_op_symbol)
                     << "+";
        }
        exp_string << expr->X_add_number;
        out->append(exp_string.str());
      }
      break;

    // UNSUPPORTED
    /* An illegal expression.  */
    case O_illegal:
      /* A nonexistent expression.  */
    case O_absent:
      /* X_add_symbol + X_add_number - the base address of the image.  */
    case O_symbol_rva:
      /* A register (X_add_number is register number).  */
    case O_register:
      /* A big value.  If X_add_number is negative or 0, the value is in
         generic_floating_point_number.  Otherwise the value is in
         generic_bignum, and X_add_number is the number of LITTLENUMs in
         the value.  */
    case O_big:
      /* (- X_add_symbol) + X_add_number.  */
    case O_uminus:
      /* (~ X_add_symbol) + X_add_number.  */
    case O_bit_not:
      /* (! X_add_symbol) + X_add_number.  */
    case O_logical_not:
      /* (X_add_symbol * X_op_symbol) + X_add_number.  */
    case O_multiply:
      /* (X_add_symbol / X_op_symbol) + X_add_number.  */
    case O_divide:
      /* (X_add_symbol % X_op_symbol) + X_add_number.  */
    case O_modulus:
      /* (X_add_symbol << X_op_symbol) + X_add_number.  */
    case O_left_shift:
      /* (X_add_symbol >> X_op_symbol) + X_add_number.  */
    case O_right_shift:
      /* (X_add_symbol | X_op_symbol) + X_add_number.  */
    case O_bit_inclusive_or:
      /* (X_add_symbol |~ X_op_symbol) + X_add_number.  */
    case O_bit_or_not:
      /* (X_add_symbol ^ X_op_symbol) + X_add_number.  */
    case O_bit_exclusive_or:
      /* (X_add_symbol & X_op_symbol) + X_add_number.  */
    case O_bit_and:
      /* (X_add_symbol == X_op_symbol) + X_add_number.  */
    case O_eq:
      /* (X_add_symbol != X_op_symbol) + X_add_number.  */
    case O_ne:
      /* (X_add_symbol < X_op_symbol) + X_add_number.  */
    case O_lt:
      /* (X_add_symbol <= X_op_symbol) + X_add_number.  */
    case O_le:
      /* (X_add_symbol >= X_op_symbol) + X_add_number.  */
    case O_ge:
      /* (X_add_symbol > X_op_symbol) + X_add_number.  */
    case O_gt:
      /* (X_add_symbol && X_op_symbol) + X_add_number.  */
    case O_logical_and:
      /* (X_add_symbol || X_op_symbol) + X_add_number.  */
    case O_logical_or:
      /* X_op_symbol [ X_add_symbol ] */
    case O_index:
      /* machine dependent operators */
    case O_md1:
    case O_md2:
    case O_md3:
    case O_md4:
    case O_md5:
    case O_md6:
    case O_md7:
    case O_md8:
    case O_md9:
    case O_md10:
    case O_md11:
    case O_md12:
    case O_md13:
    case O_md14:
    case O_md15:
    case O_md16:
    case O_md17:
    case O_md18:
    case O_md19:
    case O_md20:
    case O_md21:
    case O_md22:
    case O_md23:
    case O_md24:
    case O_md25:
    case O_md26:
    case O_md27:
    case O_md28:
    case O_md29:
    case O_md30:
    case O_md31:
    case O_md32:
  /* this must be the largest value */
    case O_max:
    default:
      MAO_ASSERT_MSG(
          0,
          "OperandExpressionToString does not support the symbol %d\n",
          expr->X_op);
      break;
  }
  return *out;
}

const std::string &DirectiveEntry::OperandToString(const Operand &operand,
                                                   std::string *out) const {
  switch (operand.type) {
    case NO_OPERAND:
      break;
    case STRING:
      out->append(*operand.data.str);
      break;
    case INT: {
      std::ostringstream int_string;
      int_string << operand.data.i;
      out->append(int_string.str());
      break;
    }
    case SYMBOL:
      out->append(S_GET_NAME(operand.data.sym));
      break;
    case EXPRESSION:
      OperandExpressionToString(operand.data.expr, out);
      break;
    case EMPTY_OPERAND:
      // Nothing to do
      break;
    default:
      MAO_ASSERT(false);
  }
  return *out;
}

MaoUnitEntryBase::EntryType DirectiveEntry::Type() const {
  return DIRECTIVE;
}


//
// Class: DebugEntry
//

void DebugEntry::PrintEntry(FILE *out) const {
  fprintf(out, "\t%s\t%s", key_.c_str(), value_.c_str());
  fprintf(out, "\t # [%d]\t%s", line_number(),
          line_verbatim() ? line_verbatim() : "");
  fprintf(out, "\n");
}

void DebugEntry::PrintIR(FILE *out) const {
  fprintf(out, "%s %s", key_.c_str(), value_.c_str());
}


MaoUnitEntryBase::EntryType DebugEntry::Type() const {
  return DEBUG;
}


//
// Class: InstructionEntry
//

InstructionEntry::InstructionEntry(i386_insn *instruction,
                                   unsigned int line_number,
                                   const char* line_verbatim) :
    MaoUnitEntryBase(line_number, line_verbatim) {
  instruction_ = new AsmInstruction(instruction,
                                    GetOpcode(instruction->tm.name));
}

InstructionEntry::~InstructionEntry() {
  delete instruction_;
}

void InstructionEntry::PrintEntry(FILE *out) const {
  instruction_->PrintInstruction(out);
  fprintf(out, "\t # [%d]\t%s", line_number(),
          line_verbatim() ? line_verbatim() : "");
  fprintf(out, "\n");
}


void InstructionEntry::PrintIR(FILE *out) const {
  MAO_ASSERT(instruction_);
  instruction_->PrintInstruction(out);
}


MaoUnitEntryBase::EntryType InstructionEntry::Type() const {
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
