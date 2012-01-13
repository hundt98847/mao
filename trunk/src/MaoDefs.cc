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


#include "Mao.h"
#include "gen-defs.h"
#include "gen-uses.h"

#include <string.h>
#include <map>

// For each register, maintain a side data structure to
// manage mask bits, subregisters, the pointer to the actual
// register (lives in binutils).
//
class RegProps {
 public:
  RegProps(const reg_entry *reg,
           int              num)
    : reg_(reg),
      num_(num),
      mask_(),
      sub_regs_() {
    mask_.Set(num);
    sub_regs_.Set(num);
  }

  const reg_entry  *reg() { return reg_; }
  BitString        &mask() { return mask_; }
  BitString        &sub_regs() { return sub_regs_; }
  BitString        &parent_regs() { return parent_regs_; }
  const char       *name() { return reg_->reg_name; }
  int              num() { return num_; }

  void        AddSubReg(BitString &bstr) {
    sub_regs_ = sub_regs_ | bstr;
  }
  void        AddParentReg(BitString &bstr) {
    parent_regs_ = parent_regs_ | bstr;
  }

 private:
  const reg_entry *  reg_;
  int                num_;
  BitString          mask_;
  BitString          sub_regs_;
  BitString          parent_regs_;
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
// Map from dwarf2 register numbers to RegProps *
static RegNumMap reg_dw2num_map_i386;
static RegNumMap reg_dw2num_map_x86_64;

// maintain pointer to rip - it's always modified, for every insn
//
static RegProps *rip_props = NULL;
static RegProps *eip_props = NULL;

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
    // dw2_regnum[0] is for 32 bit and dw2_regnum[1] is for 64 bit
    if (i386_regtab[i].dw2_regnum[0] != Dw2Inval)
      reg_dw2num_map_i386[i386_regtab[i].dw2_regnum[0]] = r;
    if (i386_regtab[i].dw2_regnum[1] != Dw2Inval)
      reg_dw2num_map_x86_64[i386_regtab[i].dw2_regnum[1]] = r;

    if (!strcmp("rip", i386_regtab[i].reg_name))
      rip_props = r;
    if (!strcmp("eip", i386_regtab[i].reg_name))
      eip_props = r;

    // SSE flag register
    if (!strcmp("mxcsr", i386_regtab[i].reg_name))
      break;
  }
  reg_max = i;
  MAO_ASSERT(rip_props);
}


int GetNumberOfRegisters() {
  return reg_max;
}

// Create Subreg Relations by populating registers
// sub_regs mask. Convention: A register is it's own
// sub register, e.g., %rax has subregisters %rax, %eax, ...
//
void AddSubRegs(const char *r64, const char *r32,
                const char *r16, const char *r8,
                const char *r8_alias = NULL,
                const char *r8h = NULL) {
  // Find properties
  RegProps *p64 = reg_name_map.find(r64)->second;
  RegProps *p32 = reg_name_map.find(r32)->second;
  RegProps *p16 = NULL;
  RegProps *p8  = NULL;
  RegProps *p8_alias  = NULL;
  RegProps *p8h = NULL;

  // Assign sub-regs
  if (r16) {
    p16 = reg_name_map.find(r16)->second;
    if (r8) {
      p8  = reg_name_map.find(r8)->second;
      p16->AddSubReg(p8->mask());
      if (r8h) {
        p8h = reg_name_map.find(r8h)->second;
        p16->AddSubReg(p8h->mask());
      }
      if (r8_alias) {
        p8_alias  = reg_name_map.find(r8_alias)->second;
        p16->AddSubReg(p8_alias->mask());
      }
    }
    p32->AddSubReg(p16->sub_regs());
  }
  p64->AddSubReg(p32->sub_regs());

  // Assign parent-regs
  p32->AddParentReg(p64->mask());
  if (p16) {
    p16->AddParentReg(p64->mask());
    p16->AddParentReg(p32->mask());
  }
  if (p8) {
    p8->AddParentReg(p64->mask());
    p8->AddParentReg(p32->mask());
    if (p16)
      p8->AddParentReg(p16->mask());
  }
  if (p8_alias) {
    p8_alias->AddParentReg(p64->mask());
    p8_alias->AddParentReg(p32->mask());
    if (p16)
      p8_alias->AddParentReg(p16->mask());
  }
  if (p8h) {
    p8h->AddParentReg(p64->mask());
    p8h->AddParentReg(p32->mask());
    if (p16)
      p8h->AddParentReg(p16->mask());
  }
}

