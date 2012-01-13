//
// Copyright 2010 Google Inc.
//
// This program is free software; you can redistribute it and/or to
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

// Instruction builder plugin. Given a file with a single assembly
// instruction, this plugin prints C++ code that fills in an i386_insn
// structure corresponding to this instruction.

#include "Mao.h"
#include "as.h"
#include "tc-i386.h"
namespace {
PLUGIN_VERSION
}

void PrintTemplate(const insn_template *tm, int num_operands);
void PrintOperandTypes(const i386_operand_type *operand_types, int num_operands);
void PrintOpcodeModifier(i386_opcode_modifier modifier);
void PrintOperandTypes2(const i386_operand_type *types, int num_operands);
void PrintFlags(const unsigned int *flags);
void PrintRelocs(const enum bfd_reloc_code_real *reloc);
void PrintModRm(const modrm_byte *modrm);
void PrintSib(const sib_byte *sib);
void PrintDrex(const drex_byte *drex);
void PrintVexPrefix(const vex_prefix *vex);
void PrintPrefixes(unsigned int prefixes, const unsigned char *prefix);
void PrintI386InsnStruct(const i386_insn *instruction);
void PrintOperands(const i386_insn *i);

// Macros to simplify printing

// Prints a format string and a value if the value is non-zero
#define PrintNonZero(fmt_str, value) \
    if (value) printf("  "fmt_str, value)

