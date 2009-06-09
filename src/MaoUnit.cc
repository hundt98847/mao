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
#include "MaoRelax.h"
#include "MaoUnit.h"

#include "ir.h"

// Needed for macros (OPERAND_TYPE_IMM64, ..) in gotrel[]
#include "opcodes/i386-init.h"
#include "tc-i386-helper.h"


//
// Class: MaoUnit
//

// Default to no subsection selected
// A default will be generated if necessary later on.
MaoUnit::MaoUnit(MaoOptions *mao_options)
    : current_subsection_(0), mao_options_(mao_options) {
  entry_vector_.clear();
  sub_sections_.clear();
  sections_.clear();
  functions_.clear();
  entry_to_function_.clear();
  entry_to_subsection_.clear();

  const char *arch_string = get_arch();
  if (strcmp(arch_string, "i386") == 0 || strcmp(arch_string, "I386") == 0) {
    arch_ = I386;
  } else if (strcmp(arch_string, "x86_64") == 0 ||
             strcmp(arch_string, "X86_64") == 0) {
    arch_ = X86_64;
  } else {
    MAO_ASSERT_MSG(false, "Unsupported architecture: %s", arch_string);
  }
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
    for (SectionEntryIterator e_iter = ss->EntryBegin();
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
      for (SectionEntryIterator e_iter = ss->EntryBegin();
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

// Called when found a new subsection reference in the assembly.
bool MaoUnit::SetSubSection(const char *section_name,
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

  // set current_subsection!
  prev_subsection_ = current_subsection_;
  current_subsection_ = subsection;

  // Assume that the section is only one entry long.
  // last_entry is updated as we add entries..
  current_subsection_->set_first_entry(entry);
  current_subsection_->set_last_entry(entry);

  // Now we should check if we should link this entry back to the previous
  // subsection within this section!
  if (last_subsection) {
    MaoEntry *last_entry = last_subsection->last_entry();
    last_entry->set_next(entry);
    entry->set_prev(last_entry);
  }

  return section_pair.first;
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
      MAO_ASSERT_MSG(false, "Unable to match code flag to architecture");
      return NULL; // Quells a warning about flag used without being defined
  }

  InstructionEntry *e = new InstructionEntry(&insn, flag, 0, NULL, this);
  SubSection *subsection = function->GetSubSection();
  if (function) {
    entry_to_function_[e] = function;
  }
  if (subsection) {
    entry_to_subsection_[e] = subsection;
  }
  return e;
}

InstructionEntry *MaoUnit::CreateNop(Function *function) {
  InstructionEntry *e = CreateInstruction("nop", 0x90, function);

  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  e->set_id(entry_index);

  e->set_op(OP_nop);

  // Add the entry to the compilation unit
  entry_vector_.push_back(e);
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

  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  e->set_id(entry_index);
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

  // Add the entry to the compilation unit
  entry_vector_.push_back(e);
  return e;
}


InstructionEntry *MaoUnit::CreateUncondJump(LabelEntry *label,
                                            Function *function) {
  InstructionEntry *e = CreateInstruction("jmp", 0xeb, function);
  expressionS *disp_expression = (expressionS *)xmalloc(sizeof(expressionS));
  symbolS *symbolP;

  i386_insn *insn = e->instruction();
  insn->operands = 1;
  insn->disp_operands = 1;
  insn->mem_operands = 1;

  //Set the displacement fileds
  insn->types[0].bitfield.disp32 = 1;
  insn->types[0].bitfield.disp32s = 1;

  for (int j = 0; j < MAX_OPERANDS; j++)
    insn->reloc[j] = NO_RELOC;

  symbolP = symbol_find_or_make (label->name());

  disp_expression->X_op = O_symbol;
  disp_expression->X_add_symbol = symbolP;
  disp_expression->X_add_number = 0;

  insn->op[0].disps = disp_expression;



  // next free ID for the entry
  EntryID entry_index = entry_vector_.size();
  e->set_id(entry_index);

  e->set_op(OP_jmp);

  // Add the entry to the compilation unit
  entry_vector_.push_back(e);
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
      SetSubSection("mao_start_section", 0,  entry);
      current_subsection_->set_start_section(true);
    }
  }
  // Create a subsection if necessary
  if (create_default_section &&
      (!current_subsection_ || current_subsection_->start_section())) {
    SetSubSection(DEFAULT_SECTION_NAME, 0, entry);
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
      if (directive_entry->op() == DirectiveEntry::SECTION) {
        // the name from operand
        MAO_ASSERT(directive_entry->NumOperands() > 0);
        const DirectiveEntry::Operand *section_name =
            directive_entry->GetOperand(0);
        MAO_ASSERT(section_name->type == DirectiveEntry::STRING);
        std::string *s_section_name = section_name->data.str;
        SetSubSection(s_section_name->c_str(), 0, entry);
      }

      if (directive_entry->op() == DirectiveEntry::SET) {
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
        const DirectiveEntry::Operand *op_expr = directive_entry->GetOperand(1);
        if (op_expr->type == DirectiveEntry::EXPRESSION) {
          expressionS *op_as_expr = op_expr->data.expr;
          // make sure its a symbol..
          if (op_as_expr->X_op == O_symbol) {
            Symbol *op_symbol_2 =
                symbol_table_.Find(S_GET_NAME(op_as_expr->X_add_symbol));
            op_symbol_2->AddEqual(op_mao_symbol);
          }
        }
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

void MaoUnit::FindFunctions() {
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
        // size directive for current function?
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
              break;
            }
          }
        }

        // check if we found the next function?
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
  // section. Special case for sections created in mao to handle the inital
  // directives found int he assembly file.
  int function_number = 0;
  for (SectionIterator iter = SectionBegin();
       iter != SectionEnd();
       ++iter) {
    // Make sure the section exists in bfd, and it nos created
    // created temporarily inside mao.
    Section *section = *iter;
    if (bfd_get_section_by_name(stdoutput,
                                section->name().c_str()) != NULL &&
        section->name() != ".eh_frame") {
      // Iterate over entries, until we find an entry belonging to a function,

     // or the end of the text section
      MaoEntry *entry = *(section->EntryBegin());
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
        while (entry &&
               !InFunction(entry)) {
          function->set_last_entry(entry);
          entry_to_function_[entry] = function;
          entry = entry->next();
        }
        functions_.push_back(function);
      }
    }
  }
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

void MaoUnit::PushCurrentSubSection() {
  MAO_ASSERT(current_subsection_);
  subsection_stack_.push(current_subsection_);
}


void MaoUnit::PopSubSection(int line_number) {
  MAO_ASSERT(!subsection_stack_.empty());
  SubSection *ss = subsection_stack_.top();
  subsection_stack_.pop();

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(
                         ss->name().c_str()));
  DirectiveEntry *directive =
      new DirectiveEntry(DirectiveEntry::SECTION, operands,
                         line_number, NULL, this);
  // AddEntry will change current_subserction_
  AddEntry(directive, false);
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



//
// Class: SectionIterator
//

Section *&SectionIterator::operator *() const {
  return section_iter_->second;
}

SectionIterator &SectionIterator::operator ++() {
  ++section_iter_;
  return *this;
}

SectionIterator &SectionIterator::operator --() {
  --section_iter_;
  return *this;
}

bool SectionIterator::operator ==(const SectionIterator &other) const {
  return (section_iter_ == other.section_iter_);
}

bool SectionIterator::operator !=(const SectionIterator &other) const {
  return !((*this) == other);
}


//
// Class: ConstSectionIterator
//

Section * const &ConstSectionIterator::operator *() const {
  return section_iter_->second;
}

ConstSectionIterator const &ConstSectionIterator::operator ++() {
  ++section_iter_;
  return *this;
}

ConstSectionIterator const &ConstSectionIterator::operator --() {
  --section_iter_;
  return *this;
}

bool ConstSectionIterator::operator ==(const ConstSectionIterator &other)
    const {
  return (section_iter_ == other.section_iter_);
}

bool ConstSectionIterator::operator !=(const ConstSectionIterator &other)
    const {
  return !((*this) == other);
}




//
// Class: MaoEntry
//

MaoEntry::MaoEntry(unsigned int line_number, const char *line_verbatim,
                   MaoUnit *maounit) :
    maounit_(maounit), id_(0), next_(0), prev_(0), line_number_(line_number) {
  if (line_verbatim) {
    MAO_ASSERT(strlen(line_verbatim) < MAX_VERBATIM_ASSEMBLY_STRING_LENGTH);
    line_verbatim_ = strdup(line_verbatim);
  } else {
    line_verbatim_ = 0;
  }
}

MaoEntry::~MaoEntry() {
}


// used when creating temporary expression
// used when processing the dot (".") label in non-absolute sections
#ifndef FAKE_LABEL_NAME
#define FAKE_LABEL_NAME "L0\001"
#endif

std::string MaoEntry::GetDotOrSymbol(symbolS *symbol) const {
  const char *s = S_GET_NAME(symbol);
  MAO_ASSERT(s);
  if (strcmp(s, FAKE_LABEL_NAME) == 0) {
    return ".";

// This code fails, since the same temporary name is used for both dot symbols
// and other temporary expressions.
//     // gas can sometimes store temporary value as a symbol.
//     // See /binutils-2.19/gas/testsuite/gas/i386/absrel.s for an example.
//     std::stringstream out;
//     out << S_GET_VALUE(symbol);
//     return out.str();
  } else {
    return s;
  }
}