// For a given register mask, find the
// corresponding RegProps and additionally
// set the mask's subregs found in the RegProps
//
void FillSubRegs(BitString *mask) {
  if (mask->IsNonNull() && !mask->IsUndef()) {
    // Loop over bitstring (4 * 64 = 256 bits)
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

// For a given register mask, find the
// corresponding RegProps and additionally
// set the mask's parent regs found in the RegProps
//
void FillParentRegs(BitString *mask) {
  if (mask->IsNonNull() && !mask->IsUndef()) {
    // Loop over bitstring (4 * 64 = 256 bits)
    for (int i = 0; i < 4; i++)
      if (mask->GetWord(i))
        for (int j = 0; j < 64; j++) {
          if (i*64+j >= reg_max)
            break;

          if (mask->Get(i*64+j)) {
            RegProps *r = reg_num_map.find(i*64+j)->second;
            MAO_ASSERT(r);
            *mask = *mask | r->parent_regs();
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

  AddSubRegs("rax", "eax", "ax", "al", "axl", "ah");
  AddSubRegs("rcx", "ecx", "cx", "cl", "cxl", "ch");
  AddSubRegs("rdx", "edx", "dx", "dl", "dxl", "dh");
  AddSubRegs("rbx", "ebx", "bx", "bl", "bxl", "bh");
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
    FillParentRegs(&def_entries[i].reg_mask);
    FillParentRegs(&def_entries[i].reg_mask8);
    FillParentRegs(&def_entries[i].reg_mask16);
    FillParentRegs(&def_entries[i].reg_mask32);
    FillParentRegs(&def_entries[i].reg_mask64);
  }
  for (unsigned int i = 0; i < use_entries_size; i++) {
    FillSubRegs(&use_entries[i].reg_mask);
    FillSubRegs(&use_entries[i].reg_mask8);
    FillSubRegs(&use_entries[i].reg_mask16);
    FillSubRegs(&use_entries[i].reg_mask32);
    FillSubRegs(&use_entries[i].reg_mask64);
    FillParentRegs(&use_entries[i].reg_mask);
    FillParentRegs(&use_entries[i].reg_mask8);
    FillParentRegs(&use_entries[i].reg_mask16);
    FillParentRegs(&use_entries[i].reg_mask32);
    FillParentRegs(&use_entries[i].reg_mask64);
  }
}

// For a given register name, return it's
// sub-register mask.
//
BitString GetMaskForRegister(const reg_entry *reg) {
  if (!reg)
    return BitString(256, 4, 0x0ull, 0x0ull, 0x0ull, 0x0ull);

  RegProps *rprops = reg_ptr_map.find(reg)->second;
  MAO_ASSERT(rprops);
  return rprops->sub_regs();
}

// For an instruction, check def masks, check
// operands and if they define a register, add
// the masks to the results.
//
// If expand_mask is true, the mask includes all parent and child registers
// of the defined registers.
BitString GetRegisterDefMask(const InstructionEntry *insn,
                             bool expand_mask) {
  DefEntry *e = &def_entries[insn->op()];
  MAO_ASSERT(e->opcode == insn->op());

  BitString mask = e->reg_mask;

  //TODO: Do not blindly apply the operand width based masks
  //The masks are blindly applied because in certain instructions
  //without explicit operands the operand width is encoded in
  //the opcode and the previous system of determining it based
  //on the type of the operand fails.
  //
  mask = mask | e->reg_mask8;
  mask = mask | e->reg_mask16;
  mask = mask | e->reg_mask32;
  mask = mask | e->reg_mask64;

  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        const reg_entry *reg = insn->GetRegisterOperand(op);
        mask = mask | GetMaskForRegister(reg);
       }
    }
  }
  //The following code is to handle an instruction like
  // sar %rax, which actually means
  // sar $1, %rax
  // In the MaoDefs.tbl, it says that the operation writes
  // to its dest operand which is interpreted as operand 1,
  // but is actually operand 0 in this case
  if ( (insn->NumOperands() == 1) &&
       (insn->IsRegisterOperand(0)) &&
       (e->op_mask & (1 << 1)) ) {
    mask = mask | GetMaskForRegister(insn->GetRegisterOperand(0));
  }

  DefEntry *prefix_entry = NULL;
  if (insn->HasPrefix (REPE_PREFIX_OPCODE))
    prefix_entry = &def_entries[OP_repe];
  else if (insn->HasPrefix (REPNE_PREFIX_OPCODE))
    prefix_entry = &def_entries[OP_repne];

  if (prefix_entry != NULL) {
    BitString prefix_mask = prefix_entry->reg_mask;
    mask = mask | prefix_mask;
  }
  if (expand_mask) {
    FillSubRegs(&mask);
    FillParentRegs(&mask);
  }
  return mask;
}

// For an instruction, check use masks, check
// operands and if they use a register, add
// the masks to the results.
//
// If expand_mask is true, the mask includes all parent and child registers
// of the defined registers.
BitString GetRegisterUseMask(const InstructionEntry *insn,
                             bool expand_mask) {
  UseEntry *e = &use_entries[insn->op()];
  MAO_ASSERT(e->opcode == insn->op());

  BitString mask = e->reg_mask;

  //TODO: Do not blindly apply the operand width based masks
  //The masks are blindly applied because in certain instructions
  //without explicit operands the operand width is encoded in
  //the opcode and the previous system of determining it based
  //on the type of the operand fails.
  //
  mask = mask | e->reg_mask8;
  mask = mask | e->reg_mask16;
  mask = mask | e->reg_mask32;
  mask = mask | e->reg_mask64;

  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        mask = mask | GetMaskForRegister(insn->GetRegisterOperand(op));
       }
    }
  }
  if (insn->HasBaseRegister() && (e->op_mask & REG_OP_BASE)) {
    mask = mask | GetMaskForRegister(insn->GetBaseRegister());
  }
  if (insn->HasIndexRegister() && (e->op_mask & REG_OP_INDEX)) {
    mask = mask | GetMaskForRegister(insn->GetIndexRegister());
  }
  UseEntry *prefix_entry = NULL;
  if (insn->HasPrefix (REPE_PREFIX_OPCODE))
    prefix_entry = &use_entries[OP_repe];
  else if (insn->HasPrefix (REPNE_PREFIX_OPCODE))
    prefix_entry = &use_entries[OP_repne];

  if (prefix_entry != NULL) {
    BitString prefix_mask = prefix_entry->reg_mask;
    mask = mask | prefix_mask;
  }
  if (expand_mask) {
    FillSubRegs(&mask);
    FillParentRegs(&mask);
  }
  return mask;
}


