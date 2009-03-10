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
  RegProps(reg_entry  *reg,
	   int         num) : 
    reg_(reg),
    num_(num),
    mask_(num),
    sub_regs_(num) 
  {}

  BitString  &mask() { return mask_; }
  BitString  &subs() { return sub_regs_; }
  const char *name() { return reg_->reg_name; }

  void        AddSubReg(BitString &bstr) {
    sub_regs_ = sub_regs_ + bstr;
  }

 private:
  reg_entry *        reg_;
  int                num_;
  BitString          mask_;
  BitString          sub_regs_;
};

typedef std::map<const char *, RegProps *, ltstr> RegNameMap;
static RegNameMap reg_name_map;

typedef std::map<reg_entry *, RegProps *> RegPtrMap;
static RegPtrMap reg_ptr_map;

// Create a register alias
//
static void AddAlias(const char *name, 
		     const char *alias) {
  RegNameMap::iterator it = reg_name_map.find(name);

  MAO_ASSERT(it != reg_name_map.end());
  MAO_ASSERT(reg_name_map.find(alias) == reg_name_map.end());

  reg_name_map[alias] = (*it).second;
}


// Read Register Table, give every register a unique bit,
// create RegProps object for each register.
//
void ReadRegisterTable() {
  extern reg_entry i386_regtab[];

  for (unsigned int i = 0; ; ++i) {
    RegProps *r = new RegProps(&i386_regtab[i], i);
    reg_name_map[i386_regtab[i].reg_name] = r;
    reg_ptr_map[&i386_regtab[i]] = r;

    if (!strcmp("mxcsr", i386_regtab[i].reg_name))
      break;
  }
}

// Create Subreg Relations
//
void AddSubRegs(const char *r64, const char *r32,
		const char *r16, const char *r8) {
  RegProps *p64 = reg_name_map.find(r64)->second;
  RegProps *p32 = reg_name_map.find(r32)->second;
  RegProps *p16 = reg_name_map.find(r16)->second;
  RegProps *p8  = reg_name_map.find(r8)->second;
  p16->AddSubReg(p8->mask());
  p32->AddSubReg(p16->subs());
  p64->AddSubReg(p32->subs());
}

static void InitRegProps() {
  ReadRegisterTable();

  AddAlias("al",  "r0b");
  AddAlias("ax",  "r0w");
  AddAlias("eax", "r0d");
  AddAlias("rax", "r0");

  AddAlias("cl",  "r1b");
  AddAlias("cx",  "r1w");
  AddAlias("ecx", "r1d");
  AddAlias("rcx", "r1");

  AddAlias("dl",  "r2b");
  AddAlias("dx",  "r2w");
  AddAlias("edx", "r2d");
  AddAlias("rdx", "r2");

  AddAlias("bl",  "r3b");
  AddAlias("bx",  "r3w");
  AddAlias("ebx", "r3d");
  AddAlias("rbx", "r3");

  AddAlias("spl", "r4b");
  AddAlias("sp",  "r4w");
  AddAlias("esp", "r4d");
  AddAlias("rsp", "r4");

  AddAlias("bpl",  "r5b");
  AddAlias("bp",  "r5w");
  AddAlias("ebp", "r5d");
  AddAlias("rbp", "r5");

  AddAlias("sil", "r5bw");
  AddAlias("si",  "r6w");
  AddAlias("esi", "r6d");
  AddAlias("rsi", "r6");

  AddAlias("dil", "r7b");
  AddAlias("di",  "r7w");
  AddAlias("edi", "r7d");
  AddAlias("rdi", "r7");

  AddSubRegs("rax", "eax", "ax", "al");
  AddSubRegs("rcx", "ecx", "cx", "cl");
  AddSubRegs("rdx", "edx", "dx", "dl");
  AddSubRegs("rbx", "ebx", "bx", "bl");
  AddSubRegs("rsp", "esp", "sp", "spl");
  AddSubRegs("rbp", "ebp", "bp", "bpl");
  AddSubRegs("rsi", "esi", "si", "sil");
  AddSubRegs("rdi", "edi", "di", "dil");
  AddSubRegs("r8" , "r8d",  "r8w",  "r8b");
  AddSubRegs("r9" , "r9d",  "r9w",  "r9b");
  AddSubRegs("r10", "r10d", "r10w", "r10b");
  AddSubRegs("r11", "r11d", "r11w", "r11b");
  AddSubRegs("r12", "r12d", "r12w", "r12b");
  AddSubRegs("r13", "r13d", "r13w", "r13b");
  AddSubRegs("r14", "r14d", "r14w", "r14b");
  AddSubRegs("r15", "r15d", "r15w", "r15b");
}

unsigned long long GetMaskForRegister(const char *reg) {
  static bool regs_initialized = false;
  if (!regs_initialized) { 
    InitRegProps();
    regs_initialized = true;
  }

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

// See whether pmask defines subregs masked in imask
bool DefinesSubReg64(unsigned long long pmask,
                     unsigned long long imask) {
  if ((pmask & REG_RAX) == REG_RAX) {
    if ((imask & REG_EAX) == REG_EAX)
      return true;
  }
  if ((pmask & REG_RCX) == REG_RCX) {
    if ((imask & REG_ECX) == REG_ECX)
      return true;
  }
  if ((pmask & REG_RDX) == REG_RDX) {
    if ((imask & REG_EDX) == REG_EDX)
      return true;
  }
  if ((pmask & REG_RBX) == REG_RBX) {
    if ((imask & REG_EBX) == REG_EBX)
      return true;
  }
  return false;
}
