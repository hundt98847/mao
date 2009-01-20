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


#include <stdio.h>

#include "MaoDebug.h"
#include "MaoUnit.h"

// Class SymbolTable

SymbolTable::SymbolTable() {
  table_.clear();
}

SymbolTable::~SymbolTable() {
  // we should clear the entries here.
  // when added, they are allocated with new!
  for (std::map<const char*, Symbol *, ltstr>::iterator iter = table_.begin();
      iter != table_.end(); ++iter) {
    free(iter->second);
  }
}

bool SymbolTable::Exists(const char *name) {
  std::map<const char*, Symbol *, ltstr>::const_iterator it = table_.find(name);
  return it != table_.end();
}

Symbol *SymbolTable::Add(const char *name, Symbol *symbol) {
  // the name is in the symbol?
  //  table_[name] = symbol;
  table_[symbol->name()] = symbol;
  return symbol;
}

void SymbolTable::Print() const {
  Print(stdout);
}

void SymbolTable::Print(FILE *out) const {
  for (std::map<const char*, Symbol *, ltstr>::const_iterator iter =
           table_.begin();
      iter != table_.end(); ++iter) {
    fprintf(out, "\t# ");
    fprintf(out, " %10s", iter->first);
    fprintf(out, " type=%c",
            iter->second->symbol_type() == OBJECT_SYMBOL?'O':(
                iter->second->symbol_type() == FUNCTION_SYMBOL?'F':(
                    iter->second->symbol_type() == NOTYPE_SYMBOL?'N':(
                        iter->second->symbol_type() == FILE_SYMBOL?'I':(
                            iter->second->symbol_type() ==
                            SECTION_SYMBOL?'S':'X')))));
    fprintf(out, " visible=%c",
            iter->second->symbol_visibility() == GLOBAL?'G':(
                iter->second->symbol_visibility() == LOCAL?'L':(
                    iter->second->symbol_visibility() == WEAK?'W':'X')));
    fprintf(out, " size=%d", iter->second->size() );
    if (iter->second->common())
      fprintf(out, " common=(%d,%d)", iter->second->common_size(),
              iter->second->common_align());
    if (iter->second->section_name())
      fprintf(out, " %s", iter->second->section_name());
    else
      fprintf(out, " *UND*");
    fprintf(out, "\n");
  }
}


Symbol *SymbolTable::Find(const char *name) {
  std::map<const char*, Symbol *, ltstr>::const_iterator it = table_.find(name);
  MAO_ASSERT(it != table_.end() );
  return it->second;
}

Symbol *SymbolTable::FindOrCreateAndFind(const char *name) {
  if (!Exists(name)) {
    return Add(name, new Symbol(name));
  } else {
    return Find(name);
  }
  MAO_RASSERT(1);
}

//
// Class: Symbol
//

Symbol::Symbol(const char *name, const SymbolVisibility symbol_visibility,
               const SymbolType symbol_type) {
  MAO_ASSERT(strlen(name) < kMaxSymbolLength);
  name_ = strdup(name);
  symbol_visibility_ = symbol_visibility;
  symbol_type_ = symbol_type;
  size_ = 0;
  common_ = false;
  common_size_ = 0;
  common_align_ = 0;
  section_name_ = 0;
}

Symbol::~Symbol() {
  MAO_ASSERT(name_);
  free(name_);
}

SymbolVisibility Symbol::symbol_visibility() const {
  return symbol_visibility_;
}

void Symbol::set_symbol_visibility(const SymbolVisibility symbol_visibility) {
    symbol_visibility_ = symbol_visibility;
}

const char *Symbol::name() const {
  MAO_ASSERT(name_);
  return name_;
}

void Symbol::set_size(const unsigned int size) {
  size_ = size;
}

unsigned int Symbol::size() const {
  return size_;
}

void Symbol::set_common(const bool common) {
  common_ = common;
}

bool Symbol::common() const {
  return common_;
}


unsigned int Symbol::common_size() const {
  return common_size_;
}

void Symbol::set_common_size(const unsigned int common_size) {
  common_size_ = common_size;
}


unsigned int Symbol::common_align() const {
  return common_align_;
}


void Symbol::set_common_align(const unsigned int common_align) {
  common_align_ = common_align;
}

SymbolType Symbol::symbol_type() const {
  return symbol_type_;
}

void Symbol::set_symbol_type(const SymbolType symbol_type) {
  symbol_type_ = symbol_type;
}

const char *Symbol::section_name() const {
  return section_name_;
}

void Symbol::set_section_name(const char *section_name) {
  MAO_ASSERT(section_name);
  section_name_ = section_name;
}
