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

#ifndef SYMBOLTABLE_H_
#define SYMBOLTABLE_H_

#include <map>

#include "irlink.h"

class Symbol {
  // allocates memory INSIDE the constructor!
 public:
  Symbol(const char *name,
         SymbolVisibility symbol_visibility = LOCAL,
         const SymbolType symbol_type = OBJECT_SYMBOL);
  ~Symbol();
  // The list of type can be found in irlink.h
  enum SymbolType symbol_type() const;
  void set_symbol_type(const SymbolType symbol_type);
  unsigned int size() const;
  void set_size(const unsigned int size);
  const char *name() const;
  SymbolVisibility symbol_visibility() const;
  void set_symbol_visibility(const SymbolVisibility symbol_visibility);
  bool common() const;
  void set_common(const bool common);
  unsigned int common_size() const;
  void set_common_size(const unsigned int common_size);
  unsigned int common_align() const;
  void set_common_align(const unsigned int common_align);
  const char *section_name() const;
  void set_section_name(const char *section_name);

  static const unsigned int kMaxSymbolLength = MAX_SYMBOL_NAME_LENGTH;

 private:
  // Dynamically allocated, thus not a constant.
  char *name_;
  // Type of symbol. See irlink.h for list of types
  SymbolType symbol_type_;
  // Size of symbol
  unsigned int size_;
  // Visibility of symbol. See irlink.h for list
  SymbolVisibility symbol_visibility_;
  // Common symbol?
  bool common_;
  // Only valid if common_ is true
  unsigned int common_size_;
  unsigned int common_align_;
  // Name of section associated with symbol.
  // TODO(martint): use pointer to section instead
  const char *section_name_;
};

// Symbol table
class SymbolTable {
 public:
  SymbolTable();
  ~SymbolTable();
  Symbol *Add(const char *name, Symbol *symbol);
  bool Exists(const char *name);
  // Returns a pointer to a symbol with the given name. Creates it if it does
  // not already exists.
  Symbol *FindOrCreateAndFind(const char *name);
  // Returns a pointer a symbol with the given name. Assumes such a symbol
  // exists.
  Symbol *Find(const char *name);
  // Prints out the symbol table.
  void Print() const;
  void Print(FILE *out) const;
 private:
  // Used for the map of symbols
  struct ltstr {
    bool operator()(const char* s1, const char* s2) const {
      return strcmp(s1, s2) < 0;
    }
  };
  std::map<const char *, Symbol *, ltstr> table_;
};


#endif  // SYMBOLTABLE_H_
