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



#include <cstring>
#include <fstream>
#include <iostream>

#include <stdio.h>

#include "ir-gas.h"
#include "irlink.h"
#include "MaoDebug.h"
#include "MaoOptions.h"
#include "MaoUnit.h"

extern "C" const char *S_GET_NAME(symbolS *s);
extern "C" void   as_where (char **, unsigned int *);

// Reference to a the mao_unit. 
MaoUnit *maounit_;

struct link_context_s {
  unsigned int line_number;
  char *filename;
};


struct link_context_s get_link_context() {
  struct link_context_s link_context;
  unsigned int line_no;
  char *file;
  as_where(&file, &line_no);
  link_context.line_number = line_no;
  link_context.filename = file;
  return link_context;
}


void link_insn(i386_insn *insn, size_t SizeOfInsn, const char *line_verbatim) {
  MAO_ASSERT(sizeof(i386_insn) == SizeOfInsn);
  i386_insn *inst = insn;

  struct link_context_s link_context = get_link_context();
  MAO_ASSERT(maounit_);
  maounit_->AddEntry(new InstructionEntry(inst,
                                          link_context.line_number,
                                          line_verbatim), true, true);
}

void link_label(const char *name, const char *line_verbatim) {
  // Add the label to the symbol table.
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  struct link_context_s link_context = get_link_context();
  MAO_ASSERT(maounit_);
  maounit_->AddEntry(new LabelEntry(name, link_context.line_number,
                                    line_verbatim), true, true);
}

void link_symbol(const char *name, enum SymbolVisibility symbol_visibility,
                 const char *line_verbatim) {
  // one of two things happens here.
  // If the symbol exists, then update its visibility.
  // If a symbol does not exists, then create a symbol, but
  // with an undefined section

  // Get table
  MAO_ASSERT(maounit_);
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  // Create symbol if needed
  if (!symbol_table->Exists(name)) {
    symbol_table->Add(name, new Symbol(name));
  }
  // Get symbol
  Symbol *symbol = symbol_table->Find(name);
  MAO_ASSERT(symbol);

  // Update symbol
  symbol->set_symbol_visibility(symbol_visibility);
}

void link_comm(const char *name, unsigned int common_size,
               unsigned int common_align, const char *line_verbatim) {
  MAO_ASSERT(name);
  MAO_ASSERT(maounit_);
  maounit_->AddCommSymbol(name, common_size, common_align);
}


void link_directive(const char *key, const char *value,
                    const char *line_verbatim) {
  MAO_ASSERT(key);
  MAO_ASSERT(value);
  struct link_context_s link_context = get_link_context();
  MAO_ASSERT(maounit_);
  maounit_->AddEntry(new DirectiveEntry(key, value, link_context.line_number,
                                        line_verbatim), true, false);
}


void link_debug(const char *key, const char *value, const char *line_verbatim) {
  MAO_ASSERT(key);
  MAO_ASSERT(value);
  struct link_context_s link_context = get_link_context();
  MAO_ASSERT(maounit_);
  maounit_->AddEntry(new DebugEntry(key, value, link_context.line_number,
                                    line_verbatim), true, false);
}


char is_whitespace(char c) {
  return (c == ' ' || c == '\t');
}

// Get the name of the section from the argument and place the result in the
// buffer returns a pointer to the buffer
char *get_section_name(const char *arguments, char *buffer) {
  MAO_ASSERT(arguments);
  MAO_ASSERT(strlen(arguments) < MAX_SYMBOL_NAME_LENGTH);

  const char *arguments_p = arguments;
  while (is_whitespace(*arguments_p)) {
    arguments_p++;
  }
  char *buffer_p = buffer;
  while (*arguments_p != '\0' && *arguments_p != ',' && *arguments_p != '\n' &&
         !is_whitespace(*arguments_p)) {
    *buffer_p++ = *arguments_p++;
  }
  *buffer_p = '\0';
  return buffer;
}

// A section can be declared in several ways
// .data, .bss, .txt
// or
// .section ARGUMENTS
//
// void link_section(const char *arguments, const char *filename,
//                    unsigned int line_number,
//                    const char *line_verbatim) {
void link_section(const char *name, const char *arguments,
                  const char *create_op,
                  const char *line_verbatim) {
  // make sure the . is stripped away from the name
  MAO_ASSERT(name);
  MAO_ASSERT(name[0] != '.');
  MAO_ASSERT(create_op);
  MAO_ASSERT(maounit_);
  if (strcmp(name, "bss") == 0 ||
      strcmp(name, "text") == 0 ||
      strcmp(name, "data") == 0) {
    // No argument are used for these.
    maounit_->SetSubSection(name, 0, create_op);
  } else if (strcmp(name, "section") == 0) {
    // Get name of section from argument
    MAO_ASSERT(strlen(arguments) < MAX_SYMBOL_NAME_LENGTH);
    char name_buffer[MAX_SYMBOL_NAME_LENGTH];
    get_section_name(arguments, name_buffer);
    MAO_ASSERT(name_buffer);
    maounit_->SetSubSection(name_buffer, 0, create_op);
  } else {
    MAO_RASSERT(0);  // Unknown directive found
  }
}

void link_type(const char *name, SymbolType symbol_type,
               const char *line_verbatim) {
  MAO_ASSERT(maounit_);
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  Symbol *symbol = symbol_table->FindOrCreateAndFind(name);
  MAO_ASSERT(symbol);
  symbol->set_symbol_type(symbol_type);
  return;
}

void link_size(const char *name, unsigned int size, const char *line_verbatim) {
  MAO_ASSERT(name);
  MAO_ASSERT(maounit_);
  SymbolTable *symbol_table = maounit_->GetSymbolTable();
  MAO_ASSERT(symbol_table);
  Symbol *symbol = symbol_table->FindOrCreateAndFind(name);
  MAO_ASSERT(symbol);
  symbol->set_size(size);
  return;
}

void register_mao_unit(MaoUnit *maounit) {
  MAO_ASSERT(maounit);
  maounit_ = maounit;
}
