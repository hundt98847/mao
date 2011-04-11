//
// Copyright 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
// 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
// Free Software Foundation, Inc.
//
// This file was derived from part of GAS, the GNU Assembler.
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
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

#include <limits.h>
#include <stdlib.h>
#include "MaoDebug.h"
#include "libiberty.h"
extern "C" {
  #include "as.h"
}
#include "tc-i386.h"
#include "tc-i386-helper.h"

void X86InstructionSizeHelper::MergeSizePair(const std::pair<int, bool> &from,
                                             std::pair<int, bool> *to) {
  to->first += from.first;
  to->second |= from.second;
}

std::pair<int, bool> X86InstructionSizeHelper::SizeOfBranch() {
  int size = 1;                 // At least 1 opcode byte

  if (insn_->prefix[DATA_PREFIX] != 0)
    size++;

  /* Pentium4 branch hints.  */
  if (insn_->prefix[SEG_PREFIX] == CS_PREFIX_OPCODE    /* not taken */
      || insn_->prefix[SEG_PREFIX] == DS_PREFIX_OPCODE /* taken */ ) {
    size++;
  }

  if (insn_->prefix[REX_PREFIX] != 0)
    size++;

  return std::make_pair(size, true);
}

std::pair<int, bool> X86InstructionSizeHelper::SizeOfJump(enum flag_code flag) {
  int size = 1;  // 1 byte for the opcode

  if (insn_->tm.opcode_modifier.jumpbyte) {
    /* This is a loop or jecxz type instruction.  */
    size++;  // 1 byte for the offset for this type
    if (insn_->prefix[ADDR_PREFIX] != 0)
      size++;

    /* Pentium4 branch hints.  */
    if (insn_->prefix[SEG_PREFIX] == CS_PREFIX_OPCODE /* not taken */
        || insn_->prefix[SEG_PREFIX] == DS_PREFIX_OPCODE /* taken */)
      size++;
  } else {
    int width16 = flag == CODE_16BIT ? true : false;
    if (insn_->prefix[DATA_PREFIX] != 0) {
      size++;
      width16 = !width16;
    }

    if (width16)
      size += 2;  // 2 byte offset in 16-bit mode
    else
      size += 4;  // 4 byte offset in 32-bit nmode
  }

  if (insn_->prefix[REX_PREFIX] != 0)
    size++;

  return std::make_pair(size, false);
}

std::pair<int, bool> X86InstructionSizeHelper::SizeOfIntersegJump(
    enum flag_code flag) {
  int size = 0;
  bool width16 = flag == CODE_16BIT ? true : false;

  if (insn_->prefix[DATA_PREFIX] != 0) {
    size++;
    width16 = !width16;
  }

  if (insn_->prefix[REX_PREFIX] != 0)
    size++;

  if (width16)
    size += 2;  // 2 byte offset in 16-bit mode
  else
    size += 4;  // 4 byte offset in 32-bit mode

  /* 1 opcode; 2 segment; offset  */
  size += 1 + 2;

  return std::make_pair(size, false);
}

std::pair<int, bool> X86InstructionSizeHelper::SizeOfDisp() {
  unsigned int n;
  int size = 0;

  for (n = 0; n < insn_->operands; n++) {
    if (operand_type_check(insn_->types[n], disp)) {
      if (insn_->types[n].bitfield.disp64)
        size += 8;
      else if (insn_->types[n].bitfield.disp8)
        size += 1;
      else if (insn_->types[n].bitfield.disp16)
        size += 2;
      else
        size = 4;
    }
  }
  return std::make_pair(size, false);
}

std::pair<int, bool> X86InstructionSizeHelper::SizeOfImm() {
  unsigned int n;
  int size = 0;

  for (n = 0; n < insn_->operands; n++) {
    if (operand_type_check(insn_->types[n], imm)) {
      if (insn_->types[n].bitfield.imm64)
        size += 8;
      else if (insn_->types[n].bitfield.imm8 || insn_->types[n].bitfield.imm8s)
        size += 1;
      else if (insn_->types[n].bitfield.imm16)
        size += 2;
      else
        size += 4;
    }
  }
  return std::make_pair(size, false);
}

std::pair<int, bool> X86InstructionSizeHelper::SizeOfInstruction(
    enum flag_code flag) {
  std::pair<int, bool> size(0, false);


  // Size jumps.
  if (insn_->tm.opcode_modifier.jump)
    size = SizeOfBranch();
  else if (insn_->tm.opcode_modifier.jumpbyte ||
           insn_->tm.opcode_modifier.jumpdword)
    size = SizeOfJump(flag);
  else if (insn_->tm.opcode_modifier.jumpintersegment) {
    size = SizeOfIntersegJump(flag);
  } else {
    /* Output normal instructions here.  */
    unsigned char *q;
    unsigned int j;

    /* Since the VEX prefix contains the implicit prefix, we don't
       need the explicit prefix.  */
    if (!insn_->tm.opcode_modifier.vex) {

      /* The prefix bytes.  */
      for (j = ARRAY_SIZE (insn_->prefix), q = insn_->prefix; j > 0; j--, q++)
        if (*q)
          size.first++;
    }

    if (insn_->tm.opcode_modifier.vex) {
      for (j = 0, q = insn_->prefix; j < ARRAY_SIZE (insn_->prefix); j++, q++)
        if (*q)
          switch (j) {
            case REX_PREFIX:
              /* REX byte is encoded in VEX prefix.  */
              break;
            case SEG_PREFIX:
            case ADDR_PREFIX:
              size.first++;
              break;
            default:
              /* There should be no other prefixes for instructions
                 with VEX prefix.  */
              MAO_ASSERT(false);
          }

      /* Now the VEX prefix.  */
      size.first += insn_->vex.length;
    }

    /* Now the opcode; be careful about word order here!  */
    size.first += insn_->tm.opcode_length;

    /* Now the modrm byte and sib byte (if present).  */
    if (insn_->tm.opcode_modifier.modrm) {
      size.first++;

      /* If insn_->rm.regmem == ESP (4)
         && insn_->rm.mode != (Register mode)
         && not 16 bit
         ==> need second modrm byte.  */
      if (insn_->rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING
          && insn_->rm.mode != 3
          && !(insn_->base_reg && insn_->base_reg->reg_type.bitfield.reg16))
        size.first++;
    }

    if (insn_->disp_operands)
      MergeSizePair(SizeOfDisp(), &size);

    if (insn_->imm_operands)
      MergeSizePair(SizeOfImm(), &size);
  }

  return size;
}
