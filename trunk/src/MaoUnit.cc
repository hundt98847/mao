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

#include <iostream>
#include <sstream>
#include <string>

#include "mao.h"
#include "MaoCFG.h"
#include "MaoDebug.h"
#include "MaoEntry.h"
#include "MaoRelax.h"
#include "MaoUnit.h"

#include "ir.h"
#include "struc-symbol.h"

#include "tc-i386-helper.h"


//
// Class: MaoUnit
//

// Default to no subsection selected
// A default will be generated if necessary later on.
MaoUnit::MaoUnit(MaoOptions *mao_options)
    : arch_(UNKNOWN), current_subsection_(0), mao_options_(mao_options) {
  entry_vector_.clear();
  sub_sections_.clear();
  sections_.clear();
  functions_.clear();
  entry_to_function_.clear();
  entry_to_subsection_.clear();
}

MaoUnit::~MaoUnit() {
  // Remove subsections and free allocated memory
  for (std::vector<SubSection *>::iterator iter =
           sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    delete *iter;
  }

  // Remove functions and free allocated memory
  for (MaoUnit::ConstFunctionIterator iter = ConstFunctionBegin();
      iter != ConstFunctionEnd();
      ++iter) {
    delete *iter;
  }

  // Remove sections and free allocated memory
  for (std::map<const char *, Section *, ltstr>::const_iterator iter =
           sections_.begin();
       iter != sections_.end(); ++iter) {
    delete iter->second;
  }

  // Remove entries and free allocated memory
  for (VectorEntryIterator iter = entry_vector_.begin();
       iter != entry_vector_.end();
       ++iter) {
    delete *iter;
  }
}

void MaoUnit::SetDefaultArch() {
  const char *arch_string = get_default_arch();
  if (strcmp(arch_string, "i386") == 0 || strcmp(arch_string, "I386") == 0) {
    arch_ = I386;
  } else if (strcmp(arch_string, "x86_64") == 0 ||
             strcmp(arch_string, "X86_64") == 0) {
    arch_ = X86_64;
  } else {
    MAO_ASSERT_MSG(false, "Unsupported architecture: %s", arch_string);
  }
}

void MaoUnit::PrintMaoUnit() const {
  PrintMaoUnit(stdout);
}

class EntryDumper : public MaoDebugAction {
 public:
  EntryDumper() {}

  void set_entry(MaoEntry *e) { entry_ = e; }

  virtual void Invoke(FILE *outfile) {
    fprintf(outfile, "***   generating asm for entry [%d], line: %d\n",
            entry_->id(), entry_->line_number());
  }

 private:
  MaoEntry *entry_;
};


// Prints all entries in the MaoUnit
void MaoUnit::PrintMaoUnit(FILE *out) const {
  EntryDumper entry_dumper;
  for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
       iter != sub_sections_.end(); ++iter) {
    SubSection *ss = *iter;
    for (EntryIterator e_iter = ss->EntryBegin();
         e_iter != ss->EntryEnd();
         ++e_iter) {
      MaoEntry *e = *e_iter;
      entry_dumper.set_entry(e);
      e->PrintEntry(out);
    }
  }
}

void MaoUnit::PrintIR(bool print_entries, bool print_sections,
                      bool print_subsections, bool print_functions) const {
  PrintIR(stdout, print_entries, print_sections, print_subsections);
}


const char *MaoUnit::FunctionName(MaoEntry *entry) const {
  const char *function = "";
  if (InFunction(entry)) {
    function = (*(entry_to_function_.find(entry))).second->name().c_str();
  }
  return function;
}

const char *MaoUnit::SectionName(MaoEntry *entry) const {
  const char *section = "";
  if (InSubSection(entry)) {
    SubSection *ss = (*(entry_to_subsection_.find(entry))).second;
    section = ss->section()->name().c_str();
  }
  return section;
}