// Return the matching relocation string, or an empty string
// if no match is found.
const std::string &MaoEntry::RelocToString(const enum bfd_reloc_code_real reloc,
                                           std::string *out) const {
  // copied gotrel from tc-i386.c
  static const struct {
    const char *str;
    const enum bfd_reloc_code_real rel[2];
    const i386_operand_type types64;
  } gotrel[] = {
    { "PLTOFF",   { _dummy_first_bfd_reloc_code_real,
                    BFD_RELOC_X86_64_PLTOFF64 },
      OPERAND_TYPE_IMM64 },
    { "PLT",      { BFD_RELOC_386_PLT32,
                    BFD_RELOC_X86_64_PLT32    },
      OPERAND_TYPE_IMM32_32S_DISP32 },
    { "GOTPLT",   { _dummy_first_bfd_reloc_code_real,
                    BFD_RELOC_X86_64_GOTPLT64 },
      OPERAND_TYPE_IMM64_DISP64 },
    { "GOTOFF",   { BFD_RELOC_386_GOTOFF,
                BFD_RELOC_X86_64_GOTOFF64 },
      OPERAND_TYPE_IMM64_DISP64 },
    { "GOTPCREL", { _dummy_first_bfd_reloc_code_real,
                    BFD_RELOC_X86_64_GOTPCREL },
      OPERAND_TYPE_IMM32_32S_DISP32 },
    { "TLSGD",    { BFD_RELOC_386_TLS_GD,
                    BFD_RELOC_X86_64_TLSGD    },
      OPERAND_TYPE_IMM32_32S_DISP32 },
    { "TLSLDM",   { BFD_RELOC_386_TLS_LDM,
                    _dummy_first_bfd_reloc_code_real },
      OPERAND_TYPE_NONE },
    { "TLSLD",    { _dummy_first_bfd_reloc_code_real,
                    BFD_RELOC_X86_64_TLSLD    },
      OPERAND_TYPE_IMM32_32S_DISP32 },
    { "GOTTPOFF", { BFD_RELOC_386_TLS_IE_32,
                    BFD_RELOC_X86_64_GOTTPOFF },
      OPERAND_TYPE_IMM32_32S_DISP32 },
    { "TPOFF",    { BFD_RELOC_386_TLS_LE_32,
                    BFD_RELOC_X86_64_TPOFF32  },
      OPERAND_TYPE_IMM32_32S_64_DISP32_64 },
    { "NTPOFF",   { BFD_RELOC_386_TLS_LE,
                    _dummy_first_bfd_reloc_code_real },
      OPERAND_TYPE_NONE },
    { "DTPOFF",   { BFD_RELOC_386_TLS_LDO_32,
                    BFD_RELOC_X86_64_DTPOFF32 },
      OPERAND_TYPE_IMM32_32S_64_DISP32_64 },
    { "GOTNTPOFF",{ BFD_RELOC_386_TLS_GOTIE,
                    _dummy_first_bfd_reloc_code_real },
      OPERAND_TYPE_NONE },
    { "INDNTPOFF",{ BFD_RELOC_386_TLS_IE,
                    _dummy_first_bfd_reloc_code_real },
      OPERAND_TYPE_NONE },
    { "GOT",      { BFD_RELOC_386_GOT32,
                    BFD_RELOC_X86_64_GOT32    },
      OPERAND_TYPE_IMM32_32S_64_DISP32 },
    { "TLSDESC",  { BFD_RELOC_386_TLS_GOTDESC,
                    BFD_RELOC_X86_64_GOTPC32_TLSDESC },
      OPERAND_TYPE_IMM32_32S_DISP32 },
    { "TLSCALL",  { BFD_RELOC_386_TLS_DESC_CALL,
                    BFD_RELOC_X86_64_TLSDESC_CALL },
      OPERAND_TYPE_IMM32_32S_DISP32 },
  };


  for (unsigned int i = 0; i < sizeof(gotrel)/sizeof(gotrel[0]); ++i) {
    int idx = maounit_->Is64BitMode()?1:0;
    if (reloc == gotrel[i].rel[idx]) {
      out->append("@");
      out->append(gotrel[i].str);
      return *out;
    }
  }
  // Specal case not covered in the gotrel array.
  // Found in gas test-suite reloc32.s etc.
  switch (reloc) {
    case BFD_RELOC_32:
      out->append("@GOTOFF");
      break;
    case BFD_RELOC_32_PCREL:
      out->append("@GOTPCREL");
      break;
    case   BFD_RELOC_NONE:
    case _dummy_first_bfd_reloc_code_real:
      break;
    default:
      MAO_ASSERT_MSG(reloc == _dummy_first_bfd_reloc_code_real,
                     "Unknown reloc: %d", reloc);
  }
  // No matching relocation found!
  return *out;
}


void MaoEntry::Spaces(unsigned int n, FILE *outfile) const {
  for (unsigned int i = 0; i < n; i++) {
    fprintf(outfile, " ");
  }
}

std::string &MaoEntry::SourceInfoToString(std::string *out) const {
  std::ostringstream stream;
  stream << "\t# id: "
         << id()
         << ", l: "
         << line_number()
         << "\t";
  if (0 && line_verbatim())  // TODO(rhundt): Invent option for this
    stream << line_verbatim();
  out->append(stream.str());
  return *out;
}

void MaoEntry::LinkBefore(MaoEntry *entry) {
  entry->set_next(this);
  entry->set_prev(prev());

  if (prev()) {
    prev()->set_next(entry);
  } else {
    // TODO(rhundt): Set "first" pointer of everything to entry
  }
  this->set_prev(entry);

  // Do we need to update the function?
  Function *function = maounit_->GetFunction(this);
  if (function &&
      function->first_entry() == this) {
    function->set_first_entry(entry);
  }

  // Do we need to update the subsection?
  SubSection *subsection = maounit_->GetSubSection(this);
  MAO_ASSERT(subsection);
  if (subsection->first_entry() == this) {
    subsection->set_first_entry(entry);
  }
}

void MaoEntry::LinkAfter(MaoEntry *entry) {
  entry->set_next(next());
  entry->set_prev(this);
  set_next(entry);

  // Do we need to update the function?
  Function *function = maounit_->GetFunction(this);
  if (function &&  // Not all entries are part of a function.
      function->last_entry() == this) {
    function->set_last_entry(entry);
  }

  // Do we need to update the subsection?
  SubSection *subsection = maounit_->GetSubSection(this);
  MAO_ASSERT(subsection);
  if (subsection->last_entry() == this) {
    subsection->set_last_entry(entry);
  }
}


void MaoEntry::AlignTo(int power_of_2_alignment,
                       int fill_value,
                       int max_bytes_to_skip) {
  MAO_ASSERT(maounit_->GetFunction(this));

  Function *function = maounit_->GetFunction(this);
  SubSection *ss = function->GetSubSection();
  MAO_ASSERT(ss);

  DirectiveEntry::OperandVector operands;
  operands.push_back(new DirectiveEntry::Operand(power_of_2_alignment));
  if (fill_value == -1)
    operands.push_back(new DirectiveEntry::Operand());
  else
    operands.push_back(new DirectiveEntry::Operand(fill_value));
  operands.push_back(new DirectiveEntry::Operand(max_bytes_to_skip));
  DirectiveEntry *align_entry = maounit_->CreateDirective(
    DirectiveEntry::P2ALIGN, operands,
    function, ss);
  LinkBefore(align_entry);
}


const char *MaoEntry::GetSymbolnameFromExpression(expressionS *expr) const {
  const char *label_name = NULL;
  // TODO(martint): support all expression
  switch (expr->X_op) {
    case O_constant:
      if (expr->X_add_symbol) {
        label_name = S_GET_NAME(expr->X_add_symbol);
      } else {
        return NULL;
      }
      break;
    case O_symbol:
      label_name = S_GET_NAME(expr->X_add_symbol);
      break;
    case O_add:
      label_name = S_GET_NAME(expr->X_add_symbol);
      break;
    case O_subtract:
      label_name = S_GET_NAME(expr->X_add_symbol);
      break;
    default:
      // Expression not supported
      return NULL;
  }
  return label_name;
}

//
// Class: LabelEntry
//

void LabelEntry::PrintEntry(FILE *out) const {
  MAO_ASSERT(name_);
  std::string s;
  ToString(&s);
  MaoEntry::SourceInfoToString(&s);
  fprintf(out, "%s\n", s.c_str());
}


std::string &LabelEntry::ToString(std::string *out) const {
  out->append(name_);
  out->append(":");
  return *out;
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
  ".set",  // identical to .equ
  ".equiv",
  ".weakref",
  ".arch",
  ".linefile",
  ".loc",
  ".allow_index_reg",
  ".disallow_index_reg",
  ".org",
  ".code16",
  ".code16gcc",
  ".code32",
  ".code64",
  ".dc.d",
  ".dc.s",
  ".dc.x"
};


const char *DirectiveEntry::GetOperandSeparator() const {
  if (IsDebugDirective()) {
    return " ";
  }
  return ", ";
}

void DirectiveEntry::PrintEntry(::FILE *out) const {
  std::string s;
  s.append("\t");
  s.append(GetOpcodeName());
  s.append("\t");
  OperandsToString(&s, GetOperandSeparator());
  SourceInfoToString(&s);
  fprintf(out, "%s\n", s.c_str());
}

std::string &DirectiveEntry::ToString(std::string *out) const {
  //  std::ostringstream stream;
  out->append(GetOpcodeName());
  out->append(" ");
  OperandsToString(out, GetOperandSeparator());
  return *out;
}

void DirectiveEntry::PrintIR(::FILE *out) const {
  std::string operands;
  fprintf(out, "%s %s", GetOpcodeName(),
          OperandsToString(&operands, GetOperandSeparator()).c_str());
}

const std::string &DirectiveEntry::OperandsToString(
    std::string *out,
    const char *separator) const {
  for (OperandVector::const_iterator iter = operands_.begin();
       iter != operands_.end(); ++iter) {
    if (iter != operands_.begin())
      out->append(separator);
    OperandToString(**iter, out);
  }
  return *out;
}


