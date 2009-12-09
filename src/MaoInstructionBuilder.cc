// Copyright 2009 Google Inc. All Rights Reserved.
// Author: eraman@google.com (Easwaran Raman)

extern "C" {
  #include "as.h"
  #include "tc-i386.h"
}

void PrintTemplate (insn_template *tm);
void PrintOperandTypes (i386_operand_type *operand_types);
void PrintOpcodeModifier (i386_opcode_modifier *modifier);
void PrintOperandTypes2 (i386_operand_type *types);

void print_i386_insn_struct (i386_insn *instruction) {
  PrintTemplate (&(instruction->tm));
  printf ("i->suffix = %d;\n", instruction->suffix);
  printf ("i->operands= %d;\n", instruction->operands);
  printf ("i->reg_operands= %d;\n", instruction->reg_operands);
  printf ("i->disp_operands= %d;\n", instruction->disp_operands);
  printf ("i->mem_operands= %d;\n", instruction->mem_operands);
  printf ("i->imm_operands= %d;\n", instruction->imm_operands);
  PrintOperandTypes2(instruction->types);
}

#define PrintField(fmt_str, value) \
    printf (fmt_str, value);

#define PrintNonZeroField(fmt_str, value) \
    printf (fmt_str, value);

#define Print3(x, y)\
    printf(#x, y);


#define PrintNonZero(prefix, structure, field, format_str) \
    if (structure->field) \
    Print3(prefix.structure.field = format_str;\n, structure->field);

#define PrintNonZero2(prefix, structure, field, format_str) \
    if (structure.field) \
    Print3(prefix.structure.field = format_str;\n, structure.field);

#define PrintNonZero1(prefix, structure, field, format_str) \
    if (structure.field) \
    Print3(prefix->structure.field = format_str;\n, structure.field);

#define PrintField1(prefix, structure, field, format_str) \
    Print3(prefix.structure.field = format_str;\n, structure->field);

#define PrintField2(prefix, structure, field, format_str) \
    Print3(prefix->structure.field = format_str;\n, structure->field);
void PrintTemplate (insn_template *tm) {
#ifdef PREFIX
#undef PREFIX
#endif
#define PREFIX i->tm

  PrintField2(i, tm, name, strdup ("%s"))
  PrintField2(i, tm, operands, %d)
  PrintField2(i, tm, base_opcode, %d)
  PrintField2(i, tm, extension_opcode, %d)
  PrintField2(i, tm, opcode_length, %d)
  /* Omitting CPU flags for now. Seems to be all zeroes */

  PrintOpcodeModifier(&(tm->opcode_modifier));
  PrintOperandTypes(tm->operand_types);


}

void PrintOperandTypes (i386_operand_type *operand_types) {
  printf ("int i;");
  for (int i=0; i<MAX_OPERANDS; i++) {
    printf ("i = %d;\n", i);
    PrintNonZero2(i->tm, operand_types[i], bitfield.reg8, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.reg16, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.reg32, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.reg64, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.floatreg, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.regmmx, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.regxmm, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.regymm, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.control, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.debug, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.test, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.sreg2, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.sreg3, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm1, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm8, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm8s, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm16, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm32, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm32, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.imm64, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.disp8, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.disp16, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.disp32, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.disp32s, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.disp64, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.acc, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.floatacc, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.baseindex, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.inoutportreg, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.shiftcount, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.jumpabsolute, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.esseg, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.regmem, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.mem, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.byte, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.word, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.dword, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.fword, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.qword, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.tbyte, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.xmmword, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.ymmword, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.unspecified, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.anysize, %d)
    PrintNonZero2(i->tm, operand_types[i], bitfield.vex_imm4, %d)
  }
}

void PrintOperandTypes2 (i386_operand_type *types) {
  printf ("int i;");
  for (int i=0; i<MAX_OPERANDS; i++) {
    printf ("i = %d;\n", i);
    PrintNonZero1(i, types[i], bitfield.reg8, %d)
    PrintNonZero1(i, types[i], bitfield.reg16, %d)
    PrintNonZero1(i, types[i], bitfield.reg32, %d)
    PrintNonZero1(i, types[i], bitfield.reg64, %d)
    PrintNonZero1(i, types[i], bitfield.floatreg, %d)
    PrintNonZero1(i, types[i], bitfield.regmmx, %d)
    PrintNonZero1(i, types[i], bitfield.regxmm, %d)
    PrintNonZero1(i, types[i], bitfield.regymm, %d)
    PrintNonZero1(i, types[i], bitfield.control, %d)
    PrintNonZero1(i, types[i], bitfield.debug, %d)
    PrintNonZero1(i, types[i], bitfield.test, %d)
    PrintNonZero1(i, types[i], bitfield.sreg2, %d)
    PrintNonZero1(i, types[i], bitfield.sreg3, %d)
    PrintNonZero1(i, types[i], bitfield.imm1, %d)
    PrintNonZero1(i, types[i], bitfield.imm8, %d)
    PrintNonZero1(i, types[i], bitfield.imm8s, %d)
    PrintNonZero1(i, types[i], bitfield.imm16, %d)
    PrintNonZero1(i, types[i], bitfield.imm32, %d)
    PrintNonZero1(i, types[i], bitfield.imm32, %d)
    PrintNonZero1(i, types[i], bitfield.imm64, %d)
    PrintNonZero1(i, types[i], bitfield.disp8, %d)
    PrintNonZero1(i, types[i], bitfield.disp16, %d)
    PrintNonZero1(i, types[i], bitfield.disp32, %d)
    PrintNonZero1(i, types[i], bitfield.disp32s, %d)
    PrintNonZero1(i, types[i], bitfield.disp64, %d)
    PrintNonZero1(i, types[i], bitfield.acc, %d)
    PrintNonZero1(i, types[i], bitfield.floatacc, %d)
    PrintNonZero1(i, types[i], bitfield.baseindex, %d)
    PrintNonZero1(i, types[i], bitfield.inoutportreg, %d)
    PrintNonZero1(i, types[i], bitfield.shiftcount, %d)
    PrintNonZero1(i, types[i], bitfield.jumpabsolute, %d)
    PrintNonZero1(i, types[i], bitfield.esseg, %d)
    PrintNonZero1(i, types[i], bitfield.regmem, %d)
    PrintNonZero1(i, types[i], bitfield.mem, %d)
    PrintNonZero1(i, types[i], bitfield.byte, %d)
    PrintNonZero1(i, types[i], bitfield.word, %d)
    PrintNonZero1(i, types[i], bitfield.dword, %d)
    PrintNonZero1(i, types[i], bitfield.fword, %d)
    PrintNonZero1(i, types[i], bitfield.qword, %d)
    PrintNonZero1(i, types[i], bitfield.tbyte, %d)
    PrintNonZero1(i, types[i], bitfield.xmmword, %d)
    PrintNonZero1(i, types[i], bitfield.ymmword, %d)
    PrintNonZero1(i, types[i], bitfield.unspecified, %d)
    PrintNonZero1(i, types[i], bitfield.anysize, %d)
    PrintNonZero1(i, types[i], bitfield.vex_imm4, %d)
  }
}
void PrintOpcodeModifier (i386_opcode_modifier *modifier) {
  PrintNonZero(i->tm, modifier, d, %d)
  PrintNonZero(i->tm, modifier, w, %d)
  PrintNonZero(i->tm, modifier, modrm, %d)
  PrintNonZero(i->tm, modifier, shortform, %d)
  PrintNonZero(i->tm, modifier, jump, %d)
  PrintNonZero(i->tm, modifier, jumpdword, %d)
  PrintNonZero(i->tm, modifier, jumpbyte, %d)
  PrintNonZero(i->tm, modifier, jumpintersegment, %d)
  PrintNonZero(i->tm, modifier, floatmf, %d)
  PrintNonZero(i->tm, modifier, floatr, %d)
  PrintNonZero(i->tm, modifier, floatd, %d)
  PrintNonZero(i->tm, modifier, size16, %d)
  PrintNonZero(i->tm, modifier, size32, %d)
  PrintNonZero(i->tm, modifier, size64, %d)
  PrintNonZero(i->tm, modifier, ignoresize, %d)
  PrintNonZero(i->tm, modifier, defaultsize, %d)
  PrintNonZero(i->tm, modifier, no_bsuf, %d)
  PrintNonZero(i->tm, modifier, no_wsuf, %d)
  PrintNonZero(i->tm, modifier, no_lsuf, %d)
  PrintNonZero(i->tm, modifier, no_ssuf, %d)
  PrintNonZero(i->tm, modifier, no_qsuf, %d)
  PrintNonZero(i->tm, modifier, no_ldsuf, %d)
  PrintNonZero(i->tm, modifier, fwait, %d)
  PrintNonZero(i->tm, modifier, isstring, %d)
  PrintNonZero(i->tm, modifier, regkludge, %d)
  PrintNonZero(i->tm, modifier, firstxmm0, %d)
  PrintNonZero(i->tm, modifier, implicit1stxmm0, %d)
  PrintNonZero(i->tm, modifier, byteokintel, %d)
  PrintNonZero(i->tm, modifier, todword, %d)
  PrintNonZero(i->tm, modifier, toqword, %d)
  PrintNonZero(i->tm, modifier, addrprefixop0, %d)
  PrintNonZero(i->tm, modifier, isprefix, %d)
  PrintNonZero(i->tm, modifier, immext, %d)
  PrintNonZero(i->tm, modifier, norex64, %d)
  PrintNonZero(i->tm, modifier, rex64, %d)
  PrintNonZero(i->tm, modifier, ugh, %d)
  PrintNonZero(i->tm, modifier, drex, %d)
  PrintNonZero(i->tm, modifier, drexv, %d)
  PrintNonZero(i->tm, modifier, drexc, %d)
  PrintNonZero(i->tm, modifier, vex, %d)
  PrintNonZero(i->tm, modifier, vex256, %d)
  PrintNonZero(i->tm, modifier, vexnds, %d)
  PrintNonZero(i->tm, modifier, vexndd, %d)
  PrintNonZero(i->tm, modifier, vexw0, %d)
  PrintNonZero(i->tm, modifier, vexw1, %d)
  PrintNonZero(i->tm, modifier, vex0f, %d)
  PrintNonZero(i->tm, modifier, vex0f38, %d)
  PrintNonZero(i->tm, modifier, vex0f3a, %d)
  PrintNonZero(i->tm, modifier, vex3sources, %d)
  PrintNonZero(i->tm, modifier, veximmext, %d)
  PrintNonZero(i->tm, modifier, sse2avx, %d)
  PrintNonZero(i->tm, modifier, noavx, %d)
  PrintNonZero(i->tm, modifier, oldgcc, %d)
  PrintNonZero(i->tm, modifier, attmnemonic, %d)
  PrintNonZero(i->tm, modifier, attsyntax, %d)
  PrintNonZero(i->tm, modifier, intelsyntax, %d)

}
