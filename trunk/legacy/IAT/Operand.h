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
// Description: File serves as the primary declaration source for the Operand
//              class, which represents the abstracted version of an assembly
//              operand, such as '%eax', for the TestGenerator of the
//              Instruction Analysis Tool.
//


#ifndef IAT_OPERAND_H_
#define IAT_OPERAND_H_

#include <string>

// Operand class represents an abstracted version of an assembly operand,
// such as '%eax'.  The operations will be read in from a file and objects will
// be instantiated and members set from within the Test Generator.
class Operand {
 public:
    inline Operand(const std::string operand_value,
                   const std::string operand_type) :
                    operand_value_(operand_value), operand_type_(operand_type) {
    }

    inline ~Operand() {
      // TODO(caseyburkhardt): Implement Destructor
    }

    inline const std::string& operand_value() {
      return this->operand_value_;
    }

    inline const std::string& operand_type() {
      return this->operand_type_;
    }

 private:
    const std::string operand_value_;
    const std::string operand_type_;
};

#endif /* IAT_ASSEMBLY_H_ */
