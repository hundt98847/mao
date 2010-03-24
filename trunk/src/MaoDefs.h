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

#include <set>

#include "MaoUtil.h"

// This should be included, but since its not guraded, it will generate errors
// #include "opcodes/i386-opc.h"

// Bitmasks for operands
#define  REG_OP0      (1 << 0)
#define  REG_OP1      (1 << 1)
#define  REG_OP2      (1 << 2)
#define  REG_OP3      (1 << 3)
#define  REG_OP4      (1 << 4)
#define  REG_OP5      (1 << 5)

#define  REG_OP_BASE  (1 << 6)
#define  REG_OP_INDEX  (1 << 7)

#define  DEF_OP_ALL (REG_OP0 | REG_OP1 | REG_OP2 | REG_OP3 | REG_OP4 | REG_OP5)

#define  USE_OP_ALL (REG_OP0 | REG_OP1 | REG_OP2 | REG_OP3 | REG_OP4 | REG_OP5 | REG_OP_BASE | REG_OP_INDEX)

// more are possible...
struct DefEntry {
  int           opcode;      // matches table gen-opcodes.h
  unsigned int  op_mask;     // if insn defs operand(s)
  BitString     reg_mask;    // if insn defs register(s)
  BitString     reg_mask8;   //   for  8-bit addressing modes
  BitString     reg_mask16;  //   for 16-bit addressing modes
  BitString     reg_mask32;  //   for 32-bit addressing modes
  BitString     reg_mask64;  //   for 64-bit addressing modes
};

// Should possibly using the same struct for both.
// Duplicating just in case if there is a difference in usage
struct UseEntry {
  int           opcode;      // matches table gen-opcodes.h
  unsigned int  op_mask;     // if insn defs operand(s)
  BitString     reg_mask;    // if insn defs register(s)
  BitString     reg_mask8;   //   for  8-bit addressing modes
  BitString     reg_mask16;  //   for 16-bit addressing modes
  BitString     reg_mask32;  //   for 32-bit addressing modes
  BitString     reg_mask64;  //   for 64-bit addressing modes
};

extern DefEntry def_entries[];
extern UseEntry use_entries[];

// external entry points
class InstructionEntry;

void InitRegisters();
BitString  GetRegisterDefMask(const InstructionEntry *insn,
                              bool expand_mask = false);

BitString  GetRegisterUseMask(const InstructionEntry *insn,
                              bool expand_mask = false);

std::set<const reg_entry *> GetDefinedRegisters(InstructionEntry *insn);
std::set<const reg_entry *> GetUsedRegisters(InstructionEntry *insn);

BitString  GetCallingConventionDefMask();

void       PrintRegistersInRegisterMask(FILE *f, BitString mask,
                                        const char *title = NULL);
BitString  GetMaskForRegister(const reg_entry *reg);
bool       DefinesSubReg(reg_entry *reg,
                         reg_entry *sub_reg);
bool       RegistersOverlap(const reg_entry *reg1,
                            const reg_entry *reg2);
bool       RegistersContained(BitString &parent,
                              BitString &child);
bool       IsParent(const reg_entry *parent,
                    const reg_entry *child);
bool       IsParentNum(int parent,
                       int child);
BitString  GetParentRegs(const reg_entry *reg);

bool       IsRegDefined(InstructionEntry *insn, const reg_entry *reg);

void FillSubRegs(BitString *mask);
void FillParentRegs(BitString *mask);

// Provide pointer to
//   %rip  for 64-bit compiles
//   %eip  for 32-bit compiles
//
const reg_entry *GetIP();

const reg_entry *GetRegFromName(const char *reg_name);
const reg_entry *GetRegFromDwarfNumber(int dw_number, bool is_64_bit);
int GetRegNum(const char *reg_name);

const char *GetRegName(int reg_number);

int GetNumberOfRegisters();

#endif  // __MAODEFS_H_INCLUDED
