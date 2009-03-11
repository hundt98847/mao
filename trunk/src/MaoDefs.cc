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

// For each register, maintain a side data structure to
// manage mask bits, subregisters, the pointer to the actual
// register (lives in binutils).
//
class RegProps {
 public:
  RegProps(const reg_entry *reg,
	   int              num) :
    reg_(reg),
    num_(num),
    mask_(num),
    sub_regs_(num)
  {}

  BitString  &mask() { return mask_; }
  BitString  &sub_regs() { return sub_regs_; }
  const char *name() { return reg_->reg_name; }

  void        AddSubReg(BitString &bstr) {
    sub_regs_ = sub_regs_ | bstr;
  }

 private:
  const reg_entry *  reg_;
  int                num_;
  BitString          mask_;
  BitString          sub_regs_;
};

// Maintain maps to allow fast register (property) lookup by
//   name
//   property entry
//   register number
//
typedef std::map<const char *, RegProps *, ltstr> RegNameMap;
static RegNameMap reg_name_map;

typedef std::map<const reg_entry *, RegProps *> RegPtrMap;
static RegPtrMap reg_ptr_map;

typedef std::map<int , RegProps *> RegNumMap;
static RegNumMap reg_num_map;

// maintain pointer to rip - it's always modified, for every insn
//
static RegProps *rip_props = NULL;

// Create a register alias, simply enter another name
// into the name map.
//
static void AddAlias(const char *name,
		     const char *alias) {
  RegNameMap::iterator it = reg_name_map.find(name);

  MAO_ASSERT(it != reg_name_map.end());
  MAO_ASSERT(reg_name_map.find(alias) == reg_name_map.end());

  reg_name_map[alias] = (*it).second;
}


// Read Register Table, give every register a unique bit,
// create RegProps object for each register, fill maps.
//
int reg_max = 0;
void ReadRegisterTable() {
  int i;
  for (i = 0; ; ++i) {
    RegProps *r = new RegProps(&i386_regtab[i], i);

    reg_name_map[i386_regtab[i].reg_name] = r;
    reg_ptr_map[&i386_regtab[i]] = r;
    reg_num_map[i] = r;

    if (!strcmp("rip", i386_regtab[i].reg_name))
      rip_props = r;

    if (!strcmp("mxcsr", i386_regtab[i].reg_name))
      break;
  }
  reg_max = i;
  MAO_ASSERT(rip_props);
}

// Create Subreg Relations by populating registers
// sub_regs mask. Convention: A register is it's own
// sub register, e.g., %rax has subregisters %rax, %eax, ...
//
void AddSubRegs(const char *r64, const char *r32,
		const char *r16, const char *r8,
                const char *r8h = NULL) {
  RegProps *p64 = reg_name_map.find(r64)->second;
  RegProps *p32 = reg_name_map.find(r32)->second;
  if (r16) {
    RegProps *p16 = reg_name_map.find(r16)->second;
    if (r8) {
      RegProps *p8  = reg_name_map.find(r8)->second;
      p16->AddSubReg(p8->mask());
      if (r8h) {
        RegProps *p8h = reg_name_map.find(r8h)->second;
        p16->AddSubReg(p8h->mask());
      }
    }
    p32->AddSubReg(p16->sub_regs());
  }
  p64->AddSubReg(p32->sub_regs());
}

// For a given register mask, find the
// corresponding RegProps and additionally
// set the mask's subregs found in the RegProps
//
void FillSubRegs(BitString *mask) {
  if (mask->IsNonNull() && !mask->IsUndef()) {
    for (int i = 0; i < 4; i++)
      if (mask->GetWord(i))
        for (int j = 0; j < 64; j++) {
          if (i*64+j >= reg_max)
            break;

          if (mask->Get(i*64+j)) {
            RegProps *r = reg_num_map.find(i*64+j)->second;
            MAO_ASSERT(r);
            *mask = *mask | r->sub_regs();
          }
        }
  }
}


