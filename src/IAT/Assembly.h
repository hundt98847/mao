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
// Description: File serves as the primary declaration source for the Assembly
//              class, which represents the abstracted version of an assembly
//              file to be written to the file system by the test generator.
//


#ifndef IAT_ASSEMBLY_H_
#define IAT_ASSEMBLY_H_

#include <string>
#include "Operation.h"
#include "Operand.h"

using namespace std;

// Assembly class represents an abstracted version of an assembly file which is
// to be written to the file system by the test generator.  It appears to be
// most logical to create an assembly object with a constructor with no
// arguments and add the information as it is determined in the test generator.
class Assembly {
 public:
  // Constructor
  Assembly();

  // Destructor
  virtual ~Assembly();

  // Accessors and Mutators
  void append_instructions(string data);
  string instruction_body();
  void set_instruction_name(string data);
  string instruction_name();
  void set_addressing_mode(string data);
  string addressing_mode();
  void set_file_name(string data);
  string file_name();
  int number_operands();
  void set_operation(Operation& operation);
  Operation* operation();
  void set_operands(Operand operands[], int number_operands);
  Operand* operand(int index);
  void set_generation_complete(bool status);
  bool generation_complete();
  void set_output_complete(bool status);
  bool output_complete();

  // Debug
  string OutputString();

 private:
  string instruction_body_;
  string instruction_name_;
  string addressing_mode_;
  string file_name_;

  int number_operands_;

  Operation* operation_;
  Operand* operand1_;
  Operand* operand2_;
  Operand* operand3_;

  // True when instructionBody contains full assembly file.
  bool generation_complete_;

  // True when file has been written to tests directory.
  bool output_complete_;
};

#endif /* IAT_ASSEMBLY_H_ */
