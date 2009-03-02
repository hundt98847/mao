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

#include <string.h>
#include <map>

class RegProps {
 public:
  RegProps(const char *name,
	   unsigned long long mask) : name_(name), mask_(mask) {}

  unsigned long long mask() { return mask_; }
  const char * name() { return name_; }

 private:
  const char *       name_;
  unsigned long long mask_;
};

typedef std::map<const char *, RegProps *, ltstr> RegMap;
static RegMap *reg_map = NULL;

static RegProps *RegReg(const char *name, 
			const char *alias, 
			unsigned long long mask) {
  RegProps *r = new RegProps(name, mask);
  (*reg_map)[name] = r;
  if (alias)
    (*reg_map)[alias] = r;
  return r;
}

static void InitRegProps() {
  reg_map = new RegMap;

  RegReg("al",  "r0b", REG_AL);
  RegReg("ah",  NULL,  REG_AH);
  RegReg("ax",  "r0w", REG_AX  | REG_AH  | REG_AL);
  RegReg("eax", "r0d", REG_EAX | REG_AX  | REG_AH | REG_AL);
  RegReg("rax", "r0",  REG_RAX | REG_EAX | REG_AX | REG_AH | REG_AL);

  RegReg("cl",  "r1b", REG_CL);
  RegReg("ch",  NULL,  REG_CH);
  RegReg("cx",  "r1w", REG_CX  | REG_CH  | REG_CL);
  RegReg("ecx", "r1d", REG_ECX | REG_CX  | REG_CH | REG_CL);
  RegReg("rcx", "r1",  REG_RCX | REG_ECX | REG_CX | REG_CH | REG_CL);

  RegReg("dl",  "r2b", REG_DL);
  RegReg("dh",  NULL,  REG_DH);
  RegReg("dx",  "r2w", REG_DX  | REG_DH  | REG_DL);
  RegReg("edx", "r2d", REG_EDX | REG_DX  | REG_DH | REG_DL);
  RegReg("rdx", "r2",  REG_RDX | REG_EDX | REG_DX | REG_DH | REG_DL);

  RegReg("bl",  "r3b", REG_BL);
  RegReg("bh",  NULL,  REG_BH);
  RegReg("bx",  "r3w", REG_BX  | REG_BH  | REG_BL);
  RegReg("ebx", "r3d", REG_EBX | REG_BX  | REG_BH | REG_BL);
  RegReg("rbx", "r3",  REG_RBX | REG_EBX | REG_BX | REG_BH | REG_BL);

  // TODO(rhundt): Model SPL
  RegReg("sp",  "r4w", REG_SP);
  RegReg("esp", "r4d", REG_ESP | REG_SP);
  RegReg("rsp", "r4",  REG_RSP | REG_ESP | REG_SP);

  RegReg("bp",  "r5w", REG_BP);
  RegReg("ebp", "r5d", REG_EBP | REG_BP);
  RegReg("rbp", "r5",  REG_RBP | REG_EBP | REG_BP);

  RegReg("si",  "r6w", REG_SI);
  RegReg("esi", "r6d", REG_ESI | REG_SI);
  RegReg("rsi", "r6",  REG_RSI | REG_ESI | REG_SI);

  RegReg("di",  "r7w", REG_DI);
  RegReg("edi", "r7d", REG_EDI | REG_DI);
  RegReg("rdi", "r7",  REG_RDI | REG_EDI | REG_DI);

  RegReg("r8b", NULL, REG_R8B);
  RegReg("r8w", NULL, REG_R8W | REG_R8B);
  RegReg("r8d", NULL, REG_R8D | REG_R8W | REG_R8B);
  RegReg("r8",  NULL, REG_R8  | REG_R8D | REG_R8W | REG_R8B);

  RegReg("r9b", NULL, REG_R9B);
  RegReg("r9w", NULL, REG_R9W | REG_R9B);
  RegReg("r9d", NULL, REG_R9D | REG_R9W | REG_R9B);
  RegReg("r9",  NULL, REG_R9  | REG_R9D | REG_R9W | REG_R9B);

  RegReg("r10b", NULL, REG_R10B);
  RegReg("r10w", NULL, REG_R10W | REG_R10B);
  RegReg("r10d", NULL, REG_R10D | REG_R10W | REG_R10B);
  RegReg("r10",  NULL, REG_R10  | REG_R10D | REG_R10W | REG_R10B);

  RegReg("r11b", NULL, REG_R11B);
  RegReg("r11w", NULL, REG_R11W | REG_R11B);
  RegReg("r11d", NULL, REG_R11D | REG_R11W | REG_R11B);
  RegReg("r11",  NULL, REG_R11  | REG_R11D | REG_R11W | REG_R11B);

  RegReg("r12b", NULL, REG_R12B);
  RegReg("r12w", NULL, REG_R12W | REG_R12B);
  RegReg("r12d", NULL, REG_R12D | REG_R12W | REG_R12B);
  RegReg("r12",  NULL, REG_R12  | REG_R12D | REG_R12W | REG_R12B);

  RegReg("r13b", NULL, REG_R13B);
  RegReg("r13w", NULL, REG_R13W | REG_R13B);
  RegReg("r13d", NULL, REG_R13D | REG_R13W | REG_R13B);
  RegReg("r13",  NULL, REG_R13  | REG_R13D | REG_R13W | REG_R13B);

  RegReg("r14b", NULL, REG_R14B);
  RegReg("r14w", NULL, REG_R14W | REG_R14B);
  RegReg("r14d", NULL, REG_R14D | REG_R14W | REG_R14B);
  RegReg("r14",  NULL, REG_R14  | REG_R14D | REG_R14W | REG_R14B);

  RegReg("r15b", NULL, REG_R15B);
  RegReg("r15w", NULL, REG_R15W | REG_R15B);
  RegReg("r15d", NULL, REG_R15D | REG_R15W | REG_R15B);
  RegReg("r15",  NULL, REG_R15  | REG_R15D | REG_R15W | REG_R15B);
}

