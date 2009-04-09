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
#include <vector>

#include "irlink.h"

class SymbolIterator;

struct cltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

typedef int SymbolID;

// TODO(martint):
// Look at "Generating binary equivalent ..." document and
// update the symbol table to match the properties listed

// TODO(martint):
// Create iterators for the symboltable

class Symbol {
  // allocates memory INSIDE the constructor!
 public:
  Symbol(const char *name, SymbolID id, const Section *section,
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
  const Section *section() const;
  void set_section(const Section *section);
  bool IsFunction() const {return symbol_type_ == FUNCTION_SYMBOL;}

  static const unsigned int kMaxSymbolLength = MAX_SYMBOL_NAME_LENGTH;

  SymbolID id() const { return id_; }

  // Associate another symbol to be "equal" to this one.
  void AddEqual(const Symbol *);

  typedef std::vector<const Symbol *>::const_iterator        EqualIterator;
  EqualIterator EqualBegin() { return equals_.begin();}
  EqualIterator EqualEnd()   { return equals_.end();}

//   std::vector<Symbol *> *GetEquals() { return &equals_; }

 private:
  // Dynamically allocated, thus not a constant.
  char *name_;
  SymbolID  id_;
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
  // Section associated with symbol.
  const Section *section_;

  // Other labels that are defined to be .set/.equ to this symbol:
  std::vector<const Symbol *> equals_;

  // Value of symbols are currently not stored in the MAO symbol table
};


// Symbol table
class SymbolTable {
 public:
  SymbolTable();
  ~SymbolTable();
  Symbol *Add(Symbol *symbol);
  bool Exists(const char *name);
  // Returns a pointer to a symbol with the given name. Creates it if it does
  // not already exists.
  Symbol *FindOrCreateAndFind(const char *name, const Section *section);
  // Returns a pointer a symbol with the given name. Assumes such a symbol
  // exists.
  Symbol *Find(const char *name);
  // Prints out the symbol table.
  void Print() const;
  void Print(FILE *out) const;

  int Size() const {return table_.size();}
  typedef std::map<const char *, Symbol *, cltstr> SymbolMap;
  typedef SymbolMap::const_iterator                ConstSymbolIterator;

  SymbolIterator Begin();
  SymbolIterator End();
  ConstSymbolIterator ConstBegin() const;
  ConstSymbolIterator ConstEnd() const;

 private:
  // Used for the map of symbols
  SymbolMap table_;
};


// Iterator wrapper for iterating over all the Symbols in a MaoUnit.
class SymbolIterator {
 public:
  explicit SymbolIterator(SymbolTable::SymbolMap::iterator symbol_iter)
      : symbol_iter_(symbol_iter) { }
  Symbol *&operator *() const;
  SymbolIterator &operator ++();
  SymbolIterator &operator --();
  bool operator ==(const SymbolIterator &other) const;
  bool operator !=(const SymbolIterator &other) const;
 private:
  SymbolTable::SymbolMap::iterator symbol_iter_;
};



#endif  // SYMBOLTABLE_H_
