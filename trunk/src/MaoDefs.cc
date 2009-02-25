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

#include "MaoDefs.h"
#include "MaoUnit.h"
#include "gen-defs.h"

unsigned long long GetRegisterDefMask(InstructionEntry *insn) {
  DefEntry *e = &def_entries[insn->op()];
  MAO_ASSERT(e->opcode = insn->op());

  unsigned long long mask = e->reg_mask;
  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        const char *reg = insn->GetRegisterOperand(op);
        if (!strcmp(reg, "al")) mask |= REG_AL; else
        if (!strcmp(reg, "ah")) mask |= REG_AH; else
        if (!strcmp(reg, "ax")) mask |= REG_AX; else
        if (!strcmp(reg, "eax")) mask |= REG_EAX; else
        if (!strcmp(reg, "rax")) mask |= REG_RAX; else

        if (!strcmp(reg, "cl")) mask |= REG_CL; else
        if (!strcmp(reg, "ch")) mask |= REG_CH; else
        if (!strcmp(reg, "cx")) mask |= REG_CX; else
        if (!strcmp(reg, "ecx")) mask |= REG_ECX; else
        if (!strcmp(reg, "rcx")) mask |= REG_RCX; else

        if (!strcmp(reg, "dl")) mask |= REG_DL; else
        if (!strcmp(reg, "dh")) mask |= REG_DH; else
        if (!strcmp(reg, "dx")) mask |= REG_DX; else
        if (!strcmp(reg, "edx")) mask |= REG_EDX; else
        if (!strcmp(reg, "rdx")) mask |= REG_RDX; else

        if (!strcmp(reg, "bl")) mask |= REG_BL; else
        if (!strcmp(reg, "bh")) mask |= REG_BH; else
        if (!strcmp(reg, "bx")) mask |= REG_BX; else
        if (!strcmp(reg, "ebx")) mask |= REG_EBX; else
        if (!strcmp(reg, "rbx")) mask |= REG_RBX; else

        if (!strcmp(reg, "sp"))  mask |= REG_SP; else
        if (!strcmp(reg, "esp"))  mask |= REG_ESP; else
        if (!strcmp(reg, "rsp"))  mask |= REG_RSP; else

        if (!strcmp(reg, "bp"))  mask |= REG_BP; else
        if (!strcmp(reg, "ebp"))  mask |= REG_EBP; else
        if (!strcmp(reg, "rbp"))  mask |= REG_RBP; else

        if (!strcmp(reg, "si"))  mask |= REG_SI; else
        if (!strcmp(reg, "esi"))  mask |= REG_ESI; else
        if (!strcmp(reg, "rsi"))  mask |= REG_RSI; else

        if (!strcmp(reg, "di"))  mask |= REG_DI; else
        if (!strcmp(reg, "edi"))  mask |= REG_EDI; else
        if (!strcmp(reg, "rdi"))  mask |= REG_RDI; else

        if (!strcmp(reg, "r8"))  mask |= REG_R8; else
        if (!strcmp(reg, "r9"))  mask |= REG_R9; else
        if (!strcmp(reg, "r10"))  mask |= REG_R10; else
        if (!strcmp(reg, "r11"))  mask |= REG_R11; else
        if (!strcmp(reg, "r12"))  mask |= REG_R12; else
        if (!strcmp(reg, "r13"))  mask |= REG_R13; else
        if (!strcmp(reg, "r14"))  mask |= REG_R14; else
        if (!strcmp(reg, "r15"))  mask |= REG_R15;
      }
    }
  }
  return mask;
}


void PrintRegisterDefMask(unsigned long long mask, FILE *f) {
  if (mask & REG_AL) fprintf(f, "al ");
  if (mask & REG_AH) fprintf(f, "ah ");
  if (mask & REG_AX) fprintf(f, "ax ");
  if (mask & REG_EAX) fprintf(f, "eax ");
  if (mask & REG_RAX) fprintf(f, "rax ");

  if (mask & REG_CL) fprintf(f, "cl ");
  if (mask & REG_CH) fprintf(f, "ch ");
  if (mask & REG_CX) fprintf(f, "cx ");
  if (mask & REG_ECX) fprintf(f, "ecx ");
  if (mask & REG_RCX) fprintf(f, "rcx ");

  if (mask & REG_DL) fprintf(f, "dl ");
  if (mask & REG_DH) fprintf(f, "dh ");
  if (mask & REG_DX) fprintf(f, "dx ");
  if (mask & REG_EDX) fprintf(f, "edx ");
  if (mask & REG_RDX) fprintf(f, "rdx ");

  if (mask & REG_BL) fprintf(f, "bl ");
  if (mask & REG_BH) fprintf(f, "bh ");
  if (mask & REG_BX) fprintf(f, "bx ");
  if (mask & REG_EBX) fprintf(f, "ebx ");
  if (mask & REG_RBX) fprintf(f, "rbx ");

  if (mask & REG_SP) fprintf(f, "sp ");
  if (mask & REG_ESP) fprintf(f, "esp ");
  if (mask & REG_RSP) fprintf(f, "rsp ");

  if (mask & REG_BP) fprintf(f, "bp ");
  if (mask & REG_EBP) fprintf(f, "ebp ");
  if (mask & REG_RBP) fprintf(f, "rbp ");

  if (mask & REG_SI) fprintf(f, "si ");
  if (mask & REG_ESI) fprintf(f, "esi ");
  if (mask & REG_RSI) fprintf(f, "rsi ");

  if (mask & REG_DI) fprintf(f, "di ");
  if (mask & REG_EDI) fprintf(f, "edi ");
  if (mask & REG_RDI) fprintf(f, "rdi ");

  if (mask & REG_R8) fprintf(f, "r8 ");
  if (mask & REG_R9) fprintf(f, "r9 ");
  if (mask & REG_R10) fprintf(f, "r10 ");
  if (mask & REG_R11) fprintf(f, "r11 ");
  if (mask & REG_R12) fprintf(f, "r12 ");
  if (mask & REG_R13) fprintf(f, "r13 ");
  if (mask & REG_R14) fprintf(f, "r14 ");
  if (mask & REG_R15) fprintf(f, "r15 ");
}