const char *DirectiveEntry::OpToString(operatorT op) const {
  switch (op) {
    case O_add:             return "+";
    case O_subtract:        return "-";
    case O_divide:          return "/";
    case O_multiply:        return "*";
    case O_modulus:         return "%";
    case O_left_shift:      return "<<";
    case O_right_shift:     return "<<";
    case O_bit_inclusive_or:return "|";
    case O_bit_or_not:      return "|~";
    case O_bit_exclusive_or:return "^";
    case O_bit_and:         return "&";
    case O_eq:              return "==";
    case O_ne:              return "!=";
    case O_lt:              return "<";
    case O_le:              return "<=";
    case O_ge:              return ">=";
    case O_gt:              return ">";
    case O_logical_and:     return "&&";
    case O_logical_or:      return "||";
    default:
      break;
  }
  MAO_ASSERT(false);
  return "";
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
          exp_string << GetDotOrSymbol(expr->X_add_symbol);
          if (expr->X_add_number != 0) {
            exp_string << "+";
          }
        }
        if (expr->X_add_number != 0) {
          exp_string << expr->X_add_number;
        }
        out->append(exp_string.str());
      }
      break;

    case O_add:             /* (X_add_symbol +  X_op_symbol) + X_add_number.  */
    case O_subtract:        /* (X_add_symbol -  X_op_symbol) + X_add_number.  */
    case O_divide:          /* (X_add_symbol /  X_op_symbol) + X_add_number.  */
    case O_multiply:        /* (X_add_symbol *  X_op_symbol) + X_add_number.  */
    case O_modulus:         /* (X_add_symbol %  X_op_symbol) + X_add_number.  */
    case O_left_shift:      /* (X_add_symbol << X_op_symbol) + X_add_number.  */
    case O_right_shift:     /* (X_add_symbol >> X_op_symbol) + X_add_number.  */
    case O_bit_inclusive_or:/* (X_add_symbol |  X_op_symbol) + X_add_number.  */
    case O_bit_or_not:      /* (X_add_symbol |~ X_op_symbol) + X_add_number.  */
    case O_bit_exclusive_or:/* (X_add_symbol ^  X_op_symbol) + X_add_number.  */
    case O_bit_and:         /* (X_add_symbol &  X_op_symbol) + X_add_number.  */
    case O_eq:              /* (X_add_symbol == X_op_symbol) + X_add_number.  */
    case O_ne:              /* (X_add_symbol != X_op_symbol) + X_add_number.  */
    case O_lt:              /* (X_add_symbol <  X_op_symbol) + X_add_number.  */
    case O_le:              /* (X_add_symbol <= X_op_symbol) + X_add_number.  */
    case O_ge:              /* (X_add_symbol >= X_op_symbol) + X_add_number.  */
    case O_gt:              /* (X_add_symbol >  X_op_symbol) + X_add_number.  */
    case O_logical_and:     /* (X_add_symbol && X_op_symbol) + X_add_number.  */
    case O_logical_or:      /* (X_add_symbol || X_op_symbol) + X_add_number.  */
      {
        std::ostringstream exp_string;
        if (expr->X_add_symbol) {
          exp_string << GetDotOrSymbol(expr->X_add_symbol);
          if (expr->X_op_symbol) {
            exp_string << OpToString(expr->X_op);
          }
        }
        if (expr->X_op_symbol) {
          exp_string << GetDotOrSymbol(expr->X_op_symbol);
          if (expr->X_add_number != 0) {
            exp_string << "+";
          }
        }
        if (expr->X_add_number != 0) {
          exp_string << expr->X_add_number;
        }
        out->append(exp_string.str());
      }
      break;
      /* A register (X_add_number is register number).  */
    case O_register:
      {
        std::ostringstream exp_string;
        exp_string << "%" << i386_regtab[expr->X_add_number].reg_name;
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
          "OperandExpressionToString does not support the op %d\n",
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
      out->append(operand.data.str->c_str());
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
    case EXPRESSION_RELOC:
      OperandExpressionToString(operand.data.expr_reloc.expr, out);
      // Append the relocation to the string here!
      if (operand.data.expr_reloc.reloc != _dummy_first_bfd_reloc_code_real) {
        std::string reloc;
        out->append(RelocToString(operand.data.expr_reloc.reloc, &reloc));
      }
      break;
    case EMPTY_OPERAND:
      // Nothing to do
      break;
    default:
      MAO_ASSERT(false);
  }
  return *out;
}

MaoEntry::EntryType DirectiveEntry::Type() const {
  return DIRECTIVE;
}

bool DirectiveEntry::IsJumpTableEntry() const {
  // TODO(martint): Make sure we support jump tables generated for various
  // optimization levels and targets.
  if ((op_ == LONG || op_ == QUAD)) {
    return true;
  } else {
    return false;
  }
}


// Return label entry found, or NULL if unknown
// For PIC-code, the expression might be a subtraction. Then the symbol
// needed to find the target is the first symbol.
LabelEntry *DirectiveEntry::GetJumpTableTarget() {
  LabelEntry *le = NULL;
  const char *label_name = NULL;
  MAO_ASSERT(IsJumpTableEntry());
  if (NumOperands() == 1) {
    // Get the operand!
    const Operand *op = GetOperand(0);
    switch (op->type) {
      case STRING:
        label_name = op->data.str->c_str();
        break;
      case EXPRESSION:
        // Get the label from expression
        label_name = GetSymbolnameFromExpression(op->data.expr);
        break;
      default:
        // Operand type (%d) not supported in jump-table.
        return NULL;
    }
    // if we found a label, find the corresponding entry
    if (label_name == NULL) {
      // Unable to find label in jump table
      return NULL;
    }
    // GetLabelEntry returns NULL if it cant find the entry.
    le = maounit_->GetLabelEntry(label_name);
  }
  return le;
}

bool DirectiveEntry::IsDebugDirective() const {
  return (op() == DirectiveEntry::LINEFILE ||
          op() == DirectiveEntry::FILE ||
          op() == DirectiveEntry::LOC);
}

//
// Class: InstructionEntry
//

/* Returns 0 if attempting to add a prefix where one from the same
   class already exists, 1 if non rep/repne added, 2 if rep/repne
   added.  */
int InstructionEntry::AddPrefix(unsigned int prefix) {
  int ret = 1;
  unsigned int q;
  if (prefix >= REX_OPCODE && prefix < REX_OPCODE + 16
      && code_flag_ == CODE_64BIT) {
    if ((instruction_->prefix[X86InstructionSizeHelper::REX_PREFIX] &
         prefix & REX_W)
        || ((instruction_->prefix[X86InstructionSizeHelper::REX_PREFIX] &
             (REX_R | REX_X | REX_B))
            && (prefix & (REX_R | REX_X | REX_B))))
      ret = 0;
    q = X86InstructionSizeHelper::REX_PREFIX;
  } else {
    switch (prefix) {
      default:
        MAO_ASSERT(false);

      case CS_PREFIX_OPCODE:
      case DS_PREFIX_OPCODE:
      case ES_PREFIX_OPCODE:
      case FS_PREFIX_OPCODE:
      case GS_PREFIX_OPCODE:
      case SS_PREFIX_OPCODE:
        q = X86InstructionSizeHelper::SEG_PREFIX;
        break;

      case REPNE_PREFIX_OPCODE:
      case REPE_PREFIX_OPCODE:
        ret = 2;
        /* fall thru */
      case LOCK_PREFIX_OPCODE:
        q = X86InstructionSizeHelper::LOCKREP_PREFIX;
        break;

      case FWAIT_OPCODE:
        q = X86InstructionSizeHelper::WAIT_PREFIX;
        break;

      case ADDR_PREFIX_OPCODE:
        q = X86InstructionSizeHelper::ADDR_PREFIX;
        break;

      case DATA_PREFIX_OPCODE:
        q = X86InstructionSizeHelper::DATA_PREFIX;
        break;
    }
    if (instruction_->prefix[q] != 0) {
      ret = 0;
    }
  }

  if (ret) {
    if (!instruction_->prefix[q])
      ++instruction_->prefixes;
    instruction_->prefix[q] |= prefix;
  } else {
    MAO_ASSERT_MSG(false, "same type of prefix used twice");
  }

  return ret;
}

InstructionEntry::InstructionEntry(i386_insn *instruction,
                                   enum flag_code code_flag,
                                   unsigned int line_number,
                                   const char* line_verbatim,
                                   MaoUnit *maounit) :
    MaoEntry(line_number, line_verbatim, maounit), code_flag_(code_flag),
    execution_count_valid_(false), execution_count_(0) {
  op_ = GetOpcode(instruction->tm.name);
  MAO_ASSERT(op_ != OP_invalid);
  MAO_ASSERT(instruction);
  instruction_ = CreateInstructionCopy(instruction);

  // Here we can make sure that the prefixes are correct!
  unsigned int prefix;
  if (!instruction_->tm.opcode_modifier.vex) {
    switch (instruction_->tm.opcode_length) {
      case 3:
        if (instruction_->tm.base_opcode & 0xff000000) {
          prefix = (instruction_->tm.base_opcode >> 24) & 0xff;
          goto check_prefix;
        }
        break;
      case 2:
        if ((instruction_->tm.base_opcode & 0xff0000) != 0) {
          prefix = (instruction_->tm.base_opcode >> 16) & 0xff;
          if (instruction_->tm.cpu_flags.bitfield.cpupadlock) {
         check_prefix:
            if (prefix != REPE_PREFIX_OPCODE ||
                (instruction_->prefix[X86InstructionSizeHelper::LOCKREP_PREFIX]
                 != REPE_PREFIX_OPCODE))
              AddPrefix(prefix);
          } else {
            AddPrefix(prefix);
          }
        }
        break;
      case 1:
        break;
      default:
        MAO_ASSERT(false);
    }
  }
}

InstructionEntry::~InstructionEntry() {
  MAO_ASSERT(instruction_);
  FreeInstruction();
}

void InstructionEntry::PrintEntry(FILE *out) const {
  std::string s;
  InstructionToString(&s);
  ProfileToString(&s);
  SourceInfoToString(&s);
  fprintf(out, "%s\n", s.c_str());
}


std::string &InstructionEntry::ToString(std::string *out) const {
  InstructionToString(out);
  return *out;
}


void InstructionEntry::PrintIR(FILE *out) const {
  std::string s;
  InstructionToString(&s);
  ProfileToString(&s);
  fprintf(out, "%s", s.c_str());
}

MaoEntry::EntryType InstructionEntry::Type() const {
  return INSTRUCTION;
}

const char *InstructionEntry::op_str() const {
  MAO_ASSERT(instruction_);
  MAO_ASSERT(instruction_->tm.name);
  return(instruction_->tm.name);
}