// Print with indentation after stringifying the first argument
#define PrettyPrint(x, y)\
    printf("  "#x, y)


// Print a field in prefix (dot) structure if it is non-zero
#define PrintNonZeroDot(prefix, structure, field, format_str) \
    if (structure.field) \
    PrettyPrint(prefix.structure.field = format_str;\n, structure.field)

// Print a field in prefix (arrow) structure if it is non-zero
#define PrintNonZeroArrow(prefix, structure, field, format_str) \
    if (structure.field) \
    PrettyPrint(prefix->structure.field = format_str;\n, structure.field)

// Print a field in prefix (arrow) structure
#define PrintArrow(prefix, structure, field, format_str) \
    PrettyPrint(prefix->structure.field = format_str;\n, structure->field)

MAO_DEFINE_OPTIONS(INSBUILDPLUG, "Generates i386_insn structure corresponding "\
                   "to an instruction in the input file", 0) {
};

class InstructionBuilderPlugin : public MaoPass {
 public:
  InstructionBuilderPlugin(MaoOptionMap *options, MaoUnit *mao)
      : MaoPass("INSBUILDPLUG", options, mao) { }

  bool Go() {
    InstructionEntry *insn = NULL;
    for (ConstSectionIterator section = unit_->ConstSectionBegin();
          section != unit_->ConstSectionEnd(); ++section) {
      for (EntryIterator entry = (*section)->EntryBegin();
           entry != (*section)->EntryEnd(); ++entry) {
        if ((*entry)->IsInstruction()) {
          // We expect the file to have only one insruction
          MAO_RASSERT_MSG(insn == NULL,
                          "More than one instruction found in input file\n");
          insn = (*entry)->AsInstruction();
        }
      }
    }
    PrintI386InsnStruct(insn->instruction());
    return true;
  }
};


// External Entry Point
//
void MaoInit() {
  RegisterUnitPass(
      "INSBUILDPLUG",
      MaoPassManager::GenericPassCreator<InstructionBuilderPlugin>);
}

void PrintI386InsnStruct(const i386_insn *instruction) {
  // Print headers
  printf("extern \"C\" {\n  #include \"as.h\"\n  #include \"tc-i386.h\"\n}\n");
  printf("#include \"MaoDefs.h\"\n");
  printf("void FillInstructionDetails(i386_insn *i) {\n");

  printf("  // Zero out the structure.\n");
  printf("  memset(i, 0, sizeof(*i));\n");
  PrintTemplate(&(instruction->tm), instruction->operands);

  printf("  i->suffix = %d;\n", instruction->suffix);
  printf("  i->operands= %d;\n", instruction->operands);
  printf("  i->reg_operands= %d;\n", instruction->reg_operands);
  printf("  i->disp_operands= %d;\n", instruction->disp_operands);
  printf("  i->mem_operands= %d;\n", instruction->mem_operands);
  printf("  i->imm_operands= %d;\n", instruction->imm_operands);
  PrintOperandTypes2(instruction->types, instruction->operands);
  PrintOperands(instruction);
  PrintFlags(instruction->flags);
  PrintRelocs(instruction->reloc);
  if (instruction->base_reg != NULL)
    PrintNonZero("i->base_reg = GetRegFromName (\"%s\");\n",
                 instruction->base_reg->reg_name);
  if (instruction->index_reg != NULL)
    PrintNonZero("i->index_reg = GetRegFromName (\"%s\");\n",
                 instruction->index_reg->reg_name);
  PrintNonZero("i->log2_scale_factor = %d;\n",
               instruction->log2_scale_factor);

  // TODO(eraman): Fill the segentry fields for the few cases they matter
  PrintPrefixes(instruction->prefixes, instruction->prefix);
  PrintModRm(&(instruction->rm));
  PrintNonZero("i->rex = %d;\n", instruction->rex);
  PrintSib(&(instruction->sib));
  PrintDrex(&(instruction->drex));
  PrintVexPrefix(&(instruction->vex));
  printf("}\n");
}

void PrintTemplate(const insn_template *tm, int num_operands) {
  PrintArrow(i, tm, name, strdup("%s"));
  PrintArrow(i, tm, operands, %d);
  PrintArrow(i, tm, base_opcode, %d);
  PrintArrow(i, tm, extension_opcode, %d);
  PrintArrow(i, tm, opcode_length, %d);
  /* Omitting CPU flags for now. Seems to be all zeroes */

  PrintOpcodeModifier(tm->opcode_modifier);
  PrintOperandTypes(tm->operand_types, num_operands);
}

void PrintModRm(const modrm_byte *modrm) {
  PrintNonZero("i->rm.regmem = %u;\n", modrm->regmem);
  PrintNonZero("i->rm.reg = %u;\n", modrm->reg);
  PrintNonZero("i->rm.mode = %u;\n", modrm->mode);
}

void PrintSib(const sib_byte *sib) {
  PrintNonZero("i->sib.base = %u;\n", sib->base);
  PrintNonZero("i->sib.index = %u;\n", sib->index);
  PrintNonZero("i->sib.scale = %u;\n", sib->scale);
}

void PrintDrex(const drex_byte *drex) {
  PrintNonZero("i->drex.reg = %u;\n", drex->reg);
  PrintNonZero("i->drex.rex = %u;\n", drex->rex);
  PrintNonZero("i->drex.modrm_reg = %u;\n", drex->modrm_reg);
  PrintNonZero("i->drex.modrm_regmem = %u;\n", drex->modrm_regmem);
}

void PrintVexPrefix(const vex_prefix *vex) {
  PrintNonZero("i->vex.bytes[0] = %u;\n", vex->bytes[0]);
  PrintNonZero("i->vex.bytes[1] = %u;\n", vex->bytes[1]);
  PrintNonZero("i->vex.bytes[2] = %u;\n", vex->bytes[2]);
  PrintNonZero("i->vex.length = %u;\n", vex->length);
  if (vex->register_specifier)
    PrintNonZero("  i->vex.register_specifier = GetRegFromName (\"%s\");\n",
                 vex->register_specifier->reg_name);
}

void PrintPrefixes(unsigned int prefixes, const unsigned char *prefix) {
  if (!prefixes)
    return;
  printf("  i->prefixes = %u;\n", prefixes);

  for (unsigned int i = 0; i < prefixes; i++) {
    if (prefix[i])
      printf("  i->prefix[%d] = %u;\n", i, prefix[i]);
  }
}

void PrintFlags(const unsigned int *flags) {
  for (int i = 0; i < MAX_OPERANDS; i++) {
    if (flags[i])
      printf("  i->flags[%d] = %u;\n", i, flags[i]);
  }
}

void PrintRelocs(const enum bfd_reloc_code_real *reloc) {
  for (int i = 0; i < MAX_OPERANDS; i++) {
    if (reloc[i])
      printf("  i->reloc[%d] = static_cast<bfd_reloc_code_real>(%u);\n",
             i, reloc[i]);
  }
}

// Prints the bitfields that describe the types of the operands
void PrintOperandTypes(const i386_operand_type *operand_types, int num_operands) {
  printf("  int j;\n");
  for (int j = 0; j < num_operands; j++) {
    printf("\n  j = %d;\n", j);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.reg8, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.reg16, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.reg32, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.reg64, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.floatreg, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.regmmx, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.regxmm, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.regymm, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.control, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.debug, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.test, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.sreg2, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.sreg3, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm1, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm8, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm8s, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm16, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm32, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm32, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.imm64, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.disp8, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.disp16, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.disp32, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.disp32s, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.disp64, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.acc, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.floatacc, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.baseindex, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.inoutportreg, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.shiftcount, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.jumpabsolute, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.esseg, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.regmem, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.mem, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.byte, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.word, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.dword, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.fword, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.qword, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.tbyte, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.xmmword, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.ymmword, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.unspecified, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.anysize, %d);
    PrintNonZeroDot(i->tm, operand_types[j], bitfield.vex_imm4, %d);
  }
}

