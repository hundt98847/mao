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
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

#include "Mao.h"

SymbolTable::SymbolTable() {
  table_.clear();
}

SymbolTable::~SymbolTable() {
  // we should clear the entries here.
  // when added, they are allocated with new!
  for (std::map<const char*, Symbol *, ltstr>::iterator iter = table_.begin();
      iter != table_.end(); ++iter) {
    delete iter->second;
  }
}

bool SymbolTable::Exists(const char *name) {
  std::map<const char*, Symbol *, ltstr>::const_iterator it = table_.find(name);
  return it != table_.end();
}

Symbol *SymbolTable::Add(Symbol *symbol) {
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
    Symbol *symbol = iter->second;
    fprintf(out, "\t# ");
    fprintf(out, " [%3d] ", symbol->id());
    fprintf(out, " %-10s", symbol->name());
    fprintf(out, " type=%c",
            symbol->symbol_type() == OBJECT_SYMBOL?'O':(
                symbol->symbol_type() == FUNCTION_SYMBOL?'F':(
                    symbol->symbol_type() == NOTYPE_SYMBOL?'N':(
                        symbol->symbol_type() == FILE_SYMBOL?'I':(
                            symbol->symbol_type() ==
                            SECTION_SYMBOL?'S':'X')))));
    fprintf(out, " visible=%c",
            symbol->symbol_visibility() == GLOBAL?'G':(
                symbol->symbol_visibility() == LOCAL?'L':(
                    symbol->symbol_visibility() == WEAK?'W':'X')));
    fprintf(out, " size=%d", symbol->size() );
    if (symbol->common())
      fprintf(out, " common=(%d,%d)", symbol->common_size(),
              symbol->common_align());
    if (symbol->section())
      fprintf(out, " [%d: %s]", symbol->section()->id(),
              symbol->section()->name().c_str());
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

Symbol *SymbolTable::FindOrCreateAndFind(const char *name,
                                         const Section *section) {
  if (!Exists(name)) {
    // TODO(martint): use ID factory
    return Add(new Symbol(name, Size(), section));
  } else {
    return Find(name);
  }
  MAO_RASSERT(1);
}

SymbolIterator SymbolTable::Begin() {
  return SymbolIterator(table_.begin());
}

SymbolIterator SymbolTable::End() {
  return SymbolIterator(table_.end());
}

SymbolTable::ConstSymbolIterator SymbolTable::ConstBegin() const {
  return table_.begin();
}

SymbolTable::ConstSymbolIterator SymbolTable::ConstEnd() const {
  return table_.end();
}


//
// Class: SymbolIterator
//

Symbol *&SymbolIterator::operator *() const {
  return symbol_iter_->second;
}

SymbolIterator &SymbolIterator::operator ++() {
  ++symbol_iter_;
  return *this;
}

SymbolIterator &SymbolIterator::operator --() {
  --symbol_iter_;
  return *this;
}

bool SymbolIterator::operator ==(const SymbolIterator &other) const {
  return (symbol_iter_ == other.symbol_iter_);
}

bool SymbolIterator::operator !=(const SymbolIterator &other) const {
  return !((*this) == other);
}


//
// Class: Symbol
//
Symbol::Symbol(const char *name, SymbolID id, const Section *section,
               const SymbolVisibility symbol_visibility,
               const SymbolType symbol_type)
    : id_(id),
      symbol_type_(symbol_type),
      size_(0),
      symbol_visibility_(symbol_visibility),
      common_(false),
      common_size_(0),
      common_align_(0),
      section_(section) {
  MAO_ASSERT(strlen(name) < kMaxSymbolLength);
  name_ = strdup(name);
  equals_.clear();
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

const Section *Symbol::section() const {
  return section_;
}


void Symbol::AddEqual(const Symbol *symbol) {
  equals_.push_back(symbol);
}

void Symbol::set_section(const Section *section) {
  MAO_ASSERT(section);
  section_ = section;
}