void MaoUnit::PrintIR(FILE *out, bool print_entries, bool print_sections,
                      bool print_subsections, bool print_functions) const {
  if (print_entries) {
    // Print the entries
    for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
         iter != sub_sections_.end(); ++iter) {
      SubSection *ss = *iter;
      for (EntryIterator e_iter = ss->EntryBegin();
           e_iter != ss->EntryEnd();
           ++e_iter) {
        MaoEntry *e = *e_iter;
        fprintf(out, "[%5d][%c][%14s][%10s] ", e->id(), e->GetDescriptiveChar(),
                FunctionName(e), SectionName(e));
        if (MaoEntry::INSTRUCTION == e->Type()) {
          fprintf(out, "\t");
        }
        e->PrintIR(out);
        fprintf(out, "\n");
      }
    }
  }

  if (print_sections) {
    // Print the sections
    fprintf(out, "Sections : \n");
    for (ConstSectionIterator iter = ConstSectionBegin();
         iter != ConstSectionEnd();
         ++iter) {
      Section *section = *iter;
      fprintf(out, "[%3d] %s [", section->id(), section->name().c_str());
      const std::vector<SubSectionID> subsectionIDs =
          section->GetSubsectionIDs();
      for (std::vector<SubSectionID>::const_iterator ss_iter =
               subsectionIDs.begin();
           ss_iter != subsectionIDs.end();
           ++ss_iter) {
        fprintf(out, " %d", *ss_iter);
      }
      fprintf(out, "]\n");
    }
  }

  if (print_subsections) {
    // Print the subsections
    fprintf(out, "Subsections : \n");
    for (std::vector<SubSection *>::const_iterator iter = sub_sections_.begin();
         iter != sub_sections_.end(); ++iter) {
      SubSection *ss = *iter;
      fprintf(out, "[%3d] [%d-%d]: %s\n", ss->id(),
              ss->first_entry()->id(), ss->last_entry()->id(),
              ss->name().c_str());
    }
  }

  if (print_functions) {
    // Print the functions
    fprintf(out, "Functions : \n");
    for (MaoUnit::ConstFunctionIterator iter = ConstFunctionBegin();
         iter != ConstFunctionEnd();
         ++iter) {
      Function *function = *iter;
      CFG *cfg = CFG::GetCFGIfExists(this, function);
      fprintf(out, "[%3d] [%3d-%3d] [%c%c]: %s \n",
              function->id(),
              function->first_entry()->id(),
              function->last_entry()->id(),
              cfg ? (cfg->HasUnresolvedIndirectJump() ? 'I' : ' ') : '?',
              cfg ? (cfg->HasExternalJump() ? 'E' : ' ') : '?',
              function->name().c_str());
    }
  }
}

Section *MaoUnit::GetSection(const std::string &section_name) const {
  Section *section = NULL;
  // See if section already have been created.
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name.c_str());
  if (it != sections_.end()) {
    section = it->second;
  }
  return section;
}

std::pair<bool, Section *> MaoUnit::FindOrCreateAndFind(
    const char *section_name) {
  bool new_section = false;
  Section *section;
  // See if section already have been created.
  std::map<const char *, Section *, ltstr>::const_iterator it =
      sections_.find(section_name);
  if (it == sections_.end()) {
    // Create it.
    // TODO(martint): Use an ID factory for the ID
    section = new Section(section_name, sections_.size());
    sections_[section->name().c_str()] = section;
    new_section = true;
  } else {
    section = it->second;
  }
  MAO_ASSERT(section);
  return std::make_pair(new_section, section);
}

SubSection *MaoUnit::AddNewSubSection(const char *section_name,
                                      unsigned int subsection_number,
                                      MaoEntry *entry) {
  // Get (and possibly create) the section
  std::pair<bool, Section *> section_pair = FindOrCreateAndFind(section_name);
  Section *section = section_pair.second;
  MAO_ASSERT(section);
  // Create a new subsection
  // TODO(martint): create ID factory
  SubSection *subsection = new SubSection(sub_sections_.size(),
                                          subsection_number, section_name,
                                          section);

  // Makes it possible to link entries between subsections
  SubSection *last_subsection = section->GetLastSubSection();

  sub_sections_.push_back(subsection);
  section->AddSubSection(subsection);

  // Assume that the section is only one entry long.
  // last_entry is updated as we add entries..
  subsection->set_first_entry(entry);
  subsection->set_last_entry(entry);

  // Now we should check if we should link this entry back to the previous
  // subsection within this section!
  if (last_subsection) {
    MaoEntry *last_entry = last_subsection->last_entry();
    last_entry->set_next(entry);
    entry->set_prev(last_entry);
  }

  return subsection;
}