void PrintOperands(const i386_insn *i) {
  for (unsigned int j = 0; j < i->operands; j++) {
    if (InstructionEntry::IsRegisterOperand(i, j)) {
      printf("  i->op[%u].regs = GetRegFromName (\"%s\");\n",
             j, i->op[j].regs->reg_name);
    } else if (InstructionEntry::IsImmediateOperand(i, j)) {
      printf("  i->op[%u].imms = "
             "static_cast<expressionS*>xmalloc(sizeof(expressionS));\n",
             j);
      if (i->op[j].imms->X_add_symbol) {
        const char *name = S_GET_NAME(i->op[j].imms->X_add_symbol);
        printf("  i->op[%u].imms->X_add_symbol = "
               "symbol_find_or_make(\"%s\");\n", j, name);
      }
      if (i->op[j].imms->X_op_symbol) {
        const char *name = S_GET_NAME(i->op[j].imms->X_op_symbol);
        printf("  i->op[%u].imms->X_op_symbol = "
               "symbol_find_or_make(\"%s\");\n", j, name);
      }
      printf("  i->op[%u].imms->X_add_number = %ld;\n",
             j, i->op[j].imms->X_add_number);
      printf("  i->op[%u].imms->X_op = %u;\n", j, i->op[j].imms->X_op);
      printf("  i->op[%u].imms->X_unsigned = %u;\n",
             j, i->op[j].imms->X_unsigned);
      printf("  i->op[%u].imms->X_md = %u;\n", j, i->op[j].imms->X_md);
    } else if (i->op[j].disps != NULL) {
      printf("  i->op[%u].disps = "
             "(expressionS*)malloc(sizeof(expressionS));\n", j);
      if (i->op[j].disps->X_add_symbol) {
        const char *name = S_GET_NAME(i->op[j].disps->X_add_symbol);
        printf("  i->op[%u].disps->X_add_symbol = "
               "symbol_find_or_make(\"%s\");\n", j, name);
      }
      if (i->op[j].disps->X_op_symbol) {
        const char *name = S_GET_NAME(i->op[j].disps->X_op_symbol);
        printf("  i->op[%u].disps->X_op_symbol = "
               "symbol_find_or_make(\"%s\");\n", j, name);
      }
      printf("  i->op[%u].disps->X_add_number = %ld;\n",
             j, i->op[j].disps->X_add_number);
      printf("  i->op[%u].disps->X_op = %u;\n", j, i->op[j].disps->X_op);
      printf("  i->op[%u].disps->X_unsigned = %u;\n",
             j, i->op[j].disps->X_unsigned);
      printf("  i->op[%u].disps->X_md = %u;\n", j, i->op[j].disps->X_md);
    }
  }
}

