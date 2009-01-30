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
#include "ir-gas.h"
#include "tc-i386-helper.h"

int X86InstructionSizeHelper::SizeOfBranch() {
  int size = 1;			// At least 1 opcode byte

  if (insn_->prefix[DATA_PREFIX] != 0)
    size++;

  /* Pentium4 branch hints.  */
  if (insn_->prefix[SEG_PREFIX] == CS_PREFIX_OPCODE	/* not taken */
      || insn_->prefix[SEG_PREFIX] == DS_PREFIX_OPCODE /* taken */ ) {
    size++;
  }

  if (insn_->prefix[REX_PREFIX] != 0)
    size++;

  return size;
}

int X86InstructionSizeHelper::SizeOfJump() {
  // TODO(nvachhar):
  MAO_ASSERT(false);
  return 0;
}

int X86InstructionSizeHelper::SizeOfIntersegJump() {
  // TODO(nvachhar):
  MAO_ASSERT(false);
  return 0;
}

int X86InstructionSizeHelper::SizeOfDisp() {
  unsigned int n;
  int size = 0;

  for (n = 0; n < insn_->operands; n++) {
    if (operand_type_check (insn_->types[n], disp)) {
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
  return size;
}

int X86InstructionSizeHelper::SizeOfImm() {
  unsigned int n;
  int size = 0;

  for (n = 0; n < insn_->operands; n++) {
    if (operand_type_check (insn_->types[n], imm)) {
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
  return size;
}


/* Returns 0 if attempting to add a prefix where one from the same
   class already exists, 1 if non rep/repne added, 2 if rep/repne
   added.  */
int X86InstructionSizeHelper::AddPrefix(unsigned int prefix) {
  int ret = 1;
  unsigned int q;

  if (prefix >= REX_OPCODE && prefix < REX_OPCODE + 16
      && flag_code == CODE_64BIT) {
    if ((insn_->prefix[REX_PREFIX] & prefix & REX_W)
        || ((insn_->prefix[REX_PREFIX] & (REX_R | REX_X | REX_B))
            && (prefix & (REX_R | REX_X | REX_B))))
      ret = 0;
    q = REX_PREFIX;
  } else {
    switch (prefix) {
      default:
        MAO_ASSERT(false);

      case CS_PREFIX_OPCODE:
      case DS_PREFIX_OPCODE:
      case ES_PREFIX_OPCODE:
      case FS_PREFIX_OPCODE:
      case GS_PREFIX_OPCODE:
      case SS_PREFIX_OPCODE:
        q = SEG_PREFIX;
        break;

      case REPNE_PREFIX_OPCODE:
      case REPE_PREFIX_OPCODE:
        ret = 2;
        /* fall thru */
      case LOCK_PREFIX_OPCODE:
        q = LOCKREP_PREFIX;
        break;

      case FWAIT_OPCODE:
        q = WAIT_PREFIX;
        break;

      case ADDR_PREFIX_OPCODE:
        q = ADDR_PREFIX;
        break;

      case DATA_PREFIX_OPCODE:
        q = DATA_PREFIX;
        break;
    }
    if (insn_->prefix[q] != 0)
      ret = 0;
  }

  if (ret) {
    if (!insn_->prefix[q])
      ++insn_->prefixes;
    insn_->prefix[q] |= prefix;
  } else {
    MAO_ASSERT_MSG(false, "same type of prefix used twice");
  }

  return ret;
}

int X86InstructionSizeHelper::SizeOfInstruction() {
  int size = 0;

  // Size jumps.
  if (insn_->tm.opcode_modifier.jump)
    size = SizeOfBranch();
  else if (insn_->tm.opcode_modifier.jumpbyte || insn_->tm.opcode_modifier.jumpdword)
    size = SizeOfJump();
  else if (insn_->tm.opcode_modifier.jumpintersegment)
    size = SizeOfIntersegJump();
  else {
    /* Output normal instructions here.  */
    unsigned char *q;
    unsigned int j;
    unsigned int prefix;

    /* Since the VEX prefix contains the implicit prefix, we don't
       need the explicit prefix.  */
    if (!insn_->tm.opcode_modifier.vex) {
      switch (insn_->tm.opcode_length) {
        case 3:
          if (insn_->tm.base_opcode & 0xff000000) {
            prefix = (insn_->tm.base_opcode >> 24) & 0xff;
            goto check_prefix;
          }
          break;
        case 2:
          if ((insn_->tm.base_opcode & 0xff0000) != 0) {
            prefix = (insn_->tm.base_opcode >> 16) & 0xff;
            if (insn_->tm.cpu_flags.bitfield.cpupadlock) {
           check_prefix:
              if (prefix != REPE_PREFIX_OPCODE || (insn_->prefix[LOCKREP_PREFIX]
                                                   != REPE_PREFIX_OPCODE))
                AddPrefix(prefix);
            } else
              AddPrefix(prefix);
          }
          break;
        case 1:
          break;
        default:
          MAO_ASSERT(false);
      }

      /* The prefix bytes.  */
      for (j = ARRAY_SIZE (insn_->prefix), q = insn_->prefix; j > 0; j--, q++)
        if (*q)
          size++;
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
              size++;
              break;
            default:
              /* There should be no other prefixes for instructions
                 with VEX prefix.  */
              MAO_ASSERT(false);
          }

      /* Now the VEX prefix.  */
      size += insn_->vex.length;
    }

    /* Now the opcode; be careful about word order here!  */
    size += insn_->tm.opcode_length;

    /* Now the modrm byte and sib byte (if present).  */
    if (insn_->tm.opcode_modifier.modrm) {
      size++;

      /* If insn_->rm.regmem == ESP (4)
         && insn_->rm.mode != (Register mode)
         && not 16 bit
         ==> need second modrm byte.  */
      if (insn_->rm.regmem == ESCAPE_TO_TWO_BYTE_ADDRESSING
          && insn_->rm.mode != 3
          && !(insn_->base_reg && insn_->base_reg->reg_type.bitfield.reg16))
        size++;
    }

    /* Write the DREX byte if needed.  */
    if (insn_->tm.opcode_modifier.drex || insn_->tm.opcode_modifier.drexc)
      size++;

    if (insn_->disp_operands)
      size += SizeOfDisp();

    if (insn_->imm_operands)
      size += SizeOfImm();
  }

  return size;
}