// This deallocates memory allocated in CreateInstructionCopy().
void InstructionEntry::FreeInstruction() {
  MAO_ASSERT(instruction_);
  for (unsigned int i = 0; i < instruction_->operands; i++) {
    if (IsImmediateOperand(instruction_, i)) {
      delete instruction_->op[i].imms;
    }
    if (IsMemOperand(instruction_, i)) {
      delete instruction_->op[i].disps;
    }
    if (IsRegisterOperand(instruction_, i)) {
      delete instruction_->op[i].regs;
    }
  }
  for (unsigned int i = 0; i < 2; i++) {
    delete instruction_->seg[i];
  }
  delete instruction_->base_reg;
  delete instruction_->index_reg;
  delete instruction_;
}

bool InstructionEntry::IsPredicated() const {
  const MaoOpcode opcode_is_predicated[] =  {
    OP_cmovo,   OP_cmovno,  OP_cmovb,   OP_cmovc,    OP_cmovnae,
    OP_cmovae,  OP_cmovnc,  OP_cmovnb,  OP_cmove,    OP_cmovz,
    OP_cmovne,  OP_cmovnz,  OP_cmovbe,  OP_cmovna,   OP_cmova,
    OP_cmovnbe, OP_cmovs,   OP_cmovns,  OP_cmovp,    OP_cmovnp,
    OP_cmovl,   OP_cmovnge, OP_cmovge,  OP_cmovnl,   OP_cmovle,
    OP_cmovng,  OP_cmovg,   OP_cmovnle, OP_fcmovb,   OP_fcmovnae,
    OP_fcmove,  OP_fcmovbe, OP_fcmovna, OP_fcmovu,   OP_fcmovae,
    OP_fcmovnb, OP_fcmovne, OP_fcmova,  OP_fcmovnbe, OP_fcmovnu
  };

  if (IsInList(op(), opcode_is_predicated,
               sizeof(opcode_is_predicated)/sizeof(MaoOpcode)))
    return true;
  return false;
}