// Similar to PrintOperandTypes, except that the bitfields are in a different
// structure.
void PrintOperandTypes2(const i386_operand_type *types, int num_operands) {
  for (int j = 0; j < num_operands; j++) {
    printf("\n  j = %d;\n", j);
    PrintNonZeroArrow(i, types[j], bitfield.reg8, %d);
    PrintNonZeroArrow(i, types[j], bitfield.reg16, %d);
    PrintNonZeroArrow(i, types[j], bitfield.reg32, %d);
    PrintNonZeroArrow(i, types[j], bitfield.reg64, %d);
    PrintNonZeroArrow(i, types[j], bitfield.floatreg, %d);
    PrintNonZeroArrow(i, types[j], bitfield.regmmx, %d);
    PrintNonZeroArrow(i, types[j], bitfield.regxmm, %d);
    PrintNonZeroArrow(i, types[j], bitfield.regymm, %d);
    PrintNonZeroArrow(i, types[j], bitfield.control, %d);
    PrintNonZeroArrow(i, types[j], bitfield.debug, %d);
    PrintNonZeroArrow(i, types[j], bitfield.test, %d);
    PrintNonZeroArrow(i, types[j], bitfield.sreg2, %d);
    PrintNonZeroArrow(i, types[j], bitfield.sreg3, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm1, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm8, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm8s, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm16, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm32, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm32, %d);
    PrintNonZeroArrow(i, types[j], bitfield.imm64, %d);
    PrintNonZeroArrow(i, types[j], bitfield.disp8, %d);
    PrintNonZeroArrow(i, types[j], bitfield.disp16, %d);
    PrintNonZeroArrow(i, types[j], bitfield.disp32, %d);
    PrintNonZeroArrow(i, types[j], bitfield.disp32s, %d);
    PrintNonZeroArrow(i, types[j], bitfield.disp64, %d);
    PrintNonZeroArrow(i, types[j], bitfield.acc, %d);
    PrintNonZeroArrow(i, types[j], bitfield.floatacc, %d);
    PrintNonZeroArrow(i, types[j], bitfield.baseindex, %d);
    PrintNonZeroArrow(i, types[j], bitfield.inoutportreg, %d);
    PrintNonZeroArrow(i, types[j], bitfield.shiftcount, %d);
    PrintNonZeroArrow(i, types[j], bitfield.jumpabsolute, %d);
    PrintNonZeroArrow(i, types[j], bitfield.esseg, %d);
    PrintNonZeroArrow(i, types[j], bitfield.regmem, %d);
    PrintNonZeroArrow(i, types[j], bitfield.mem, %d);
    PrintNonZeroArrow(i, types[j], bitfield.byte, %d);
    PrintNonZeroArrow(i, types[j], bitfield.word, %d);
    PrintNonZeroArrow(i, types[j], bitfield.dword, %d);
    PrintNonZeroArrow(i, types[j], bitfield.fword, %d);
    PrintNonZeroArrow(i, types[j], bitfield.qword, %d);
    PrintNonZeroArrow(i, types[j], bitfield.tbyte, %d);
    PrintNonZeroArrow(i, types[j], bitfield.xmmword, %d);
    PrintNonZeroArrow(i, types[j], bitfield.ymmword, %d);
    PrintNonZeroArrow(i, types[j], bitfield.unspecified, %d);
    PrintNonZeroArrow(i, types[j], bitfield.anysize, %d);
    PrintNonZeroArrow(i, types[j], bitfield.vex_imm4, %d);
  }
}
void PrintOpcodeModifier(i386_opcode_modifier opcode_modifier) {
  PrintNonZeroDot(i->tm, opcode_modifier, d, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, w, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, modrm, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, shortform, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, jump, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, jumpdword, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, jumpbyte, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, jumpintersegment, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, floatmf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, floatr, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, floatd, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, size16, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, size32, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, size64, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, ignoresize, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, defaultsize, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, no_bsuf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, no_wsuf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, no_lsuf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, no_ssuf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, no_qsuf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, no_ldsuf, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, fwait, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, isstring, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, regkludge, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, firstxmm0, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, implicit1stxmm0, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, byteokintel, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, todword, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, toqword, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, addrprefixop0, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, isprefix, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, immext, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, norex64, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, rex64, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, ugh, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, drex, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, drexv, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, drexc, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vex, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vex256, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vexnds, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vexndd, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vexw0, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vexw1, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vex0f, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vex0f38, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vex0f3a, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, vex3sources, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, veximmext, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, sse2avx, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, noavx, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, oldgcc, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, attmnemonic, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, attsyntax, %d);
  PrintNonZeroDot(i->tm, opcode_modifier, intelsyntax, %d);
}
}  // namespace