LabelEntry *MaoUnit::GetLabelEntry(const char *label_name) const {
  std::map<const char *, LabelEntry *>::const_iterator iter =
      labels_.find(label_name);
  if (iter == labels_.end()) {
    return NULL;
  } else {
    return iter->second;
  }
}


static struct insn_template FindTemplate(const char *opcode,
                                         unsigned int base_opcode) {
  for (int i = 0;; ++i) {
    if (!i386_optab[i].name)
      break;
    if (base_opcode == i386_optab[i].base_opcode)
      if (!strcasecmp(opcode, i386_optab[i].name))
        return i386_optab[i];
  }
  MAO_ASSERT_MSG(false, "Couldn't find instruction template for: %s", opcode);
  return i386_optab[0];  // Should never happen;
}

InstructionEntry *MaoUnit::CreateInstruction(i386_insn *instruction,
                                             Function *function) {
  enum flag_code flag;
  switch (arch_) {
    case X86_64:
      flag = CODE_64BIT;
      break;
    case I386:
      flag = CODE_32BIT;
      break;
    default:
      MAO_RASSERT_MSG(false, "Unable to match code flag to architecture");
      return NULL;  // Quells a warning about flag used without being defined
  }

  InstructionEntry *e = new InstructionEntry(instruction, flag, 0, NULL, this);
  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  e->set_id(entry_index);

  // Add the entry to the compilation unit
  entry_vector_.push_back(e);

  SubSection *subsection = function->GetSubSection();
  if (function) {
    entry_to_function_[e] = function;
  }
  if (subsection) {
    entry_to_subsection_[e] = subsection;
  }
  return e;
}

// Note that the base_opcode for instructions can be found in:
//
//    ~/mao/binutils-2.21.1/opcodes/i36-opc.tbl
//
InstructionEntry *MaoUnit::CreateInstruction(const char *opcode_str,
                                             unsigned int base_opcode,
                                             Function *function) {
  i386_insn insn;
  memset(&insn, 0, sizeof(i386_insn));
  insn.tm = FindTemplate(opcode_str, base_opcode);

  enum flag_code flag;
  switch (arch_) {
    case X86_64:
      flag = CODE_64BIT;
      break;
    case I386:
      flag = CODE_32BIT;
      break;
    default:
      MAO_RASSERT_MSG(false, "Unable to match code flag to architecture");
      return NULL;  // Quells a warning about flag used without being defined
  }

  InstructionEntry *e = new InstructionEntry(&insn, flag, 0, NULL, this);
  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  e->set_id(entry_index);

  // Add the entry to the compilation unit
  entry_vector_.push_back(e);

  SubSection *subsection = function->GetSubSection();
  if (function) {
    entry_to_function_[e] = function;
  }
  if (subsection) {
    entry_to_subsection_[e] = subsection;
  }
  return e;
}

InstructionEntry *MaoUnit::CreateAdd(Function *function) {
  InstructionEntry *e = CreateInstruction("add", 0x83, function);

  e->set_op(OP_add);

  return e;
}

InstructionEntry *MaoUnit::CreateSub(Function *function) {
  InstructionEntry *e = CreateInstruction("sub", 0x83, function);

  e->set_op(OP_sub);

  return e;
}


InstructionEntry *MaoUnit::CreateNop(Function *function) {
  InstructionEntry *e = CreateInstruction("nop", 0x90, function);

  e->set_op(OP_nop);

  return e;
}

InstructionEntry *MaoUnit::Create2ByteNop(Function *function) {
  #define WORD_MNEM_SUFFIX  'w'
  InstructionEntry *e = CreateInstruction("xchg", 0x90, function);

  i386_insn *insn = e->instruction();
  insn->operands = 2;
  insn->reg_operands = 2;

  insn->suffix = WORD_MNEM_SUFFIX;

  insn->types[0].bitfield.acc = 1;
  insn->types[1].bitfield.acc = 1;

  const reg_entry *r = GetRegFromName("ax");
  insn->op[0].regs = r;
  insn->op[1].regs = r;
  insn->types[0] = r->reg_type;
  insn->types[1] = r->reg_type;

  e->set_op(OP_nop);

  return e;
}

