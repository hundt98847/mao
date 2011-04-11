/* tc-i386.c -- Assemble code for the Intel 80386
   Copyright 1989, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* Intel 80386 machine specific gas.
   Written by Eliot Dresselhaus (eliot@mgm.mit.edu).
   x86_64 support by Jan Hubicka (jh@suse.cz)
   VIA PadLock support by Michal Ludvig (mludvig@suse.cz)
   Bugs & suggestions are completely welcome.  This is free software.
   Please help us make it better.  */

#include "as.h"

#ifndef TC_I386_HELPER_H_
#define TC_I386_HELPER_H_

#ifndef INLINE
#if __GNUC__ >= 2
#define INLINE __inline__
#else
#define INLINE
#endif
#endif

enum operand_type {
  reg,
  imm,
  disp,
  anymem
};

#ifdef __cplusplus
extern "C" {
#endif

  INLINE int
  operand_type_check (i386_operand_type t, enum operand_type c) {
    switch (c) {
      case reg:
        return (t.bitfield.reg8
                || t.bitfield.reg16
                || t.bitfield.reg32
                || t.bitfield.reg64);

      case imm:
        return (t.bitfield.imm8
                || t.bitfield.imm8s
                || t.bitfield.imm16
                || t.bitfield.imm32
                || t.bitfield.imm32s
                || t.bitfield.imm64);

      case disp:
        return (t.bitfield.disp8
                || t.bitfield.disp16
                || t.bitfield.disp32
                || t.bitfield.disp32s
                || t.bitfield.disp64);

      case anymem:
        return (t.bitfield.disp8
                || t.bitfield.disp16
                || t.bitfield.disp32
                || t.bitfield.disp32s
                || t.bitfield.disp64
                || t.bitfield.baseindex);

      default:
        abort ();
    }

    return 0;
  }

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

#include <utility>

#undef WAIT_PREFIX
#undef SEG_PREFIX
#undef ADDR_PREFIX
#undef DATA_PREFIX
#undef REP_PREFIX
#undef LOCK_PREFIX
#undef REX_PREFIX
#undef MAX_PREFIXES

class X86InstructionSizeHelper {
 public:
  enum Prefix {
    WAIT_PREFIX    = 0,
    SEG_PREFIX	   = 1,
    ADDR_PREFIX	   = 2,
    DATA_PREFIX	   = 3,
    REP_PREFIX     = 4,
    LOCK_PREFIX    = 5,
    REX_PREFIX     = 6,	/* must come last.  */
    MAX_PREFIXES   = 7	/* max prefixes per opcode */
  };

  X86InstructionSizeHelper(i386_insn *insn) : insn_(insn) { }

  // This function returns the fixed size of an instruction and a bool
  // which indicates whether or not the instruction can be variably
  // sized.  If the bool is true, the size does *NOT* include the size
  // of the variable length part of the instruction.
  std::pair<int, bool> SizeOfInstruction(enum flag_code flag);

 private:
  /* Prefixes will be emitted in the order defined below.
     WAIT_PREFIX must be the first prefix since FWAIT is really is an
     instruction, and so must come before any prefixes.
     The preferred prefix order is SEG_PREFIX, ADDR_PREFIX, DATA_PREFIX,
     LOCKREP_PREFIX.  */
  std::pair<int, bool> SizeOfBranch();
  std::pair<int, bool> SizeOfJump(enum flag_code flag);
  std::pair<int, bool> SizeOfIntersegJump(enum flag_code flag);
  std::pair<int, bool> SizeOfDisp();
  std::pair<int, bool> SizeOfImm();

  void MergeSizePair(const std::pair<int, bool> &from, std::pair<int, bool> *to);

  i386_insn *insn_;
};

#endif


#endif  /* TC_I386_HELPER_H_ */