// Returns the set of defined registers from the list of operands.
// TODO(martint): Add implicit registers.
std::set<const reg_entry *> GetDefinedRegisters(InstructionEntry *insn) {
  std::set<const reg_entry *> regs;

  DefEntry *e = &def_entries[insn->op()];
  MAO_ASSERT(e->opcode == insn->op());

  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        const reg_entry *reg = insn->GetRegisterOperand(op);
        regs.insert(reg);
       }
    }
  }
  return regs;
}

// Returns the set of used registers from the list of operands.
// TODO(martint): Clean up code not to repeat the same logic as
// GetRegisterUseMask()
std::set<const reg_entry *> GetUsedRegisters(InstructionEntry *insn) {
  std::set<const reg_entry *> regs;

  UseEntry *e = &use_entries[insn->op()];
  MAO_ASSERT(e->opcode == insn->op());


  for (int op = 0; op < 5 && op < insn->NumOperands(); ++op) {
    if (e->op_mask & (1 << op)) {
      if (insn->IsRegisterOperand(op)) {
        const reg_entry *reg = insn->GetRegisterOperand(op);
        regs.insert(reg);
       }
    }
  }

  if (insn->HasBaseRegister() && (e->op_mask & REG_OP_BASE)) {
    const reg_entry *reg = insn->GetBaseRegister();
    regs.insert(reg);
  }
  if (insn->HasIndexRegister() && (e->op_mask & REG_OP_INDEX)) {
    const reg_entry *reg = insn->GetIndexRegister();
    regs.insert(reg);
  }

  return regs;
}