InstructionEntry *MaoUnit::CreateLock(Function *function) {
  InstructionEntry *e = CreateInstruction("lock", 0xf0, function);

  e->set_op(OP_lock);
  return e;
}

static const char *prefetch_opcode_strings[] = {
  "prefetchnta",
  "prefetcht0",
  "prefetcht1",
  "prefetcht2"
};

static const MaoOpcode prefetch_opcodes[] = {
  OP_prefetchnta,
  OP_prefetcht0,
  OP_prefetcht1,
  OP_prefetcht2
};

InstructionEntry *MaoUnit::CreatePrefetch(Function *function,
                                          int type,
                                          InstructionEntry *insn,
                                          int op_index,
                                          int offset) {
  MAO_ASSERT(type >= 0 && type <= 3);
  InstructionEntry *e = CreateInstruction(
    prefetch_opcode_strings[type], 0xf18, function);

  e->set_op(prefetch_opcodes[type]);

  i386_insn *in_insn = e->instruction();
  in_insn->operands = 1;
  in_insn->mem_operands = 1;

  // Set address expression from insn operand op
  e->SetOperand(0, insn, op_index);
  if (insn->HasPrefix(ADDR_PREFIX_OPCODE))
    e->AddPrefix(ADDR_PREFIX_OPCODE);
  if (offset && in_insn->op[0].disps)
    in_insn->op[0].disps->X_add_number += offset;

  return e;
}


InstructionEntry *MaoUnit::CreateUncondJump(LabelEntry *label,
                                            Function *function) {
  InstructionEntry *e = CreateInstruction("jmp", 0xeb, function);
  expressionS *disp_expression =
      static_cast<expressionS *>(xmalloc(sizeof(expressionS)));
  symbolS *symbolP;

  i386_insn *insn = e->instruction();
  insn->operands = 1;
  insn->disp_operands = 1;
  insn->mem_operands = 1;

  // Set the displacement fileds
  insn->types[0].bitfield.disp32 = 1;
  insn->types[0].bitfield.disp32s = 1;

  for (int j = 0; j < MAX_OPERANDS; j++)
    insn->reloc[j] = NO_RELOC;

  symbolP = symbol_find_or_make(label->name());

  disp_expression->X_op = O_symbol;
  disp_expression->X_add_symbol = symbolP;
  disp_expression->X_add_number = 0;

  insn->op[0].disps = disp_expression;



  e->set_op(OP_jmp);

  return e;
}

InstructionEntry *MaoUnit::CreateIncFromOperand(Function *function,
                                                InstructionEntry *insn2,
                                                int op2) {
  InstructionEntry *e = CreateInstruction("inc", 0xfe, function);
  e->SetOperand(0, insn2, op2);
  e->instruction()->operands++;
  e->set_op(OP_inc);

  return e;
}

InstructionEntry *MaoUnit::CreateDecFromOperand(Function *function,
                                                InstructionEntry *insn2,
                                                int op2) {
  InstructionEntry *e = CreateInstruction("dec", 0xfe, function);
  e->SetOperand(0, insn2, op2);
  e->instruction()->operands++;
  e->set_op(OP_dec);

  return e;
}

LabelEntry *MaoUnit::CreateLabel(const char *labelname,
                                 Function *function,
                                 SubSection *subsection) {
  LabelEntry *l = new LabelEntry(labelname, 0, NULL, this);

  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  l->set_id(entry_index);

  // Add the entry to the compilation unit
  entry_vector_.push_back(l);

  if (function) {
    entry_to_function_[l] = function;
  }
  if (subsection) {
    entry_to_subsection_[l] = subsection;
  }

  return l;
}

DirectiveEntry *MaoUnit::CreateDirective(
    DirectiveEntry::Opcode op,
    const DirectiveEntry::OperandVector &operands,
    Function *function,
    SubSection *subsection) {
  DirectiveEntry *directive =
      new DirectiveEntry(op, operands,
                         0, NULL, this);
  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  directive->set_id(entry_index);

  // Add the entry to the compilation unit
  entry_vector_.push_back(directive);

  if (function) {
    entry_to_function_[directive] = function;
  }
  if (subsection) {
    entry_to_subsection_[directive] = subsection;
  }

  return directive;
}

