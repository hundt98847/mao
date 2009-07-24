//
// Copyright 2009 and later Google Inc.
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
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Author: Casey Burkhardt (caseyburkhardt)
// Description: File serves as the primary implementation for the Operand
//              class, which represents the abstracted version of an assembly
//              operand, such as '%eax', for the TestGenerator of the
//              Instruction Analysis Tool.
//

#include <string>

#include "./Operand.h"

// Constructor
Operand::Operand() {
  this->operand_value_.clear();
  this->operand_type_.clear();
}

// Destructor
Operand::~Operand() {
  // TODO(caseyburkhardt): Implement Destructor
}

void Operand::set_operand_value(string data) {
  this->operand_value_ = data;
}

string Operand::operand_value() {
  return this->operand_value_;
}

void Operand::set_operand_type(string data) {
  this->operand_type_ = data;
}

string Operand::operand_type() {
  return this->operand_type_;
}

string Operand::OutputString() {
  string result = "Operand Value: " + this->operand_value_ + "\n";
  result = result + "Operand Type: " + this->operand_type_ + "\n";
  return result;
}
