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
#include <vector>

#include "Operation.h"
#include "Operand.h"

// Assembly class represents an abstracted version of an assembly file which is
// to be written to the file system by the test generator.  It appears to be
// most logical to create an assembly object with a constructor with no
// arguments and add the information as it is determined in the test generator.
class Assembly {
 public:
    inline Assembly(const std::string instruction_name,
                    const std::string addressing_mode,
                    const std::string instruction_body,
                    const std::string file_name,
                    int number_operands, Operation* operation,
                    std::vector<Operand*> operands) :
                      instruction_name_(instruction_name),
                      addressing_mode_(addressing_mode),
                      instruction_body_(instruction_body),
                      file_name_(file_name) {
      this->number_operands_ = number_operands;
      this->operation_ = operation;
      this->operands_ = operands;
    }

    ~Assembly() {
      // TODO(caseyburkhardt): Implement Destructor
    }

    inline const std::string& instruction_body() {
      return this->instruction_body_;
    }

    inline const std::string& instruction_name() {
      return this->instruction_name_;
    }

    inline const std::string& addressing_mode() {
      return this->addressing_mode_;
    }

    inline const std::string& file_name() {
      return this->file_name_;
    }

    inline int number_operands() {
      return this->operands_.size();
    }

    inline Operation* operation() {
      return this->operation_;
    }

    inline std::vector<Operand*> operands() {
      return this->operands_;
    }

 private:
    const std::string instruction_name_;
    const std::string addressing_mode_;
    const std::string instruction_body_;
    const std::string file_name_;
    int number_operands_;
    Operation* operation_;
    std::vector<Operand*> operands_;
};

#endif /* IAT_ASSEMBLY_H_ */