unsigned long long GetMaskForRegister(const char *reg) {
  static bool regs_initialized = false;
  if (!regs_initialized) 
    InitRegProps();

  unsigned long long mask = 0ULL;

  if (!reg) return mask;

  if (!strcmp(reg, "al")  || !strcmp(reg, "r0b")) mask |= REG_AL; else
  if (!strcmp(reg, "ah"))  mask |= REG_AH; else
  if (!strcmp(reg, "ax")  || !strcmp(reg, "r0w")) mask |= REG_AX  | REG_AH  | REG_AL; else
  if (!strcmp(reg, "eax") || !strcmp(reg, "r0d")) mask |= REG_EAX | REG_AX  | REG_AH | REG_AL; else
  if (!strcmp(reg, "rax") || !strcmp(reg, "r0"))  mask |= REG_RAX | REG_EAX | REG_AX | REG_AH | REG_AL; else

  if (!strcmp(reg, "cl")  || !strcmp(reg, "r1b")) mask |= REG_CL; else
  if (!strcmp(reg, "ch"))  mask |= REG_CH; else
  if (!strcmp(reg, "cx")  || !strcmp(reg, "r1w")) mask |= REG_CX  | REG_CH  | REG_CL; else
  if (!strcmp(reg, "ecx") || !strcmp(reg, "r1d")) mask |= REG_ECX | REG_CX  | REG_CH | REG_CL; else
  if (!strcmp(reg, "rcx") || !strcmp(reg, "r1"))  mask |= REG_RCX | REG_ECX | REG_CX | REG_CH | REG_CL; else

  if (!strcmp(reg, "dl")  || !strcmp(reg, "r2b")) mask |= REG_DL; else
  if (!strcmp(reg, "dh"))  mask |= REG_DH; else
  if (!strcmp(reg, "dx")  || !strcmp(reg, "r2w")) mask |= REG_DX  | REG_DH  | REG_DL; else
  if (!strcmp(reg, "edx") || !strcmp(reg, "r2d")) mask |= REG_EDX | REG_DX  | REG_DH | REG_DL; else
  if (!strcmp(reg, "rdx") || !strcmp(reg, "r2"))  mask |= REG_RDX | REG_EDX | REG_DX | REG_DH | REG_DL; else

  if (!strcmp(reg, "bl")  || !strcmp(reg, "r3b")) mask |= REG_BL; else
  if (!strcmp(reg, "bh"))  mask |= REG_BH; else
  if (!strcmp(reg, "bx")  || !strcmp(reg, "r3w")) mask |= REG_BX  | REG_BH  | REG_BL; else
  if (!strcmp(reg, "ebx") || !strcmp(reg, "r3d")) mask |= REG_EBX | REG_BX  | REG_BH | REG_BL; else
  if (!strcmp(reg, "rbx") || !strcmp(reg, "r3"))  mask |= REG_RBX | REG_EBX | REG_BX | REG_BH | REG_BL; else

  if (!strcmp(reg, "sp"))   mask |= REG_SP; else
  if (!strcmp(reg, "esp"))  mask |= REG_ESP | REG_SP; else
  if (!strcmp(reg, "rsp"))  mask |= REG_RSP | REG_ESP | REG_SP; else

  if (!strcmp(reg, "bp"))   mask |= REG_BP; else
  if (!strcmp(reg, "ebp"))  mask |= REG_EBP | REG_BP; else
  if (!strcmp(reg, "rbp"))  mask |= REG_RBP | REG_EBP | REG_BP; else

  if (!strcmp(reg, "si"))   mask |= REG_SI; else
  if (!strcmp(reg, "esi"))  mask |= REG_ESI | REG_SI; else
  if (!strcmp(reg, "rsi"))  mask |= REG_RSI | REG_ESI | REG_SI; else

  if (!strcmp(reg, "di"))   mask |= REG_DI; else
  if (!strcmp(reg, "edi"))  mask |= REG_EDI | REG_DI; else
  if (!strcmp(reg, "rdi"))  mask |= REG_RDI | REG_EDI | REG_DI; else

  if (!strcmp(reg, "r8"))   mask |= REG_R8; else
  if (!strcmp(reg, "r9"))   mask |= REG_R9; else
  if (!strcmp(reg, "r10"))  mask |= REG_R10; else
  if (!strcmp(reg, "r11"))  mask |= REG_R11; else
  if (!strcmp(reg, "r12"))  mask |= REG_R12; else
  if (!strcmp(reg, "r13"))  mask |= REG_R13; else
  if (!strcmp(reg, "r14"))  mask |= REG_R14; else
  if (!strcmp(reg, "r15"))  mask |= REG_R15;

  return mask;
}

unsigned long long GetRegisterDefMask(InstructionEntry *insn) {
  DefEntry *e = &def_entries[insn->op()];
  MAO_ASSERT(e->opcode = insn->op());

  unsigned long long mask = e->reg_mask;

  // check 1st operand. If it is 8/16/32/64 bit operand, see whether
  // there are special register masks stored for insn.
  if (insn->NumOperands()) {
    if (insn->IsRegister8Operand(0) || insn->IsMem8Operand(0))
      mask |= e->reg_mask8;
    if (insn->IsRegister16Operand(0) || insn->IsMem16Operand(0))
      mask |= e->reg_mask16;
    if (insn->IsRegister32Operand(0) || insn->IsMem32Operand(0))
      mask |= e->reg_mask32;
    if (insn->IsRegister64Operand(0) || insn->IsMem32Operand(0))
      mask |= e->reg_mask64;
  }

  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        const char *reg = insn->GetRegisterOperand(op);
        mask |= GetMaskForRegister(reg);
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
