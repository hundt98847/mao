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
// Description: File serves as the primary declaration source for the Operation
//              class, which represents the abstracted version of an assembly
//              operation, such as 'add', for the TestGenerator of the
//              Instruction Analysis Tool.
//


#ifndef IAT_OPERATION_H_
#define IAT_OPERATION_H_

#include <string>

// Operation class represents an abstracted version of an assembly operation,
// such as 'add'.  The operations will be read in from a file and objects will
// be instantiated and members set from within the Test Generator.
class Operation {
 public:
    inline Operation(const std::string& operation_name, int min, int max) :
      operation_name_(operation_name) {
      this->min_operands_ = min;
      this->max_operands_ = max;
    }

  // Destructor
    inline ~Operation() {
      // TODO(caseyburkhardt): Implement Destructor
    }

  // Accessors and Mutators
    inline const std::string& operation_name() {
      return this->operation_name_;
    }

    inline int min_operands() {
      return this->min_operands_;
    }

    inline int max_operands() {
      return this->max_operands_;
    }

 private:
    const std::string operation_name_;
    int min_operands_;
    int max_operands_;
};

#endif /* IAT_ASSEMBLY_H_ */