// Add an entry to the MaoUnit list
bool MaoUnit::AddEntry(MaoEntry *entry,
                       bool  create_default_section) {
  // Variables that _might_ get used.
  LabelEntry *label_entry;
  DirectiveEntry *directive_entry;
  Symbol *symbol;

  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();

  MAO_ASSERT(entry);
  entry->set_id(entry_index);

  // Check if we should add a new section for the first directives.
  if (!current_subsection_ &&
      !create_default_section) {
    // Make sure the entry does not create a section
    DirectiveEntry *de = entry->IsDirective()?entry->AsDirective():NULL;
    if (de == NULL || de->op() != DirectiveEntry::SECTION) {
      prev_subsection_ = current_subsection_;
      current_subsection_ = AddNewSubSection("mao_start_section", 0,  entry);
      current_subsection_->set_start_section(true);
    }
  }
  // Create a subsection if necessary
  if (create_default_section &&
      (!current_subsection_ || current_subsection_->start_section())) {
    prev_subsection_ = current_subsection_;
    current_subsection_ = AddNewSubSection(DEFAULT_SECTION_NAME, 0, entry);
    MAO_ASSERT(current_subsection_);
  }

  // Check the type
  switch (entry->Type()) {
    case MaoEntry::INSTRUCTION:
      break;
    case MaoEntry::LABEL:
      // A Label will generate in a new symbol in the symbol table
      label_entry = static_cast<LabelEntry *>(entry);
      MAO_ASSERT(labels_.insert(std::make_pair(label_entry->name(),
                                               label_entry)).second);
      symbol = symbol_table_.FindOrCreateAndFind(label_entry->name(),
                                                current_subsection_->section());
      MAO_ASSERT(symbol);
      // TODO(martint): The codes does not currently set the correct
      //                section for all labels in the symboltable.
      break;
    case MaoEntry::DIRECTIVE:
      // Update sections when necessary. Doing it here instead of
      // in ir.cc makes it possible to add the entry
      // when creating a new subsection.
      directive_entry = static_cast<DirectiveEntry *>(entry);
      switch (directive_entry->op()) {
        case DirectiveEntry::SECTION:
          {
            // the name from operand
            MAO_ASSERT(directive_entry->NumOperands() > 0);
            const DirectiveEntry::Operand *section_name =
                directive_entry->GetOperand(0);
            MAO_ASSERT(section_name->type == DirectiveEntry::STRING);
            std::string *s_section_name = section_name->data.str;
            prev_subsection_ = current_subsection_;
            current_subsection_ = AddNewSubSection(s_section_name->c_str(), 0,
                                                   entry);
            break;
          }
        case DirectiveEntry::SUBSECTION:
          {
            // the name from operand
            MAO_ASSERT(directive_entry->NumOperands() == 1);
            const DirectiveEntry::Operand *subsection_op =
                directive_entry->GetOperand(0);
            MAO_ASSERT(subsection_op->type == DirectiveEntry::INT);
            int subsection_number = subsection_op->data.i;
            prev_subsection_ = current_subsection_;
            current_subsection_ = AddNewSubSection(
                current_subsection_->name().c_str(),
                subsection_number, entry);
            break;
          }
        case DirectiveEntry::SET :
          {
            // Get the symbols here!
            MAO_ASSERT(directive_entry->NumOperands() == 2);
            const DirectiveEntry::Operand *op_symbol =
                directive_entry->GetOperand(0);

            // Handle the first operand
            MAO_ASSERT(op_symbol->type == DirectiveEntry::SYMBOL);
            Symbol *op_mao_symbol = symbol_table_.Find(
                S_GET_NAME(op_symbol->data.sym));
            MAO_ASSERT(op_mao_symbol != NULL);

            // Handle the second operand
            const DirectiveEntry::Operand *op_expr =
                directive_entry->GetOperand(1);
            if (op_expr->type == DirectiveEntry::EXPRESSION) {
              expressionS *op_as_expr = op_expr->data.expr;
              // make sure its a symbol..
              if (op_as_expr->X_op == O_symbol) {
                Symbol *op_symbol_2 =
                    symbol_table_.Find(S_GET_NAME(op_as_expr->X_add_symbol));
                op_symbol_2->AddEqual(op_mao_symbol);
              }
            }
            break;
          }
        default:
          break;
      }
      break;
    default:
      // should never happen. Catch all cases above.
      MAO_RASSERT_MSG(0, "Entry type not recognised.");
  }

  // Add the entry to the compilation unit
  entry_vector_.push_back(entry);

  // Update subsection information
  if (current_subsection_) {
    current_subsection_->set_last_entry(entry);
    entry_to_subsection_[entry] = current_subsection_;
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
    // TODO(martint): Use a ID factory
    Section *section = current_subsection_?
        (current_subsection_->section()):
        NULL;
    symbol = symbol_table_.Add(new Symbol(name, symbol_table_.Size(), section));
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

long MaoUnit::BBNameGen::i = 0;
const char *MaoUnit::BBNameGen::GetUniqueName() {
  char buff[512];
  MAO_ASSERT(i <= LONG_MAX);
  sprintf(buff, ".L__mao_label_%ld", i);
  char *buff2 = strdup(buff);
  i++;
  return buff2;
}

MaoUnit::FunctionIterator MaoUnit::FunctionBegin() {
  return functions_.begin();
}

MaoUnit::FunctionIterator MaoUnit::FunctionEnd() {
  return functions_.end();
}

MaoUnit::ConstFunctionIterator MaoUnit::ConstFunctionBegin() const {
  return functions_.begin();
}

MaoUnit::ConstFunctionIterator MaoUnit::ConstFunctionEnd() const {
  return functions_.end();
}

SectionIterator MaoUnit::SectionBegin() {
  return SectionIterator(sections_.begin());
}

SectionIterator MaoUnit::SectionEnd() {
  return SectionIterator(sections_.end());
}

ConstSectionIterator MaoUnit::ConstSectionBegin() const {
  return ConstSectionIterator(sections_.begin());
}

ConstSectionIterator MaoUnit::ConstSectionEnd() const {
  return ConstSectionIterator(sections_.end());
}

void MaoUnit::FindFunctions(bool create_anonymous) {
  // Use the symbol-table to find out the names of
  // the functions in the code!
  for (SymbolIterator iter = symbol_table_.Begin();
       iter != symbol_table_.End();
       ++iter) {
    const Symbol *symbol = *iter;
    if (symbol->IsFunction()) {
      // Find the entry given the label now
      MaoEntry *entry = GetLabelEntry(symbol->name());
      MAO_ASSERT_MSG(entry != NULL, "Unable to find label: %s", symbol->name());

      // TODO(martint): create ID factory for functions
      MAO_ASSERT(GetSubSection(entry));
      Function *function = new Function(symbol->name(), functions_.size(),
                                        GetSubSection(entry));
      function->set_first_entry(entry);
      entry_to_function_[entry] = function;

      // Find the last entry in this function:
      // A function ends when you find one of the following
      //  - The end of the section (next pointer is NULL)
      //  - Current entry is a .size directive for the current function.
      //   (The function includes this entry)
      //  - Start of a new function. Check if the next entry is a label
      //    that is marked as a function in the symbol table.

      MaoEntry *entry_tail;
      // Assume that the function starts with a label
      // and holds atleast one more entry!
      MAO_ASSERT(entry->Type() == MaoEntry::LABEL);
      entry_tail = entry->next();
      MAO_ASSERT(entry_tail->prev() == entry);
      MAO_ASSERT(entry_tail);
      // Stops at the end of a section, or breaks when a new functions is found.
      while (entry_tail->next()) {
        // Normally functions end with .size directive. Check
        // for this here.
        if (entry_tail->Type() == MaoEntry::DIRECTIVE) {
          // is it a function?
          DirectiveEntry *directive_entry = entry_tail->AsDirective();
          if (directive_entry->op() == DirectiveEntry::SIZE) {
            MAO_ASSERT(directive_entry->NumOperands() == 2);
            // check the first operand
            const DirectiveEntry::Operand *d_op =
                directive_entry->GetOperand(0);
            MAO_ASSERT(d_op->type == DirectiveEntry::SYMBOL);
            const char *size_symbol_name = S_GET_NAME(d_op->data.sym);
            if (strcmp(size_symbol_name, symbol->name()) == 0) {
              // We found the end of the function.
              break;
            }
          }
        }

        // Check if we have reached the start of a new function.
        if (entry_tail->next()->Type() == MaoEntry::LABEL) {
          // is it a function?
          LabelEntry *label_entry = entry_tail->next()->AsLabel();
          Symbol *l_symbol = symbol_table_.Find(label_entry->name());
          MAO_ASSERT(l_symbol);
          if (l_symbol->IsFunction()) {
            break;
          }
        }
        entry_to_function_[entry_tail] = function;
        entry_tail = entry_tail->next();
      }

      // Now entry_tail can not move more forward.
      entry_to_function_[entry_tail] = function;
      function->set_last_entry(entry_tail);
      functions_.push_back(function);
    }
  }

  // Now, create an unnamed function for any instructions at the start of any
  // section. Special case for sections created in mao to handle the initial
  // directives found in the assembly file.
  if (create_anonymous == true) {
    int function_number = 0;
    for (SectionIterator iter = SectionBegin();
         iter != SectionEnd();
         ++iter) {
      // Make sure the section exists in bfd, and it nos created
      // created temporarily inside mao.
      Section *section = *iter;
      if (bfd_get_section_by_name(stdoutput,
                                  section->name().c_str()) != NULL &&
          section->name() != ".eh_frame" &&
          section->name() != ".bss" &&
          section->name() != ".rodata" &&
          section->name() != ".data") {
        // Iterate over entries, until we find an entry belonging to a function,
        // or the end of the text section
        MaoEntry *entry = *(section->EntryBegin());
        int instruciton_entries = 0;
        if (entry &&
            !InFunction(entry)) {
          // Find the subsection from the entry
          SubSection *subsection = GetSubSection(entry);
          MAO_ASSERT(subsection);

          char function_name[64];
          sprintf(function_name, "__mao_unnamed%d", function_number++);
          Function *function = new Function(function_name, functions_.size(),
                                            subsection);
          function->set_first_entry(entry);
          // New section should break the anaonumous function.
          while (entry &&
                 !InFunction(entry)) {
            function->set_last_entry(entry);
            entry_to_function_[entry] = function;
            if (entry->IsInstruction())
              ++instruciton_entries;
            entry = entry->next();
          }
          // Only add anonymous functions if they have
          // any instruction entries in them.
          if (instruciton_entries > 0) {
            functions_.push_back(function);
          } else {
            delete function;
          }
        }
      }  // if (bfd_get_section_by_name( ...
    }  // for (SectionIterator iter = ...
  }  // if (create_anonymous = ...
  return;
}


Symbol *MaoUnit::AddSymbol(const char *name) {
  Section *section = current_subsection_?
      (current_subsection_->section()):
      NULL;
  // TODO(martint): Use a ID factory
  return symbol_table_.Add(new Symbol(name, symbol_table_.Size(), section));
}


Symbol *MaoUnit::FindOrCreateAndFindSymbol(const char *name) {
  Section *section = current_subsection_?
      (current_subsection_->section()):
      NULL;
  return symbol_table_.FindOrCreateAndFind(name, section);
}


Function *MaoUnit::GetFunction(MaoEntry *entry) {
  if (entry_to_function_.find(entry) == entry_to_function_.end()) {
    return NULL;
  } else {
    return entry_to_function_[entry];
  }
}

bool MaoUnit::InFunction(MaoEntry *entry) const {
  return entry_to_function_.find(entry) != entry_to_function_.end();
}


SubSection *MaoUnit::GetSubSection(MaoEntry *entry) {
  if (entry_to_subsection_.find(entry) == entry_to_subsection_.end()) {
    return NULL;
  } else {
    return entry_to_subsection_[entry];
  }
}

bool MaoUnit::InSubSection(MaoEntry *entry) const {
  return entry_to_subsection_.find(entry) != entry_to_subsection_.end();
}

void MaoUnit::DeleteEntry(MaoEntry *entry) {
  // 1. Prev/next pointers around the entry
  MaoEntry *prev_entry = entry->prev();  // Possibly null
  MaoEntry *next_entry = entry->next();  // Possibly null
  if (prev_entry)
    prev_entry->set_next(next_entry);
  if (next_entry)
    next_entry->set_prev(prev_entry);

  // 2. Remove it from entry_vector_
  entry_vector_[entry->id()] = NULL;

  // 3. Update function if needed
  if (InFunction(entry)) {
    Function *function = GetFunction(entry);
    MAO_ASSERT(function);
    // check if this function should be deleted altogether?
    if (function->first_entry() == function->last_entry()) {
      // TODO(martint): remove function!
      MAO_ASSERT_MSG(false, "Not implemented. Remove function here.");
    }
    // check if its the start or end pointers
    if (function->first_entry() == entry) {
      MAO_ASSERT(GetFunction(next_entry) == function);
      function->set_first_entry(next_entry);
    }
    if (function->last_entry() == entry) {
      MAO_ASSERT(GetFunction(prev_entry) == function);
      function->set_last_entry(prev_entry);
    }
    // For now, we reset the cfg
    // TODO(martint): Deallocate memory!
    // function->set_cfg(NULL);

    Section *section = function->GetSection();
    if (MaoRelaxer::HasSizeMap(section)) {
        std::map<MaoEntry *, int> *sizes =
            MaoRelaxer::GetSizeMap(this, section);
        MAO_ASSERT(sizes->find(entry) != sizes->end());
        sizes->erase(sizes->find(entry));
    }
  }

  // 4. Update subsection if needed
  if (InSubSection(entry)) {
    SubSection *subsection = GetSubSection(entry);
    if (subsection->first_entry() == subsection->last_entry()) {
      // TODO(martint): remove subsection!
      MAO_ASSERT_MSG(false, "Not implemented. Remove subsection here.");
    }
    // check if its the start or end pointers
    if (subsection->first_entry() == entry) {
      MAO_ASSERT(GetSubSection(next_entry) == subsection);
      subsection->set_first_entry(next_entry);
    }
    if (subsection->last_entry() == entry) {
      MAO_ASSERT(GetSubSection(prev_entry) == subsection);
      subsection->set_last_entry(prev_entry);
    }
  }

  // 5. Update entry_to_*_ maps
  if (InFunction(entry)) {
    entry_to_function_.erase(entry_to_function_.find(entry));
    MAO_ASSERT(!InFunction(entry));
  }
  if (InSubSection(entry)) {
    entry_to_subsection_.erase(entry_to_subsection_.find(entry));
    MAO_ASSERT(!InSubSection(entry));
  }

  // 6. Update labels map
  if (entry->IsLabel()) {
    LabelEntry *le = entry->AsLabel();
    labels_.erase(labels_.find(le->name()));
  }
}

void MaoUnit::PushSubSection() {
  MAO_ASSERT(current_subsection_);
  subsection_stack_.push(std::make_pair(current_subsection_,
                                        prev_subsection_));
  // In MAO. pushsection gets translated to  .section
  // directive. We need to make sure that prev_subsection is
  // set correctly here.
  prev_subsection_ = current_subsection_;
}


void MaoUnit::PopSubSection(int line_number) {
  MAO_ASSERT(!subsection_stack_.empty());
  SubSection *ss = subsection_stack_.top().first;
  SubSection *ps = subsection_stack_.top().second;
  subsection_stack_.pop();

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(
                         ss->name().c_str()));
  DirectiveEntry *directive =
      new DirectiveEntry(DirectiveEntry::SECTION, operands,
                         line_number, NULL, this);

  // AddEntry will change current_subserction_
  AddEntry(directive, false);

  // Pushsection takes a subsection number as argument. Since we translate
  // .pushsection to .section, we have to add another .subsection entry
  // if a subsection number is given.
  if (ss->number() != 0) {
    DirectiveEntry::OperandVector ss_operand;
    ss_operand.push_back(new DirectiveEntry::Operand(ss->number()));
    DirectiveEntry *ss_directive =
        new DirectiveEntry(DirectiveEntry::SUBSECTION, ss_operand,
                           line_number, NULL, this);
    AddEntry(ss_directive, false);
  }

  prev_subsection_ = ps;
}


void MaoUnit::SetPreviousSubSection(int line_number) {
  MAO_ASSERT(current_subsection_);
  MAO_ASSERT(prev_subsection_);
  // TODO(martint): Should we update the subsection_ stack?
  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(
                         prev_subsection_->name().c_str()));
  DirectiveEntry *directive =
      new DirectiveEntry(DirectiveEntry::SECTION, operands,
                         line_number, NULL, this);
  AddEntry(directive, false);
}
