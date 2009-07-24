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
// Description: File serves as the primary implementation for the Operation
//              class, which represents the abstracted version of an assembly
//              operation, such as 'add', for the TestGenerator of the
//              Instruction Analysis Tool.
//

#include <string>

#include "./Operation.h"

// Constructor
Operation::Operation() {
  this->operation_name_.clear();
  this->min_operands_ = 0;
  this->max_operands_ = 3;
}

// Destructor
Operation::~Operation() {
  // TODO(caseyburkhardt): Implement Destructor
}

void Operation::set_operation_name(string data) {
  this->operation_name_ = data;
}

string Operation::operation_name() {
  return this->operation_name_;
}

void Operation::set_min_operands(int value) {
  this->min_operands_ = value;
}

int Operation::min_operands() {
  return this->min_operands_;
}

void Operation::set_max_operands(int value) {
  this->max_operands_ = value;
}

int Operation::max_operands() {
  return this->max_operands_;
}

string Operation::OutputString() {
  char charValue[1];

  string result = "Operation Name: " + this->operation_name_ + "\n";
  sprintf(charValue, "%d", this->min_operands_);
  result = result + "Minimum Operands: " + charValue + "\n";
  sprintf(charValue, "%d", this->max_operands_);
  result = result + "Maximum Operands: " + charValue + "\n";
  return result;
}
