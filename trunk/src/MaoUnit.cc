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

#include <assert.h>

#include "MaoUnit.h"

extern "C" const char *S_GET_NAME(symbolS *s);

//
// Class: MaoUnit
//

MaoUnit::MaoUnit() {
  // Default to no subsection selected
  // A default will be generetaed if necessary later on.
  current_subsection_ = 0;
  symbol_table_.Init();
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
}

void MaoUnit::PrintMaoUnit() const {
  PrintMaoUnit(stdout);
}

// Prints all entries in the MaoUnit. Starts with the
// ones not associated with any section, then the rest by
// iterating over the subsections.
void MaoUnit::PrintMaoUnit(FILE *out) const {
  for (std::vector<MaoUnitEntryBase *>::const_iterator iter =
           entries_.begin();
      iter != entries_.end(); ++iter) {
    MaoUnitEntryBase *e = *iter;
    e->PrintEntry(out);
  }
  PrintSubSections(out);
}

void MaoUnit::PrintIR() const {
  PrintIR(stdout);
}

void MaoUnit::PrintIR(FILE *out) const {
  fprintf(out, "<ir>\n");
  // start with the subsection-less entries:
  for (std::vector<MaoUnitEntryBase *>::const_iterator e_iter =
           entries_.begin();
      e_iter != entries_.end(); ++e_iter) {
    MaoUnitEntryBase *e = *e_iter;
    e->PrintIR(2, out);
  }
  // Now print each subsection
  for (std::vector<SubSection *>::const_iterator ss_iter =
           sub_sections_.begin(); ss_iter != sub_sections_.end(); ++ss_iter) {
    SubSection *ss = *ss_iter;
    fprintf(out, "  <subsection %s %d>\n", ss->name(), ss->number());
    // loop through the subsections here!
    ss->PrintIR(4, out);
    fprintf(out, "  </subsection>\n");
  }
  fprintf(out, "</ir>\n");
}


void MaoUnit::PrintSubSections() const {
  PrintSubSections(stdout);
}

// Prints the entries found in all the subsections. The order is important
// since keepign the symbol-order as the orignal assembly file simplifies
// verification.
void MaoUnit::PrintSubSections(FILE *out) const {
  for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    SubSection *ss = *iter;
    fprintf(out, " # section : %s %s\n", ss->name(), ss->creation_op());
    fprintf(out, "%s\n", ss->creation_op());
    ss->PrintEntries(out, (SymbolTable *)&symbol_table_);
  }
  return;
}

Section *MaoUnit::FindOrCreateAndFind(const char *section_name) {
  Section *section;
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name);
  if (it == sections_.end()) {
    // creapte it!
    section = new Section(section_name);
    sections_[section->name()] = section;
  } else {
    section = it->second;
  }
  assert(section);
  return section;
}

// Called when found a new subsection reference in the assembly.
void MaoUnit::SetSubSection(const char *section_name,
                            unsigned int subsection_number,
                            const char *creation_op) {
  // Get (and possibly create) the section
  Section *section = FindOrCreateAndFind(section_name);
  assert(section);
  // Create a new subsection, even if the same subsection number
  // have already been used. This allows the program to be MaoUnit
  // to use a list of SubSections and still keep the symbol
  // order of all the symbols.
  SubSection *subsection = new SubSection(subsection_number, section,
                                          creation_op);
  sub_sections_.push_back(subsection);

  // set current_subsection!
  current_subsection_ = subsection;
}

