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
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#ifndef __MAODEFS_H_INCLUDED
#define __MAODEFS_H_INCLUDED

#include <stdio.h>
#include "MaoUtil.h"

// Bitmasks for operands
#define  DEF_OP0      (1 << 0)
#define  DEF_OP1      (1 << 1)
#define  DEF_OP2      (1 << 2)
#define  DEF_OP3      (1 << 3)
#define  DEF_OP4      (1 << 4)
#define  DEF_OP5      (1 << 5)

#define  DEF_OP_ALL (DEF_OP0 | DEF_OP1 | DEF_OP2 | DEF_OP3 | DEF_OP4 | DEF_OP5)

// more are possible...
typedef struct DefEntry {
  int           opcode;      // matches table gen-opcodes.h
  unsigned int  op_mask;     // if insn defs operand(s)
  BitString     reg_mask;    // if insn defs register(s)
  BitString     reg_mask8;   //   for  8-bit addressing modes
  BitString     reg_mask16;  //   for 16-bit addressing modes
  BitString     reg_mask32;  //   for 32-bit addressing modes
  BitString     reg_mask64;  //   for 64-bit addressing modes
};
extern DefEntry def_entries[];

// external entry points
class InstructionEntry;

void InitRegisters();
BitString  GetRegisterDefMask(InstructionEntry *insn);
void       PrintRegisterDefMask(FILE *f, BitString mask,
                                const char *title = NULL);
BitString  GetMaskForRegister(const char *reg);
bool       DefinesSubReg(struct reg_entry *reg,
                         struct reg_entry *sub_reg);
bool       RegistersOverlap(const struct reg_entry *reg1,
                            const struct reg_entry *reg2);
bool       RegistersContained(BitString &parent,
                              BitString &child);
BitString  GetParentRegs(const struct reg_entry *reg);

// Provide pointer to
//   %rip  for 64-bit compiles
//   %eip  for 32-bit compiles
//
const reg_entry *GetIP();

#endif // __MAODEFS_H_INCLUDED
