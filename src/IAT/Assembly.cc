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
// Description: File serves as the primary implementation for the Assembly
//              class, which represents the abstracted version of an assembly
//              file to be written to the file system by the test generator.
//

#include <string>

#include "./Assembly.h"

// Constructor
Assembly::Assembly() {
  this->instruction_body_.clear();
  this->addressing_mode_.clear();
  this->instruction_name_.clear();
  this->file_name_.clear();
  this->generation_complete_ = false;
  this->output_complete_ = false;
  this->number_operands_ = 0;
}

// Destructor
Assembly::~Assembly() {
  // TODO(caseyburkhardt): Implement Destructor
}

void Assembly::append_instructions(string data) {
  this->instruction_body_.append(data);
  this->instruction_body_.append("\n");
}

string Assembly::instruction_body() {
  return this->instruction_body_;
}

void Assembly::set_instruction_name(string data) {
  this->instruction_name_ = data;
}

string Assembly::instruction_name() {
  return this->instruction_name_;
}

void Assembly::set_addressing_mode(string data) {
  this->addressing_mode_ = data;
}

string Assembly::addressing_mode() {
  return this->addressing_mode_;
}

void Assembly::set_file_name(string data) {
  this->file_name_ = data;
}

string Assembly::file_name() {
  return this->file_name_;
}

int Assembly::number_operands() {
  return this->number_operands_;
}

void Assembly::set_operation(Operation& operation) {
  this->operation_ = &operation;
}

void Assembly::set_operands(Operand operands[], int number_operands) {
  if (number_operands == 0) {
    this->operand1_ = NULL;
    this->operand2_ = NULL;
    this->operand3_ = NULL;
    this->number_operands_ = 0;
  } else if (number_operands == 1) {
    this->operand1_ = &operands[0];
    this->operand2_ = NULL;
    this->operand3_ = NULL;
    this->number_operands_ = 1;
  } else if (number_operands == 2) {
    this->operand1_ = &operands[0];
    this->operand2_ = &operands[1];
    this->operand3_ = NULL;
    this->number_operands_ = 2;
  } else if (number_operands == 3) {
    this->operand1_ = &operands[0];
    this->operand2_ = &operands[1];
    this->operand3_ = &operands[2];
    this->number_operands_ = 3;
  } else {
    printf("Illegal Operation[Assembly::set_operands] - "
        "Invalid Operand Count.\n");
    exit(1);
  }
}

Operand* Assembly::operand(int index) {
  if (this->number_operands_ == 0) {
    printf("Illegal Operation[Assembly::operand] - "
        "Assembly Object has no operands.\n");
    exit(1);
  } else if (index == 0) {
    return this->operand1_;
  } else if (index == 1) {
    return this->operand2_;
  } else if (index == 2) {
    return this->operand3_;
  } else {
    printf("Illegal Operation[Assembly::operand] - "
        "Operand index out of bounds.\n");
    exit(1);
  }
}

void Assembly::set_generation_complete(bool status) {
  this->generation_complete_ = status;
}

bool Assembly::generation_complete() {
  return this->generation_complete_;
}

void Assembly::set_output_complete(bool status) {
  this->output_complete_ = status;
}

bool Assembly::output_complete() {
  return this->output_complete_;
}

string Assembly::OutputString() {
  string result = "Operation Name: " + this->instruction_name_ + "\n";
  result = result + "Addressing Mode: " + this->addressing_mode_ + "\n";
  result = result + "Instruction Body: \n" + this->instruction_body_;
  return result;
}