bool InstructionEntry::IsMemOperand(const i386_insn *instruction,
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

bool InstructionEntry::IsImmediateOperand(const i386_insn *instruction,
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

bool InstructionEntry::IsRegisterOperand(const i386_insn *instruction,
                                       const unsigned int op_index) {
  MAO_ASSERT(instruction->operands > op_index);
  i386_operand_type t = instruction->types[op_index];
  return (t.bitfield.acc
          || t.bitfield.reg8
          || t.bitfield.reg16
          || t.bitfield.reg32
          || t.bitfield.reg64
          || t.bitfield.control
          || t.bitfield.test
          || t.bitfield.debug
          || t.bitfield.sreg2
          || t.bitfield.sreg3
          || t.bitfield.floatreg
          || t.bitfield.regxmm
          || t.bitfield.regmmx
          || t.bitfield.regymm);
}

std::string &InstructionEntry::ImmediateOperandToString(std::string *out,
                                       const enum bfd_reloc_code_real reloc,
                                       const expressionS *expr) const {
  std::ostringstream string_stream;
  std::string reloc_string;
  switch (expr->X_op) {
    case O_constant: {
      /* X_add_number (a constant expression).  */
      string_stream << "$" << expr->X_add_number;
      break;
    }
    case O_symbol:
      /* X_add_symbol + X_add_number.  */
      if (expr->X_add_symbol) {
        string_stream << "$"
                      << GetDotOrSymbol(expr->X_add_symbol)
                      << RelocToString(reloc, &reloc_string);
        string_stream << "+";
      }
      string_stream << expr->X_add_number;
      break;
    case O_add:
    case O_subtract: {
      /* (X_add_symbol + X_op_symbol) + X_add_number.  */
      /* (X_add_symbol - X_op_symbol) + X_add_number.  */
      const char *op = "";
      switch (expr->X_op) {
        case O_add:
          op = "+";
          break;
        case O_subtract:
          op = "-";
          break;
        default:
          MAO_ASSERT(false);
      }
      string_stream << "$";
      if (expr->X_add_symbol || expr->X_op_symbol) {
        string_stream << "(";
      }
      if (expr->X_add_symbol) {
        string_stream << GetDotOrSymbol(expr->X_add_symbol)
                      << RelocToString(reloc, &reloc_string).c_str();
      }
      if (expr->X_op_symbol) {
        string_stream << op
                      << GetDotOrSymbol(expr->X_op_symbol);
      }
      if (expr->X_add_symbol || expr->X_op_symbol) {
        string_stream << ")";
        if (expr->X_add_number) {
          string_stream << "+";
        }
      }
      if (expr->X_add_number) {
        string_stream << expr->X_add_number;
      }
      break;
    }
    default:
      MAO_ASSERT_MSG(0, "Unable to print unsupported expression");
  }
  out->append(string_stream.str());
  return *out;
}

// Make a copy of an expression.
expressionS *InstructionEntry::CreateExpressionCopy(expressionS *in_exp) {
  if (!in_exp)
    return NULL;
  expressionS *new_exp = new expressionS;
  memcpy(new_exp, in_exp, sizeof(expressionS));
  MAO_ASSERT(new_exp->X_add_number == in_exp->X_add_number);
  return new_exp;
}


// Checks if two expressions are equal. It does not resolve anything,
// and is conservative. It only returns true if it is sure they are
// equal. Equal here means they have identical experssions. Note that
// the expressions might resolve to different things if the "." symbols
// is used.
bool InstructionEntry::EqualExpressions(expressionS *expression1,
                                        expressionS *expression2) const {
  if (expression1->X_op != expression2->X_op)
    return false;

  switch (expression1->X_op) {
    case O_constant:        /* X_add_number (a constant expression).  */
      return (expression1->X_add_number == expression2->X_add_number);
    case O_symbol:          /* X_add_symbol + X_add_number.  */
      return (expression1->X_add_symbol == expression2->X_add_symbol &&
              expression1->X_add_number == expression2->X_add_number);
    case O_add:             /* (X_add_symbol +  X_op_symbol) + X_add_number.  */
    case O_subtract:        /* (X_add_symbol -  X_op_symbol) + X_add_number.  */
    case O_divide:          /* (X_add_symbol /  X_op_symbol) + X_add_number.  */
    case O_multiply:        /* (X_add_symbol *  X_op_symbol) + X_add_number.  */
    case O_modulus:         /* (X_add_symbol %  X_op_symbol) + X_add_number.  */
    case O_left_shift:      /* (X_add_symbol << X_op_symbol) + X_add_number.  */
    case O_right_shift:     /* (X_add_symbol >> X_op_symbol) + X_add_number.  */
    case O_bit_inclusive_or:/* (X_add_symbol |  X_op_symbol) + X_add_number.  */
    case O_bit_or_not:      /* (X_add_symbol |~ X_op_symbol) + X_add_number.  */
    case O_bit_exclusive_or:/* (X_add_symbol ^  X_op_symbol) + X_add_number.  */
    case O_bit_and:         /* (X_add_symbol &  X_op_symbol) + X_add_number.  */
    case O_eq:              /* (X_add_symbol == X_op_symbol) + X_add_number.  */
    case O_ne:              /* (X_add_symbol != X_op_symbol) + X_add_number.  */
    case O_lt:              /* (X_add_symbol <  X_op_symbol) + X_add_number.  */
    case O_le:              /* (X_add_symbol <= X_op_symbol) + X_add_number.  */
    case O_ge:              /* (X_add_symbol >= X_op_symbol) + X_add_number.  */
    case O_gt:              /* (X_add_symbol >  X_op_symbol) + X_add_number.  */
    case O_logical_and:     /* (X_add_symbol && X_op_symbol) + X_add_number.  */
    case O_logical_or:      /* (X_add_symbol || X_op_symbol) + X_add_number.  */
      return (expression1->X_add_symbol == expression2->X_add_symbol &&
              expression1->X_op_symbol  == expression2->X_op_symbol  &&
              expression1->X_add_number == expression2->X_add_number);
    case O_register:        /* A register (X_add_number is register number).  */
      return (expression1->X_add_number == expression2->X_add_number);
    case O_illegal:         /* An illegal expression.  */
    case O_absent:          /* A nonexistent expression.  */
      // absent and illegal expressions are not equal.
      return false;
    case O_symbol_rva:      /* X_add_symbol + X_add_number - the base address of the image.  */
        // TODO(martint): Look at this case
      return false;
      /* A big value.  If X_add_number is negative or 0, the value is in
         generic_floating_point_number.  Otherwise the value is in
         generic_bignum, and X_add_number is the number of LITTLENUMs in
         the value.  */
    case O_big:
        // TODO(martint): Look at this case
        return false;
    case O_uminus:           /* (- X_add_symbol) + X_add_number.  */
    case O_bit_not:          /* (~ X_add_symbol) + X_add_number.  */
    case O_logical_not:      /* (! X_add_symbol) + X_add_number.  */
      return (expression1->X_add_symbol == expression2->X_add_symbol &&
              expression1->X_add_number == expression2->X_add_number);
    case O_index:             /* X_op_symbol [ X_add_symbol ] */
      return (expression1->X_add_symbol == expression2->X_add_symbol &&
              expression1->X_op_symbol  == expression2->X_op_symbol);
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
      // Fallback on a full compare for machine dependent expressions.
      return memcmp(expression1, expression2, sizeof(expressionS)) == 0;
    case O_max:      /* this must be the largest value */
      MAO_ASSERT_MSG(false, "Invalid expression in compare.");
    default:
      MAO_ASSERT_MSG(false, "Invalid expression in compare.");
  }
  MAO_ASSERT(false);
  return false;
}

void InstructionEntry::SetOperand(int op1,
                                  InstructionEntry *insn2,
                                  int op2) {
  i386_insn *i1 = instruction();
  i386_insn *i2 = insn2->instruction();

  memcpy(&i1->types[op1], &i2->types[op2], sizeof(i386_operand_type));
  i1->flags[op1] = i2->flags[op2];
  if (insn2->IsImmediateOperand(op2))
    i1->op[op1].imms = CreateExpressionCopy(i2->op[op2].imms);
  if (insn2->IsRegisterOperand(op2))
    i1->op[op1].regs = i2->op[op2].regs;
  if (insn2->IsMemOperand(op2))
    i1->op[op1].disps = CreateExpressionCopy(i2->op[op2].disps);
  if (insn2->IsMemOperand(op2)) {
    i1->base_reg = i2->base_reg;
    i1->index_reg = i2->index_reg;
    i1->log2_scale_factor = i2->log2_scale_factor;
  }
  i1->reloc[op1] = i2->reloc[op2];
}

bool InstructionEntry::CompareMemOperand(int op1,
                                         InstructionEntry *insn2,
                                         int op2) {
  i386_insn *i1 = instruction();
  i386_insn *i2 = insn2->instruction();

  if (memcmp(&i1->types[op1], &i2->types[op2], sizeof(i386_operand_type)))
    return false;
  if (i1->flags[op1] != i2->flags[op2])
    return false;

  if (i1->types[op1].bitfield.disp8 ||
      i1->types[op1].bitfield.disp16 ||
      i1->types[op1].bitfield.disp32 ||
      i1->types[op1].bitfield.disp32s ||
      i1->types[op1].bitfield.disp64 ||
      i2->types[op2].bitfield.disp8 ||
      i2->types[op2].bitfield.disp16 ||
      i2->types[op2].bitfield.disp32 ||
      i2->types[op2].bitfield.disp32s ||
      i2->types[op2].bitfield.disp64) {
    const expressionS *disp1 = i1->op[op1].disps;
    const expressionS *disp2 = i2->op[op2].disps;
    if (disp1 && !disp2) return false;
    if (!disp1 && disp2) return false;
    if (disp1 && disp2) {
      if (disp1->X_op != disp2->X_op) return false;
      if (disp1->X_op == O_constant &&
          disp1->X_add_number != disp2->X_add_number)
        return false;
      if (disp1->X_op == O_symbol &&
          strcmp(S_GET_NAME(disp1->X_add_symbol),
                 S_GET_NAME(disp2->X_add_symbol)))
        return false;
      // TODO: Check RelocString
      if (disp1->X_op == O_symbol &&
          disp1->X_add_number != disp2->X_add_number)
        return false;
      if (disp1->X_op == O_subtract) {
        if (disp1->X_add_symbol &&
            disp1->X_add_symbol != disp2->X_add_symbol)
          return false;
        if (disp1->X_op_symbol &&
            strcmp(S_GET_NAME(disp1->X_op_symbol),
                   S_GET_NAME(disp2->X_op_symbol)))
          return false;
        if (disp1->X_add_number != disp2->X_add_number)
          return false;
      }
    }
  }

  if (i1->base_reg != i2->base_reg)
    return false;

  if (i1->index_reg != i2->index_reg)
    return false;

  if (i1->log2_scale_factor != i2->log2_scale_factor)
    return false;

  return true;
}

// segment-override:signed-offset(base,index,scale)
std::string &InstructionEntry::MemoryOperandToString(std::string *out,
                                                     int op_index) const {
  std::ostringstream string_stream;

  // Find out the correct segment index. The index is based on the number
  // of memory operands in the instruction.
  int seg_index = op_index;
  for (int i = 0; i < op_index; i++) {
    if (!IsMemOperand(instruction_, i)) {
      seg_index--;
    }
  }

  const i386_operand_type &operand_type = instruction_->types[op_index];
  const enum bfd_reloc_code_real reloc = instruction_->reloc[op_index];
  const expressionS *expr = instruction_->op[op_index].disps;
  const char *segment_override = instruction_->seg[seg_index]?
      instruction_->seg[seg_index]->seg_name:
      0;
  const bool jumpabsolute = instruction_->types[op_index].bitfield.jumpabsolute
      || instruction_->tm.operand_types[op_index].bitfield.jumpabsolute;

  int scale[] = { 1, 2, 4, 8 };

  if (jumpabsolute) {
    string_stream << "*";
  }

  // segment-override:
  if (segment_override) {
    // cmps only allows es as the first segment override. Gas
    // incorrectly gives ds as the segement. We need to work
    // around this here. See tc-i386.c check_string() for details.
    if (instruction_->tm.operand_types[op_index].bitfield.esseg) {
      string_stream << "%es:";
    } else {
      string_stream << "%"
                    << segment_override
                    << ":";
    }
  }

  std::string reloc_string;

  if (operand_type.bitfield.disp8 ||
      operand_type.bitfield.disp16 ||
      operand_type.bitfield.disp32 ||
      operand_type.bitfield.disp32s ||
      operand_type.bitfield.disp64) {
    // Signed-offset:
    switch (expr->X_op) {
      case O_constant:
        /* X_add_number (a constant expression).  */
        string_stream << "("
                      << expr->X_add_number
                      << ")";
        break;
      case O_symbol:
        if (expr->X_add_number)
          string_stream << "(";
        /* X_add_symbol + X_add_number.  */
        if (expr->X_add_symbol) {
          string_stream << GetDotOrSymbol(expr->X_add_symbol)
                        << RelocToString(reloc, &reloc_string);
        }
        if (expr->X_add_number) {
          string_stream << "+"
                        << expr->X_add_number;
          string_stream << ")";
        }
        break;
        /* (X_add_symbol - X_op_symbol) + X_add_number.  */
      case O_subtract:
        if (expr->X_add_symbol || expr->X_op_symbol) {
          string_stream << "(";
        }
        if (expr->X_add_symbol) {
          string_stream << GetDotOrSymbol(expr->X_add_symbol)
                        << RelocToString(reloc, &reloc_string);
        }
        // When GOTPCREL is used, the second symbol is implicit and
        // should not be printed.
        if (reloc != BFD_RELOC_32_PCREL && reloc != BFD_RELOC_32) {
          if (expr->X_op_symbol) {
            string_stream << "-"
                          << GetDotOrSymbol(expr->X_op_symbol);
          }
        }
        if (expr->X_add_symbol || expr->X_op_symbol) {
          string_stream << ")";
          if (expr->X_add_number)
            string_stream << "+";
        }
        if (expr->X_add_number) {
          string_stream << expr->X_add_number;
        }
        break;
      default:
        MAO_ASSERT_MSG(0, "Unable to print unsupported expression: %d",
                       expr->X_op);
    }
  }
  // (base,index,scale)

  // The gas structure only holds on base_reg (the last one parsed in the
  // instruction). For instructions with multiple base_regs (movs, ..?)
  // this means that we have manually print out the correct register name.
  // movs always have si/esi as the first register (depending on the mode).
  // See the Intel Manual vol 2a specifics on movs instructions.
  const char *base_reg_name = 0;
  if (op_index == 0 && instruction_->operands > 1 &&
      (instruction_->types[0].bitfield.baseindex ||
       instruction_->types[0].bitfield.inoutportreg) &&
      (instruction_->types[1].bitfield.baseindex ||
       instruction_->types[1].bitfield.inoutportreg)) {
    // If the instruction has a prefix, we need to change
    // the register name accordingly.
    // e.g.: movsw  %cs:(%si),%es:(%di)
    //       outsb  %ds:(%esi),   (%dx)
    // rep   cmpsb      (%edi),   (%esi)
    // One could either check:
    //    instruction_->tm.operand_types[op_index].bitfield.disp32
    // or:
    //    HasPrefix(ADDR_PREFIX_OPCODE)

    // Depending on the OP, we can determine the actual register.
    //  CMPS  -> %(e)di
    //  Ohter -> %(e)si

    if (op() == OP_cmps &&
        instruction_->tm.operand_types[op_index].bitfield.disp32) {
      base_reg_name = "edi";
    } else if (op() == OP_cmps &&
               !instruction_->tm.operand_types[op_index].bitfield.disp32) {
      base_reg_name = "di";
    } else if (instruction_->tm.operand_types[op_index].bitfield.disp32) {
      base_reg_name = "esi";
    } else {
      base_reg_name = "si";
    }
  } else if (instruction_->base_reg) {
    base_reg_name = instruction_->base_reg->reg_name;
  }


  if (instruction_->base_reg || instruction_->index_reg)
    string_stream << "(";
  if (instruction_->base_reg)
    string_stream << "%"
                  << base_reg_name;
  if (instruction_->index_reg)
    string_stream << ",%"
                  << instruction_->index_reg->reg_name;
  if (instruction_->log2_scale_factor)
    string_stream << ","
                  << scale[instruction_->log2_scale_factor];
  if (instruction_->base_reg || instruction_->index_reg)
    string_stream << ")";

  out->append(string_stream.str());
  return *out;
}

bool InstructionEntry::HasBaseRegister() const {
  return instruction_->base_reg != NULL;
}

bool InstructionEntry::HasIndexRegister() const {
  return instruction_->index_reg != NULL;
}

const char *InstructionEntry::GetBaseRegisterStr() const {
  return instruction_->base_reg ? instruction_->base_reg->reg_name : NULL;
}
const char *InstructionEntry::GetIndexRegisterStr() const {
  return instruction_->index_reg ? instruction_->index_reg->reg_name : NULL;
}
const reg_entry *InstructionEntry::GetBaseRegister() const {
  return instruction_->base_reg;
}
const reg_entry *InstructionEntry::GetIndexRegister() const {
  return instruction_->index_reg;
}


unsigned int InstructionEntry::GetLog2ScaleFactor() {
  return instruction_->log2_scale_factor;
}


const std::string InstructionEntry::GetAssemblyInstructionName() const {
  #define XMMWORD_MNEM_SUFFIX  'x'
  #define YMMWORD_MNEM_SUFFIX 'y'

  // This prefix is found for some Intel syntax
  // instructions. See tc-i386.c for more info.
  #ifndef LONG_DOUBLE_MNEM_SUFFIX
  #define LONG_DOUBLE_MNEM_SUFFIX '\1'
  #endif

  if (instruction_->suffix == 0) {
    return instruction_->tm.name;
  }

  if (instruction_->suffix == LONG_DOUBLE_MNEM_SUFFIX) {
    std::string s = instruction_->tm.name;
    s.insert(s.begin(), 'l');
    return s;
  }

  const MaoOpcode opcode_needs_y_suffix[] = {
    // AVX Instructions from gas test-suite:
    OP_vcvtpd2dq, OP_vcvtpd2ps, OP_vcvttpd2dq
  };

  const MaoOpcode opcode_has_l_suffix[] =  {
    OP_movsbl, OP_movswl, OP_movzbl, OP_movzwl, OP_movswl, OP_cmovl, OP_cmovnl,
    OP_cwtl, OP_cltd, OP_movbe
  };
  const MaoOpcode opcode_has_w_suffix[] =  {
    OP_cbtw, OP_fnstsw, OP_movsbw
  };
  const MaoOpcode opcode_has_b_suffix[] =  {
    OP_setb
  };
  const MaoOpcode keep_sse4_2_suffix[] =  {
    OP_crc32
  };

  const MaoOpcode supress_suffix[] =  {
    // Misc instructions from the gas testsuite.
    OP_invept, OP_movd, OP_cmpxchg16b, OP_vmptrld, OP_vmclear, OP_vmxon,
    OP_vmptrst, OP_ldmxcsr, OP_stmxcsr, OP_clflush, OP_addsubps, OP_cvtpd2dq,
    OP_comiss, OP_cvttps2dq, OP_haddps, OP_movdqu, OP_movshdup, OP_pshufhw,
    OP_movsldup, OP_pshuflw, OP_punpcklbw, OP_unpckhpd, OP_paddq, OP_psubq,
    OP_pmuludq, OP_punpckldq, OP_punpcklqdq, OP_unpckhps, OP_punpcklwd,
    OP_cmpeqss, OP_ucomiss, OP_cvtss2sd, OP_divss, OP_minss, OP_maxss,
    OP_movntss, OP_movss, OP_rcpss, OP_rsqrtss, OP_sqrtss, OP_subss,
    OP_unpcklpd, OP_mulss, OP_unpcklps, OP_cmpss, OP_vmovd, OP_vextractps,
    OP_vpextrb, OP_vpinsrb, OP_vpextrd, OP_cqto, OP_jecxz,
    // AVX Instructions from gas test-suite:
    OP_vldmxcsr, OP_vstmxcsr, OP_vaddss, OP_vdivss, OP_vcvtss2sd, OP_vmaxss,
    OP_vminss, OP_vmulss, OP_vrcpss, OP_vrsqrtss, OP_vsqrtss, OP_vsubss,
    OP_vcmpeqss, OP_vcmpltss, OP_vcmpless, OP_vcmpunordss, OP_vcmpneqss,
    OP_vcmpnltss, OP_vcmpnless, OP_vcmpordss, OP_vcmpeq_uqss, OP_vcmpngess,
    OP_vcmpngtss, OP_vcmpfalsess, OP_vcmpneq_oqss, OP_vcmpgess, OP_vcmpgtss,
    OP_vcmptruess, OP_vcmpeq_osss, OP_vcmplt_oqss, OP_vcmple_oqss,
    OP_vcmpunord_sss, OP_vcmpneq_usss, OP_vcmpnlt_uqss, OP_vcmpnle_uqss,
    OP_vcmpord_sss, OP_vcmpeq_usss, OP_vcmpnge_uqss, OP_vcmpngt_uqss,
    OP_vcmpfalse_osss, OP_vcmpneq_osss, OP_vcmpge_oqss, OP_vcmpgt_oqss,
    OP_vcmptrue_usss, OP_vbroadcastss, OP_vcomiss, OP_vpmovsxbd, OP_vpmovsxwq,
    OP_vpmovzxbd, OP_vpmovzxwq, OP_vucomiss, OP_vmovss, OP_vmovss, OP_vcmpss,
    OP_vinsertps, OP_vroundss, OP_vfmaddss, OP_vfmaddss, OP_vfmsubss,
    OP_vfmsubss, OP_vfnmaddss, OP_vfnmsubss, OP_vfnmsubss, OP_vpmovsxbq,
    OP_vpmovzxbq, OP_vpextrw, OP_vpextrw, OP_vpinsrw, OP_vpinsrw,
    // CBW instructions
    OP_cbw, OP_cwde, OP_cdqe, OP_cwd, OP_cdq, OP_cqo,
    // From x86-64-ept.s
    OP_invvpid,
    // From x86-64-prescott.s
    OP_monitor, OP_mwait,
    // From general.s
    OP_movzbw,
    // From i386.s
    OP_fstsw,
    // From opcode.s
    OP_cwtd,
    // From svme.s
    OP_skinit, OP_vmload, OP_vmsave, OP_invlpga, OP_vmrun
  };

  if ((instruction_->suffix == XMMWORD_MNEM_SUFFIX ||
       instruction_->suffix == YMMWORD_MNEM_SUFFIX) &&
      !IsInList(op(), opcode_needs_y_suffix,
                sizeof(opcode_needs_y_suffix)/sizeof(MaoOpcode))) {
    return instruction_->tm.name;
  }

  if ((instruction_->suffix == 'l') &&
      IsInList(op(), opcode_has_l_suffix,
               sizeof(opcode_has_l_suffix)/sizeof(MaoOpcode))) {
    return instruction_->tm.name;
  }
  if ((instruction_->suffix == 'w') &&
      IsInList(op(), opcode_has_w_suffix,
               sizeof(opcode_has_w_suffix)/sizeof(MaoOpcode))) {
    return instruction_->tm.name;
  }
  if ((instruction_->suffix == 'b') &&
      IsInList(op(), opcode_has_b_suffix,
               sizeof(opcode_has_b_suffix)/sizeof(MaoOpcode))) {
    return instruction_->tm.name;
  }
  if (instruction_->suffix == 'q' &&
      instruction_->tm.name[strlen(instruction_->tm.name)-1] == 'q') {
    return instruction_->tm.name;
  }

  // Do not print suffix for cpusse4_1 instructions
  //  e.g.: OP_extractps, OP_pextrb, OP_pextrd, OP_pinsrb, OP_pinsrd
  if (instruction_->tm.cpu_flags.bitfield.cpusse4_1) {
    return instruction_->tm.name;
  }
  // Do not print suffix for cpusse4_2 instructions
  // except for OP_crc32
  if (instruction_->tm.cpu_flags.bitfield.cpusse4_2 &&
      !IsInList(op(), keep_sse4_2_suffix,
               sizeof(keep_sse4_2_suffix)/sizeof(MaoOpcode))) {
    return instruction_->tm.name;
  }

  // These instruction have valid suffix information
  // in tm.opcode_modifier.no_Xsuf
  // Use it when printing instead of the .suffix
  if (op() == OP_movsx || op() == OP_movzx) {
    // Handle movsx and movzx, found in i386.c in the gas
    // test suite
    std::string s = instruction_->tm.name;
    // Make sure max one suffix is listed in the template.
    MAO_ASSERT((!instruction_->tm.opcode_modifier.no_bsuf +
                !instruction_->tm.opcode_modifier.no_wsuf +
                !instruction_->tm.opcode_modifier.no_lsuf +
                !instruction_->tm.opcode_modifier.no_ssuf +
                !instruction_->tm.opcode_modifier.no_qsuf +
                !instruction_->tm.opcode_modifier.no_ldsuf) < 2);
    if (!instruction_->tm.opcode_modifier.no_bsuf) s.append("b");
    if (!instruction_->tm.opcode_modifier.no_wsuf) s.append("w");
    if (!instruction_->tm.opcode_modifier.no_lsuf) s.append("l");
    if (!instruction_->tm.opcode_modifier.no_ssuf) s.append("s");
    if (!instruction_->tm.opcode_modifier.no_qsuf) s.append("q");
    if (!instruction_->tm.opcode_modifier.no_ldsuf)s.append("ld");
    return s;
  }



  if (IsInList(op(), supress_suffix,
               sizeof(supress_suffix)/sizeof(MaoOpcode))) {
    return instruction_->tm.name;
  }

  std::string s = instruction_->tm.name;
  s.insert(s.end(), instruction_->suffix);
  return s;
}

std::string &InstructionEntry::PrintRexPrefix(std::string *out,
                                              int prefix) const {
  const char *rex_arr[] = {"rex", "rexz", "rexy", "rexyz", "rexx",
                           "rexxz", "rexxy", "rexxyz", "rex64",
                           "rex64z", "rex64y", "rex64yz", "rex64x",
                           "rex64xz", "rex64xy", "rex64xyz"};

  MAO_ASSERT_MSG(prefix >= REX_OPCODE &&
                 prefix < REX_OPCODE+16,
                 "Error in REX prefix: %d\n", prefix);

  out->append(rex_arr[prefix-REX_OPCODE]);
  out->append(" ");
  return *out;
}


bool InstructionEntry::HasPrefix(char prefix) const {
  if (instruction_->prefixes > 0) {
    for (unsigned int i = 0;
         i < sizeof(instruction_->prefix)/sizeof(unsigned char);
         ++i) {
      if (instruction_->prefix[i] == prefix) {
        return true;
      }
    }
  }
  return false;
}

// If the register name cr8..15 is used, the lock prefix is implicit.
bool InstructionEntry::SuppressLockPrefix() const {
  for (unsigned int op_index = 0;
      op_index < instruction_->operands;
      op_index++) {
    if (instruction_->types[op_index].bitfield.control &&
        instruction_->op[op_index].regs->reg_flags != 0) {
      return true;
    }
  }
  return false;
}

// Removes prefixes from the prefix byte that are implicit from the instruction.
int InstructionEntry::StripRexPrefix(int prefix) const {
  int stripped_prefix = prefix;

  // there are 4 bits that we want to track
  // - rex.w REX_W
  // - rex.r REX_R
  // - rex.x REX_X
  // - rex.b REX_B

  // If none of the bits are given, return unmodified.
  if (stripped_prefix == REX_OPCODE) {
    return prefix;
  }

  // No prefixes for NOP instructions
  if (op() == OP_nop) {
    return 0;
  }

  // No prefixes for multimedia instructions
  if (instruction_->tm.cpu_flags.bitfield.cpuavx ||
      instruction_->tm.cpu_flags.bitfield.cpusse ||
      instruction_->tm.cpu_flags.bitfield.cpusse2 ||
      instruction_->tm.cpu_flags.bitfield.cpusse3 ||
      instruction_->tm.cpu_flags.bitfield.cpusse4_1 ||
      instruction_->tm.cpu_flags.bitfield.cpusse4_2 ||
      instruction_->tm.cpu_flags.bitfield.cpusse5 ) {
    return 0;
  }

  // Check for the four bits, and remove them if they are unnecessary

  if ((stripped_prefix-REX_OPCODE) & REX_W) {
    // Make sure xchange-based noops are not stripped of their rex prefix
    if (op() == OP_xchg &&
        instruction_->operands == 2 &&
        IsRegisterOperand(instruction_, 0) &&
        IsRegisterOperand(instruction_, 1) &&
        instruction_->op[0].regs == instruction_->op[1].regs) {
    } else if (instruction_->suffix == 'q') {
      // Remove prefixes for instructions with the q suffix
      stripped_prefix = ((stripped_prefix-REX_OPCODE) & ~REX_W)+REX_OPCODE;
    }
  }

  if ((stripped_prefix-REX_OPCODE) & REX_R) {
    // Does this instruction have a register
    for (unsigned int i = 0; i < instruction_->operands; ++i) {
      if (IsRegisterOperand(instruction_, i)) {
        stripped_prefix = ((stripped_prefix-REX_OPCODE) & ~REX_R)+REX_OPCODE;
      }
    }
  }
  if ((stripped_prefix-REX_OPCODE) & REX_X) {
    // Does this instruction have a register
    for (unsigned int i = 0; i < instruction_->operands; ++i) {
      if (IsMemOperand(instruction_, i)) {
        stripped_prefix = ((stripped_prefix-REX_OPCODE) & ~REX_X)+REX_OPCODE;
      }
    }
  }
  if ((stripped_prefix-REX_OPCODE) & REX_B) {
    // Special case with xchg. e.g. xchg %rax,%r8
    if (op() == OP_xchg) {
      stripped_prefix = ((stripped_prefix-REX_OPCODE) & ~REX_B)+REX_OPCODE;
    } else {
      for (unsigned int i = 0; i < instruction_->operands; ++i) {
        if (IsMemOperand(instruction_, i) ||
            IsRegisterOperand(instruction_, i)) {
          // Does this instruction have a register
          stripped_prefix = ((stripped_prefix-REX_OPCODE) & ~REX_B)+REX_OPCODE;
        }
      }
    }
  }

  // Stripped away rex prefix if all the bits are removed
  if (stripped_prefix == REX_OPCODE) {
    return 0;
  }

  return stripped_prefix;
}

// Prints out the instruction.
// Goal is to make it print instructions generated by gcc
std::string &InstructionEntry::InstructionToString(std::string *out) const {
  const MaoOpcode rep_ops[] = {OP_ins, OP_outs, OP_movs, OP_lods, OP_stos};
  const MaoOpcode repe_ops[]= {OP_cmps, OP_scas};


  const MaoOpcode via_padlock_ops[] = {OP_xstorerng, OP_xcryptecb, OP_xcryptcbc,
                                       OP_xcryptcfb, OP_xcryptofb, OP_xstore,
                                       OP_montmul,   OP_xsha1,     OP_xsha256};

  // Prefixes
  out->append("\t");
  if (instruction_->prefixes > 0) {
    for (unsigned int i = 0;
         i < sizeof(instruction_->prefix)/sizeof(unsigned char);
         ++i) {
      // The prefixes in the instruction is stored in a array of size 6:
      //    These are the indexes:
      //    WAIT_PREFIX     0,
      //    SEG_PREFIX      1
      //    ADDR_PREFIX     2
      //    DATA_PREFIX     3
      //    LOCKREP_PREFIX  4
      //    REX_PREFIX      5

      if (instruction_->prefix[i] != 0) {
        // The list of available prefixes can be found in the following file:
        // ../binutils-2.19/include/opcode/i386.h
        switch (instruction_->prefix[i]) {
          // http://www.intel.com/software/products/documentation/vlin/mergedprojects/analyzer_ec/mergedprojects/reference_olh/mergedProjects/instructions/instruct32_hh/vc276.htm
          // Repeats a string instruction the number of times specified in the
          // count register ((E)CX) or until the indicated condition of the ZF
          // flag is no longer met.
          // REP (repeat)                                 ins: INS, OUTS, MOVS
          //                                                   LODS, STOS
          // REPE,  REPZ (repeat while equal, zero)          ins: CMPS, SCAS
          // REPNE, REPNZ (repeat while not equal, not zero) ins: CMPS, SCAS
          case REPNE_PREFIX_OPCODE:
            if (IsInList(op(), repe_ops, sizeof(repe_ops)/sizeof(MaoOpcode))) {
              out->append("repne ");
            } else if (IsInList(op(), rep_ops,
                                sizeof(repe_ops)/sizeof(MaoOpcode))) {
              MAO_ASSERT_MSG(false,
                             "Found prefix does not match the instruction.");
            }
            break;
          case REPE_PREFIX_OPCODE:
            if (IsInList(op(), repe_ops, sizeof(repe_ops)/sizeof(MaoOpcode))) {
              out->append("repe ");
            } else if (IsInList(op(), rep_ops,
                                sizeof(rep_ops)/sizeof(MaoOpcode))) {
              out->append("rep ");
            } else if (IsInList(op(), via_padlock_ops,
                                sizeof(via_padlock_ops)/sizeof(MaoOpcode))) {
              out->append("rep ");
            }
            break;
          // Rex prefixes are used for 64-bit extention
          // There are 4 rex-bits -> 16 rex versions
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
          case REX_OPCODE+15: {
            // Remove prefixes that are implicit in the instruction
            int stripped_prefix = StripRexPrefix(instruction_->prefix[i]);
            // Print the remaining
            if (stripped_prefix != 0) {
              PrintRexPrefix(out, stripped_prefix);
            }
            break;
          }
          case DATA_PREFIX_OPCODE:  // e.g. : cmpw    %cx, %ax
            break;
          case CS_PREFIX_OPCODE:
          case DS_PREFIX_OPCODE:
          case ES_PREFIX_OPCODE:
          case FS_PREFIX_OPCODE:
          case GS_PREFIX_OPCODE:
          case SS_PREFIX_OPCODE:
            break;
          case ADDR_PREFIX_OPCODE:
            // used in movl (%eax), %eax
            //  addr32 lea symbol,%rax
            switch (code_flag_) {
              case  CODE_16BIT:
                break;
              case  CODE_32BIT:
                out->append("addr16  ");
                break;
              case  CODE_64BIT:
                out->append("addr32  ");
                break;
              default:
                MAO_ASSERT_MSG(false, "Fould illegal prefix.");
                break;
            }
            break;
          case LOCK_PREFIX_OPCODE:
            // used in  lock xaddl        %eax, 16(%rdi)

            // If an instruction has a control-register as operand,
            // the lock prefix should not be printed.
            if (!SuppressLockPrefix()) {
              out->append("lock ");
            }
            break;
          case FWAIT_OPCODE:
            // Found in fstsw instruction
            break;
          default:
            MAO_ASSERT_MSG(false, "Unknown prefix found 0x%x\n",
                           instruction_->prefix[i]);
        }
      }
    }
  }

  // Gets the name of the assembly instruction, including
  // suffixes.
  out->append(GetAssemblyInstructionName());
  out->append("\t");

  // Loop over operands
  int num_operands = instruction_->operands;
  if (instruction_->tm.opcode_modifier.vex3sources) {
    if (!instruction_->types[0].bitfield.imm8)
      num_operands = instruction_->operands - 1;
  }
  if (!instruction_->tm.opcode_modifier.vex3sources) {
    // This takes care of instructions
    // that have opcode modifiers stored where the immeage
    // normaly is stored (SSE, SSE2, AMD 3D Now).
    if (instruction_->tm.opcode_modifier.immext) {
      num_operands = instruction_->operands - 1;
    }
  }

  for (int i = 0; i < num_operands; ++i) {
    // Handle case of instruction which have 4 operands
    // according to the instruction structure, but only
    // three in the assembly (e.g. comeqss %xmm3, %xmm2, %xmm1)
    if (i == 3 &&
        instruction_->tm.opcode_modifier.drexc &&
        instruction_->tm.extension_opcode == 65535) {
      break;
    }

    if (i > 0)
      out->append(", ");

    // IMMEDIATE
    if (IsImmediateOperand(instruction_, i)) {
      // This code makes sure that instructions like this
      // gets the correct immediate. They seem to encode data into the
      // immediates higher bits.
      // vpermil2ps $10,(%rcx),%ymm6,%ymm2 ,%ymm7
      if (instruction_->tm.opcode_modifier.vex3sources &&
          instruction_->imm_operands != 0) {
        int mask = 0xF;
        instruction_->op[i].imms->X_add_number  &= mask;
      }

      // cpusse4a instruction swap the first two operands!
      //  extrq   $4,$2,%xmm1
      if (instruction_->tm.cpu_flags.bitfield.cpusse4a &&
          num_operands > 2 &&
          i == 0 &&
          IsImmediateOperand(instruction_, 1)) {
        ImmediateOperandToString(out,
                                 instruction_->reloc[1],
                                 instruction_->op[1].imms);

      } else if (instruction_->tm.cpu_flags.bitfield.cpusse4a &&
                 num_operands > 2 &&
                 i == 1 &&
                 IsImmediateOperand(instruction_, 0)) {
        ImmediateOperandToString(out,
                                 instruction_->reloc[0],
                                 instruction_->op[0].imms);
      } else {
        ImmediateOperandToString(out,
                                 instruction_->reloc[i],
                                 instruction_->op[i].imms);
      }
    }

    // MEMORY OPERANDS
    if (IsMemOperand(instruction_, i)) {
      MemoryOperandToString(out, i);
    }

    // ACC register
    if (instruction_->types[i].bitfield.floatacc) {
      out->append("%st");
    }

    // REGISTERS

    // The various SSE5 formats. Tested on
    // x86-64-sse5.s in gas test-suite.
    if (instruction_->tm.opcode_modifier.drex &&
        instruction_->tm.opcode_modifier.drexv) {
      if ((instruction_->tm.extension_opcode == 0 &&
           (i == 2 || i == 3)) ||
          (instruction_->tm.extension_opcode == 1 &&
           (i == 2 || i == 3)) ||
          (instruction_->tm.extension_opcode == 2 &&
           (i == 0 || i == 3)) ||
          (instruction_->tm.extension_opcode == 3 &&
           (i == 0 || i == 3))) {
        out->append("%");
        out->append(instruction_->op[i].regs->reg_name);
      }
    }
    if (instruction_->tm.opcode_modifier.drex &&
        !instruction_->tm.opcode_modifier.drexv) {
      if ((instruction_->tm.extension_opcode == 0 &&
           (i == 0 || i == 3)) ||
          (instruction_->tm.extension_opcode == 1 &&
           (i == 2 || i == 3)) ||
          (instruction_->tm.extension_opcode == 2 &&
           (i == 0 || i == 3)) ||
          (instruction_->tm.extension_opcode == 3 &&
           (i == 0 || i == 3))) {
        out->append("%");
        out->append(instruction_->op[i].regs->reg_name);
      }
    }
    if (instruction_->tm.opcode_modifier.drexc) {
      if ((instruction_->tm.extension_opcode == 0 &&
           (i == 3)) ||
          (instruction_->tm.extension_opcode == 65535 &&
           (i == 2))) {
        out->append("%");
        out->append(instruction_->op[i].regs->reg_name);
      }
    }

    // If a jmp register instruction is given using
    // intel syntax, the jumpabsolute bit is not set using
    // gas 2.19. The if-below is a workaround for this.
    if (IsRegisterOperand(instruction_, i)) {
      if (instruction_->types[i].bitfield.jumpabsolute ||
          ((IsCall() || IsJump()) &&
           (instruction_->types[i].bitfield.reg8 ||
            instruction_->types[i].bitfield.reg16 ||
            instruction_->types[i].bitfield.reg32 ||
            instruction_->types[i].bitfield.reg64))) {
        out->append("*");
      }
      out->append("%");
      out->append(instruction_->op[i].regs->reg_name);
    }

    // Handle spacial case found in tc-i386.c:7326
    if (instruction_->types[i].bitfield.inoutportreg) {
      // its a register name!
      out->append("(%dx)");
    }

    if (instruction_->types[i].bitfield.shiftcount) {
      // its a register name!
      out->append("%");
      out->append(instruction_->op[i].regs->reg_name);
    }
  }
  return *out;
}

std::string &InstructionEntry::ProfileToString(std::string *out) const {
  std::ostringstream string_stream;
  if (execution_count_valid_)
    string_stream << "\t# ecount=" << execution_count_;
  out->append(string_stream.str());
  return *out;
}

// From an instruction given by gas, allocate new memory and populate the
// members.
i386_insn *InstructionEntry::CreateInstructionCopy(i386_insn *in_inst) {
  i386_insn *new_inst = new i386_insn;
  MAO_ASSERT(new_inst);

  // Copy all non-pointer data
  memcpy(new_inst, in_inst, sizeof(i386_insn));

  // Copy references
  for (unsigned int i = 0; i < new_inst->operands; i++) {
    // Select the correct part of the operand union.
    if (IsImmediateOperand(in_inst, i)) {
      new_inst->op[i].imms = new expressionS;
      MAO_ASSERT(new_inst->op[i].imms);
      *new_inst->op[i].imms = *in_inst->op[i].imms;
    } else if (IsMemOperand(in_inst, i) && in_inst->op[i].disps) {
      new_inst->op[i].disps = new expressionS;
      MAO_ASSERT(new_inst->op[i].disps);
      *new_inst->op[i].disps = *in_inst->op[i].disps;
    } else if (IsRegisterOperand(in_inst, i) ||
              in_inst->types[i].bitfield.shiftcount ) {
      new_inst->op[i].regs = in_inst->op[i].regs;
    }
  }
  new_inst->base_reg = in_inst->base_reg;
  new_inst->index_reg = in_inst->index_reg;

  // Segment overrides
  for (unsigned int i = 0; i < 2; i++) {
    if (in_inst->seg[i]) {
      seg_entry *tmp_seg = new seg_entry;
      MAO_ASSERT(tmp_seg);
      MAO_ASSERT(strlen(in_inst->seg[i]->seg_name) < MAX_SEGMENT_NAME_LENGTH);
      tmp_seg->seg_name = strdup(in_inst->seg[i]->seg_name);
      tmp_seg->seg_prefix = in_inst->seg[i]->seg_prefix;
      new_inst->seg[i] = tmp_seg;
    }
  }

  return new_inst;
}

bool InstructionEntry::IsInList(MaoOpcode opcode, const MaoOpcode list[],
                              const unsigned int number_of_elements) const {
  for (unsigned int i = 0; i < number_of_elements; i++) {
    if (opcode == list[i])
      return true;
  }
  return false;
}


bool InstructionEntry::HasFallThrough() const {
  if (IsReturn()) return false;
  if (!HasTarget()) return true;
  if (IsCall()) return true;
  if (IsCondJump()) return true;
  return false;
}

bool InstructionEntry::HasTarget() const {
  const MaoOpcode insn[] = {OP_jmp, OP_ljmp};
  if (IsInList(op(), insn, sizeof(insn)/sizeof(MaoOpcode)))
    return true;
  if (IsCondJump())
    return true;

  return false;
}

const char *InstructionEntry::GetTarget() const {
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


bool InstructionEntry::IsJump() const {
  const MaoOpcode jumps[] = {
    OP_jmp, OP_ljmp
  };
  return IsInList(op(), jumps, sizeof(jumps)/sizeof(MaoOpcode));
}

bool InstructionEntry::IsIndirectJump() const {
  // Jump instructions always have one operand
  MAO_ASSERT(!IsJump() || instruction_->operands == 1);
  return IsJump() && (instruction_->types[0].bitfield.baseindex ||
                      IsRegisterOperand(instruction_, 0));
}


bool InstructionEntry::IsCondJump() const {
  static const MaoOpcode cond_jumps[] = {
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
  return IsInList(op(), cond_jumps, sizeof(cond_jumps)/sizeof(MaoOpcode));
}

bool InstructionEntry::IsCall() const {
  const MaoOpcode calls[] = {
    OP_call, OP_lcall, OP_vmcall, OP_syscall, OP_vmmcall
  };
  return IsInList(op(), calls, sizeof(calls)/sizeof(MaoOpcode));
}

bool InstructionEntry::IsReturn() const {
  const MaoOpcode rets[] = {
    OP_ret, OP_lret, OP_retf, OP_iret, OP_sysret
  };
  return IsInList(op(), rets, sizeof(rets)/sizeof(MaoOpcode));
}

bool InstructionEntry::IsAdd() const {
  return op() == OP_add;;
}


//
// Class: SubSection
//

void SubSection::set_last_entry(MaoEntry *entry) {
  // Link the entries, unless we insert the first entry. This special case is
  // handled in AddEntry().
  if (entry != first_entry_) {
    last_entry_->set_next(entry);
    entry->set_prev(last_entry_);
  }
  last_entry_ = entry;
}


SectionEntryIterator SubSection::EntryBegin() {
  return SectionEntryIterator(first_entry());
}

SectionEntryIterator SubSection::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return SectionEntryIterator(entry);
}



//
// Class: Section
//

void Section::AddSubSection(SubSection  *subsection) {
  subsections_.push_back(subsection);
}


std::vector<SubSectionID> Section::GetSubsectionIDs() const {
  std::vector<SubSectionID> subsections;
  for (std::vector<SubSection *>::const_iterator ss_iter = subsections_.begin();
       ss_iter != subsections_.end();
       ++ss_iter) {
    subsections.push_back((*ss_iter)->id());
  }
  return subsections;
}

SectionEntryIterator Section::EntryBegin() const {
  if (subsections_.size() == 0)
    return EntryEnd();
  SubSection *ss = subsections_[0];
  return SectionEntryIterator(ss->first_entry());
}

SectionEntryIterator Section::EntryEnd() const {
  return SectionEntryIterator(NULL);
}

SubSection *Section::GetLastSubSection() const {
  if (subsections_.size() == 0) {
    return NULL;
  } else {
    return subsections_[subsections_.size()-1];
  }
}

//
// Class: SectionEntryIterator
//

SectionEntryIterator::SectionEntryIterator(MaoEntry *entry)
    : current_entry_(entry) {
  return;
}

SectionEntryIterator &SectionEntryIterator::operator ++() {
  current_entry_ = current_entry_->next();
  return *this;
}

SectionEntryIterator &SectionEntryIterator::operator --() {
  current_entry_ = current_entry_->prev();
  return *this;
}

bool SectionEntryIterator::operator ==(const SectionEntryIterator &other)
    const {
  return (current_entry_ == other.current_entry_);
}

bool SectionEntryIterator::operator !=(const SectionEntryIterator &other)
    const {
  return !((*this) == other);
}


//
// Class: Function
//

SectionEntryIterator Function::EntryBegin() {
  return SectionEntryIterator(first_entry());
}

SectionEntryIterator Function::EntryEnd() {
  MaoEntry *entry = last_entry();
  if (entry) {
    entry = entry->next();
  }
  return SectionEntryIterator(entry);
}


void Function::Print() {
  Print(stdout);
}

void Function::Print(FILE *out) {
  fprintf(out, "Function: %s\n", name().c_str());
  for (SectionEntryIterator iter = EntryBegin();
       iter != EntryEnd();
       ++iter) {
    MaoEntry *entry = *iter;
    entry->PrintEntry(out);
  }
}

void Function::set_cfg(CFG *cfg) {
  if (cfg_ != NULL) {
    delete cfg_;
  }
  cfg_ = cfg;
}

// Casting functions.
InstructionEntry *MaoEntry::AsInstruction() {
  MAO_ASSERT(IsInstruction());
  return static_cast<InstructionEntry*>(this);
}

LabelEntry *MaoEntry::AsLabel() {
  MAO_ASSERT(IsLabel());
  return static_cast<LabelEntry*>(this);
}

DirectiveEntry *MaoEntry::AsDirective() {
  MAO_ASSERT(IsDirective());
  return static_cast<DirectiveEntry*>(this);
}

InstructionEntry *MaoEntry::nextInstruction() {
  if (next_ && next_->IsInstruction())
    return next_->AsInstruction();
  return NULL;
}

InstructionEntry *MaoEntry::prevInstruction() {
  if (prev_ && prev_->IsInstruction())
    return prev_->AsInstruction();
  return NULL;
}