// Add an Instruction entry to the MaoUnit list
bool MaoUnit::AddEntry(MaoUnitEntryBase *entry, bool create_default_section) {
  // Instructions that _might_ get used.
  LabelEntry *label_entry;
  Symbol *symbol;

  assert(entry);

  // Create a subsection if necessary
  if (create_default_section && !current_subsection_) {
    SetSubSection(DEFAULT_SECTION_NAME, 0, DEFAULT_SECTION_CREATION_OP);
    assert(current_subsection_);
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
      assert(symbol);
      symbol->set_section_name(current_subsection_->name());
      break;
    case MaoUnitEntryBase::DEBUG:
      break;
    case MaoUnitEntryBase::DIRECTIVE:
      break;
    case MaoUnitEntryBase::UNDEFINED:
      break;
    default:
      // should never happen. Catch all cases above.
      assert(0);
  }

  // Add the entry to the compilation unit
  if (!current_subsection_) {
    entries_.push_back(entry);
  } else {
    current_subsection_->AddEntry(entry);
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
    assert(symbol->size() < symbol->common_size());
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
  assert(instruction);
  instruction_ = CreateInstructionCopy(instruction);
}

AsmInstruction::~AsmInstruction() {
  assert(instruction_);
  FreeInstruction();
}

const char *AsmInstruction::get_op() const {
  assert(instruction_);
  assert(instruction_->tm.name);
  return(instruction_->tm.name);
}

// This deallocates memory allocated in CreateInstructionCopy().
void AsmInstruction::FreeInstruction() {
  assert(instruction_);
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
  assert(tmp_r);
  tmp_r->reg_name = strndup(in_reg->reg_name, kMaxRegisterNameLength);
  tmp_r->reg_type = in_reg->reg_type;
  tmp_r->reg_flags = in_reg->reg_flags;
  tmp_r->reg_num = in_reg->reg_num;
  return tmp_r;
}


bool AsmInstruction::IsMemOperand(const i386_insn *instruction,
                                  const unsigned int op_index) {
  assert(instruction->operands > op_index);
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
  assert(instruction->operands > op_index);
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
  assert(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.reg8
          || t.bitfield.reg16
          || t.bitfield.reg32
          || t.bitfield.reg64);
}

// Prints out the instruction. This is work-in-progress, but currently
// supports the assembly instructions found in the assembled version
// of mao. Please add functionatlity when unsupported instructions are found.
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
  assert(new_inst);

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
      assert(new_inst->op[i].imms);
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
        assert(new_inst->op[i].disps);
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
      assert(tmp_seg);
      tmp_seg->seg_name = strndup(in_inst->seg[i]->seg_name,
                                  MAX_SEGMENT_NAME_LENGTH);
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

//
// Class: Directive
//
Directive::Directive(const char *key, const char *value) {
  assert(key);
  assert(value);
  key_ = strndup(key, MAX_DIRECTIVE_NAME_LENGTH);
  value_ = strndup(value, MAX_DIRECTIVE_NAME_LENGTH);
}

Directive::~Directive() {
  assert(key_);
  free(key_);
  assert(value_);
  free(value_);
}

const char *Directive::key() {
  assert(key_);
  return key_;
};

const char *Directive::value() {
  assert(value_);
  return value_;
}

//
// Class: Label
//

Label::Label(const char *name) {
  assert(name);
  name_ = strndup(name, MAX_SYMBOL_NAME_LENGTH);
}

Label::~Label() {
  assert(name_);
  free(name_);
}

const char *Label::name() const {
  assert(name_);
  return name_;
}

//
// Class: MaoUnitEntryBase
//

MaoUnitEntryBase::MaoUnitEntryBase(unsigned int line_number,
                                   const char *line_verbatim) :
    line_number_(line_number) {
  if (line_verbatim) {
    line_verbatim_ = strndup(line_verbatim,
                             MAX_VERBATIM_ASSEMBLY_STRING_LENGTH);
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
  assert(label_);
  fprintf(out, "%s:", label_->name() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void LabelEntry::PrintIR(unsigned int indent_level, FILE *out) const {
  assert(label_);
  Spaces(indent_level, out);
  fprintf(out, "<label>%s</label>\n", label_->name() );
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
  assert(directive_);
  fprintf(out, "\t%s\t%s", directive_->key(), directive_->value() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void DirectiveEntry::PrintIR(unsigned int indent_level, FILE *out) const {
  assert(directive_);
  Spaces(indent_level, out);
  fprintf(out, "<directive> %s %s </directive>\n", directive_->key(),
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
  assert(directive_);
  fprintf(out, "\t%s\t%s", directive_->key(), directive_->value() );
  fprintf(out, "\t # [%d]\t%s", line_number_, line_verbatim_?line_verbatim_:"");
  fprintf(out, "\n");
}

void DebugEntry::PrintIR(unsigned int indent_level, FILE *out) const {
  assert(directive_);
  Spaces(indent_level, out);
  fprintf(out, "<debug>%s %s</debug>\n", directive_->key(),
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


void InstructionEntry::PrintIR(unsigned int indent_level, FILE *out) const {
  assert(instruction_);
  Spaces(indent_level, out);
  fprintf(out, "<instruction>%s</instruction>\n", instruction_->get_op());
}


MaoUnitEntryBase::EntryType InstructionEntry::entry_type() const {
  return INSTRUCTION;
}

//
// Class: Section
//

Section::Section(const char *name) {
  assert(name);
  name_ = strndup(name, MAX_SYMBOL_NAME_LENGTH);
}

const char *Section::name() const {
  assert(name_);
  return name_;
}

Section::~Section() {
  assert(name_);
  free(name_);
}

//
// Class: SubSection
//

SubSection::SubSection(unsigned int subsection_number, Section *section,
                       const char *creation_op) :
    number_(subsection_number),
    section_(section) {
  assert(section_);
  assert(creation_op);
  creation_op_ = strdup(creation_op);  // this way free() works
}

const char *SubSection::name() const {
  assert(section_);
  return section_->name();
}

unsigned int SubSection::number() const {
  return number_;
}


// Add an Instruction entry to the MaoUnit list
bool SubSection::AddEntry(MaoUnitEntryBase *entry) {
  entries_.push_back(entry);
  return true;
}

void SubSection::PrintEntries(FILE *out, SymbolTable *symbol_table) const {
  for (std::list<MaoUnitEntryBase *>::const_iterator iter = entries_.begin();
       iter != entries_.end(); ++iter) {
    MaoUnitEntryBase *pe = *iter;
    pe->PrintEntry(out);
  }
}

void SubSection::PrintIR(unsigned int indent_level, FILE *out) const {
  for (std::list<MaoUnitEntryBase *>::const_iterator iter = entries_.begin();
       iter != entries_.end(); ++iter) {
    MaoUnitEntryBase *pe = *iter;
    pe->PrintIR(indent_level, out);
  }
}


const char *SubSection::creation_op() const {
  assert(creation_op_);
  return creation_op_;
}

SubSection::~SubSection() {
  // clear the entries!
  for (std::list<MaoUnitEntryBase *>::iterator iter = entries_.begin();
       iter != entries_.end(); ++iter) {
    delete *iter;
  }
  assert(creation_op_);
  free(creation_op_);
}