// Read register table, generate aliases, generate
// sub register relations.
//
void InitRegisters() {
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

  AddSubRegs("rax", "eax", "ax", "al", "ah");
  AddSubRegs("rcx", "ecx", "cx", "cl", "ch");
  AddSubRegs("rdx", "edx", "dx", "dl", "dh");
  AddSubRegs("rbx", "ebx", "bx", "bl", "bh");
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
  AddSubRegs("rip", "eip", NULL, NULL);

  for (unsigned int i = 0; i < def_entries_size; i++) {
    FillSubRegs(&def_entries[i].reg_mask);
    FillSubRegs(&def_entries[i].reg_mask8);
    FillSubRegs(&def_entries[i].reg_mask16);
    FillSubRegs(&def_entries[i].reg_mask32);
    FillSubRegs(&def_entries[i].reg_mask64);
  }
}

// For a given register name, return it's
// sub-register mask.
//
BitString GetMaskForRegister(const char *reg) {
  if (!reg)
    return BitString(0x0ull, 0x0ull, 0x0ull, 0x0ull);

  RegProps *rprops = reg_name_map.find(reg)->second;
  MAO_ASSERT(rprops);
  return rprops->sub_regs();
}

// For an instruction, check def masks, chec
// operands and if they define a register, add
// the masks to the results.
//
BitString GetRegisterDefMask(InstructionEntry *insn) {
  DefEntry *e = &def_entries[insn->op()];
  MAO_ASSERT(e->opcode = insn->op());

  BitString mask = e->reg_mask;

  // check 1st operand. If it is 8/16/32/64 bit operand, see whether
  // there are special register masks stored for insn.
  if (insn->NumOperands()) {
    if (insn->IsRegister8Operand(0) || insn->IsMem8Operand(0))
      mask = mask | e->reg_mask8;
    if (insn->IsRegister16Operand(0) || insn->IsMem16Operand(0))
      mask = mask | e->reg_mask16;
    if (insn->IsRegister32Operand(0) || insn->IsMem32Operand(0))
      mask = mask | e->reg_mask32;
    if (insn->IsRegister64Operand(0) || insn->IsMem32Operand(0))
      mask = mask | e->reg_mask64;
  }

  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        const char *reg = insn->GetRegisterOperandStr(op);
        mask = mask | GetMaskForRegister(reg);
       }
    }
  }
  return mask;
}

// Print register mask. Caution: Pretty slow implementation
//
void PrintRegisterDefMask(FILE *f, BitString mask, const char *title) {
  if (title)
    fprintf(f, "%s: ", title);
  for (int i = 0; i < 255; i++) {
    if (mask.Get(i)) {
      for (RegPtrMap::iterator it = reg_ptr_map.begin();
           it != reg_ptr_map.end(); ++it) {
        if ((*it).second->mask().Get(i)) {
          fprintf(f, "%s ", (*it).second->name());
          break;
        }
      }
    }
  }
  fprintf(f, "\n");
}

// See whether subreg is a part of reg
//
bool DefinesSubReg(reg_entry *reg,
                   reg_entry *subreg) {
  RegProps *preg = reg_ptr_map.find(reg)->second;
  RegProps *psubreg = reg_ptr_map.find(subreg)->second;

  if (((preg->sub_regs() - preg->mask()) & psubreg->sub_regs())
      == psubreg->sub_regs())
    return true;

  return false;
}

// See whether two registers overlap
//
bool       RegistersOverlap(const struct reg_entry *reg1,
                            const struct reg_entry *reg2) {
  RegProps *p1 = reg_ptr_map.find(reg1)->second;
  RegProps *p2 = reg_ptr_map.find(reg2)->second;
  MAO_ASSERT(p1);
  MAO_ASSERT(p2);

  return (p1->sub_regs() & p2->sub_regs()).IsNonNull();
}