// Returns a mask for the live in registers.
// TODO(martint): Take the ABI into considerations
BitString GetCallingConventionDefMask() {
  BitString mask;
  const char *amd64_abi_input_regs[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9", "xmm0", "xmm1", "xmm2",
    "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};
  for (unsigned int i = 0;
       i < sizeof(amd64_abi_input_regs)/sizeof(const char *);
       ++i) {
    MAO_ASSERT(!(mask ==
                 (mask | GetMaskForRegister(
                     GetRegFromName(amd64_abi_input_regs[i])))));
    mask = mask | GetMaskForRegister(GetRegFromName(amd64_abi_input_regs[i]));
  }
  return mask;
}


// Print register mask.
//
void PrintRegistersInRegisterMask(FILE *f, BitString mask, const char *title) {
  if (title)
    fprintf(f, "%s: ", title);
  for (int i = 0; i < 255; i++) {
    if (mask.Get(i)) {
      fprintf(f, "%s ", reg_num_map[i]->name());
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
bool       RegistersOverlap(const reg_entry *reg1,
                            const reg_entry *reg2) {
  RegProps *p1 = reg_ptr_map.find(reg1)->second;
  RegProps *p2 = reg_ptr_map.find(reg2)->second;
  MAO_ASSERT(p1);
  MAO_ASSERT(p2);

  return (p1->sub_regs() & p2->sub_regs()).IsNonNull();
}

// All bits in child must be set in parent
//
bool       RegistersContained(BitString &parent,
                              BitString &child) {
  return (parent & child) == child;
}

bool       IsParent(const reg_entry *parent,
                    const reg_entry *child) {
  BitString  parents = GetParentRegs(child);
  // is parent in parents?
  RegProps *preg = reg_ptr_map.find(parent)->second;
  return ((preg->mask() & parents).IsNonNull());
}

bool       IsParentNum(int parent,
                       int child) {
  return IsParent(reg_num_map[parent]->reg(),
                  reg_num_map[child]->reg());
}


BitString  GetParentRegs(const reg_entry *reg) {
  RegProps *p1 = reg_ptr_map.find(reg)->second;
  MAO_ASSERT(p1);
  return p1->parent_regs();
}

// TODO(rhundt): Change this to eip for 32-bit compiles
const reg_entry *GetIP() {
  // if 64-bit mode
  return rip_props->reg();
  // else
  // return eip_props->reg();
}


const reg_entry *GetRegFromName(const char *reg_name) {
  RegNameMap::iterator it = reg_name_map.find(reg_name);

  MAO_ASSERT(it != reg_name_map.end());
  return (*it).second->reg();
}

const char *GetRegName(int reg_number) {
  RegNumMap::iterator it = reg_num_map.find(reg_number);
  if (reg_num_map.end() == it) {
    fprintf(stderr, "Unable to find register :%d\n", reg_number);
  }
  MAO_ASSERT(it != reg_num_map.end());
  return (*it).second->reg()->reg_name;
}

int GetRegNum(const char *reg_name) {
  RegNameMap::iterator it = reg_name_map.find(reg_name);

  MAO_ASSERT(it != reg_name_map.end());
  return (*it).second->num();
}

// Returns  the reg_entry corresponding to the register numbers used by DWARF.
// The flag is_64_bit tells whether the code is 64 bit or not.
const reg_entry *GetRegFromDwarfNumber(int dw2_num, bool is_64_bit) {
  RegNumMap::iterator it;
  if (is_64_bit) {
    it = reg_dw2num_map_x86_64.find(dw2_num);
    MAO_ASSERT(it != reg_dw2num_map_x86_64.end());
  } else {
    it = reg_dw2num_map_i386.find(dw2_num);
    MAO_ASSERT(it != reg_dw2num_map_i386.end());
  }
  return (*it).second->reg();
}

bool IsRegDefined(InstructionEntry *insn, const reg_entry *reg) {
  // TODO(martint): Improve this slow implementation.
  std::set<const reg_entry *> defined_regs = GetDefinedRegisters(insn);
  for (std::set<const reg_entry *>::const_iterator iter =
           defined_regs.begin();
       iter != defined_regs.end();
       ++iter) {
    if (RegistersOverlap(*iter, reg)) {
      return true;
    }
  }
  return false;
}
