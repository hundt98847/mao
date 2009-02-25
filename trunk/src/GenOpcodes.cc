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

// Generate enums and a hashtable for x86 instructions.
// Usage: GenOpcodes instruction-table
//
// Instruction table is something like:
//    binutils-2.19/opcodes/i386-opc.tbl
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <map>
#include <algorithm>

#include "MaoDefs.h"

struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

// Note: The following few helper routines, as
//       well as the main loop in main(), are directly
//       copied from the i386-gen.c sources in
//       binutils-2.19/opcodes/...
//

static char *
remove_leading_whitespaces(char *str) {
  while (isspace(*str))
    str++;
  return str;
}

/* Remove trailing white spaces.  */

static void
remove_trailing_whitespaces(char *str) {
  size_t last = strlen(str);

  if (last == 0)
    return;

  do {
    last--;
    if (isspace(str[last]))
      str[last] = '\0';
    else
      break;
  } while (last != 0);
}


static char *
next_field(char *str, char sep, char **next) {
  char *p;

  p = remove_leading_whitespaces(str);
  for (str = p; *str != sep && *str != '\0'; str++);

  *str = '\0';
  remove_trailing_whitespaces(p);

  *next = str + 1;

  return p;
}

void usage(const char *argv[]) {
  fprintf(stderr, "USAGE:\n"
          "  %s table-file\n\n", argv[0]);
  fprintf(stderr,
          "Produces file: gen-opcodes.h\n");
  exit(1);
}

class GenDefEntry {
 public:
  GenDefEntry() : op_mask(0), reg_mask(0),
                  reg_mask8(0), reg_mask16(0),
                  reg_mask32(0), reg_mask64(0) {}
  unsigned int       op_mask;
  unsigned long long reg_mask;
  unsigned long long reg_mask8;
  unsigned long long reg_mask16;
  unsigned long long reg_mask32;
  unsigned long long reg_mask64;
};

typedef std::map<char *, GenDefEntry *, ltstr> MnemMap;
static MnemMap mnem_map;

void ReadSideEffects(const char *fname) {
  char buff[1024];

  FILE *f = fopen(fname, "r");
  if (!f) {
    fprintf(stderr, "Cannot open side-effect table: %s\n", fname);
    exit(1);
  }
  while (!feof(f)) {
    char *p = fgets(buff, 1024, f);
    if (!p) break;
    char *end = p + strlen(p);

    if (buff[0] == '/' && buff[1] == '/') continue;
    if (buff[0] == '#' || buff[0] == '\n') continue;
    char *mnem = next_field(buff, ' ', &p);

    GenDefEntry *e = new GenDefEntry;
    mnem_map[strdup(mnem)] = e;

    unsigned long long *mask = &e->reg_mask;

    while (p && *p) {
      char *q = next_field(p, ' ', &p);
      if (!strcmp(q, "all:"))    mask = &e->reg_mask; else
      if (!strcmp(q, "addr8:"))  mask = &e->reg_mask8; else
      if (!strcmp(q, "addr16:")) mask = &e->reg_mask16; else
      if (!strcmp(q, "addr32:")) mask = &e->reg_mask32; else
      if (!strcmp(q, "addr64:")) mask = &e->reg_mask64; else

      if (!strcmp(q, "al"))  *mask |= REG_AL; else
      if (!strcmp(q, "ah"))  *mask |= REG_AH; else
      if (!strcmp(q, "ax"))  *mask |= (REG_AX  | REG_AH  | REG_AL); else
      if (!strcmp(q, "eax")) *mask |= (REG_EAX | REG_AX  | REG_AH | REG_AL); else
      if (!strcmp(q, "rax")) *mask |= (REG_RAX | REG_EAX | REG_AX | REG_AH | REG_AL); else

      if (!strcmp(q, "cl"))  *mask |= REG_CL; else
      if (!strcmp(q, "ch"))  *mask |= REG_CH; else
      if (!strcmp(q, "cx"))  *mask |= (REG_CX  | REG_CH  | REG_CL); else
      if (!strcmp(q, "ecx")) *mask |= (REG_ECX | REG_CX  | REG_CH | REG_CL); else
      if (!strcmp(q, "rcx")) *mask |= (REG_RCX | REG_ECX | REG_CX | REG_CH | REG_CL); else

      if (!strcmp(q, "dl"))  *mask |= REG_DL; else
      if (!strcmp(q, "dh"))  *mask |= REG_DH; else
      if (!strcmp(q, "dx"))  *mask |= (REG_DX  | REG_DH  | REG_DL); else
      if (!strcmp(q, "edx")) *mask |= (REG_EDX | REG_DX  | REG_DH | REG_DL); else
      if (!strcmp(q, "rdx")) *mask |= (REG_RDX | REG_EDX | REG_DX | REG_DH | REG_DL); else

      if (!strcmp(q, "bl"))  *mask |= REG_BL; else
      if (!strcmp(q, "bh"))  *mask |= REG_BH; else
      if (!strcmp(q, "bx"))  *mask |= (REG_BX  | REG_BH  | REG_BL); else
      if (!strcmp(q, "ebx")) *mask |= (REG_EBX | REG_BX  | REG_BH | REG_BL); else
      if (!strcmp(q, "rbx")) *mask |= (REG_RBX | REG_EBX | REG_BX | REG_BH | REG_BL); else

      if (!strcmp(q, "sp"))  *mask |= REG_SP; else
      if (!strcmp(q, "esp"))  *mask |= REG_ESP; else
      if (!strcmp(q, "rsp"))  *mask |= REG_RSP; else

      if (!strcmp(q, "bp"))  *mask |= REG_BP; else
      if (!strcmp(q, "ebp"))  *mask |= REG_EBP; else
      if (!strcmp(q, "rbp"))  *mask |= REG_RBP; else

      if (!strcmp(q, "si"))  *mask |= REG_SI; else
      if (!strcmp(q, "esi"))  *mask |= REG_ESI; else
      if (!strcmp(q, "rsi"))  *mask |= REG_RSI; else

      if (!strcmp(q, "di"))  *mask |= REG_DI; else
      if (!strcmp(q, "edi"))  *mask |= REG_EDI; else
      if (!strcmp(q, "rdi"))  *mask |= REG_RDI; else

      if (!strcmp(q, "r8"))  *mask |= REG_R8; else
      if (!strcmp(q, "r9"))  *mask |= REG_R9; else
      if (!strcmp(q, "r10"))  *mask |= REG_R10; else
      if (!strcmp(q, "r11"))  *mask |= REG_R11; else
      if (!strcmp(q, "r12"))  *mask |= REG_R12; else
      if (!strcmp(q, "r13"))  *mask |= REG_R13; else
      if (!strcmp(q, "r14"))  *mask |= REG_R14; else
      if (!strcmp(q, "r15"))  *mask |= REG_R15; else

      if (!strcmp(q, "op0"))  e->op_mask |= DEF_OP0; else
      if (!strcmp(q, "src"))  e->op_mask |= DEF_OP0; else
      if (!strcmp(q, "op1"))  e->op_mask |= DEF_OP1; else
      if (!strcmp(q, "dest")) e->op_mask |= DEF_OP1; else
      if (!strcmp(q, "op2"))  e->op_mask |= DEF_OP2; else
      if (!strcmp(q, "op3"))  e->op_mask |= DEF_OP3; else
      if (!strcmp(q, "op4"))  e->op_mask |= DEF_OP4; else
      if (!strcmp(q, "op5"))  e->op_mask |= DEF_OP5;

      if (p > end) break;
    }
  }

  fclose(f);
}

static void PrintRegMask(FILE *def, unsigned long long mask) {
  fprintf(def, ", 0");

  if (mask & REG_AL)  fprintf(def, " | REG_AL");
  if (mask & REG_AH)  fprintf(def, " | REG_AH");
  if (mask & REG_AX)  fprintf(def, " | REG_AX");
  if (mask & REG_EAX) fprintf(def, " | REG_EAX");
  if (mask & REG_RAX) fprintf(def, " | REG_RAX");

  if (mask & REG_CL)  fprintf(def, " | REG_CL");
  if (mask & REG_CH)  fprintf(def, " | REG_CH");
  if (mask & REG_CX)  fprintf(def, " | REG_CX");
  if (mask & REG_ECX) fprintf(def, " | REG_ECX");
  if (mask & REG_RCX) fprintf(def, " | REG_RCX");

  if (mask & REG_DL)  fprintf(def, " | REG_DL");
  if (mask & REG_DH)  fprintf(def, " | REG_DH");
  if (mask & REG_DX)  fprintf(def, " | REG_DX");
  if (mask & REG_EDX) fprintf(def, " | REG_EDX");
  if (mask & REG_RDX) fprintf(def, " | REG_RDX");

  if (mask & REG_BL)  fprintf(def, " | REG_BL");
  if (mask & REG_BH)  fprintf(def, " | REG_BH");
  if (mask & REG_BX)  fprintf(def, " | REG_BX");
  if (mask & REG_EBX) fprintf(def, " | REG_EBX");
  if (mask & REG_RBX) fprintf(def, " | REG_RBX");

  if (mask & REG_SP)  fprintf(def, " | REG_SP");
  if (mask & REG_ESP) fprintf(def, " | REG_ESP");
  if (mask & REG_RSP) fprintf(def, " | REG_RSP");

  if (mask & REG_BP)  fprintf(def, " | REG_BP");
  if (mask & REG_EBP) fprintf(def, " | REG_EBP");
  if (mask & REG_RBP) fprintf(def, " | REG_RBP");

  if (mask & REG_SI)  fprintf(def, " | REG_SI");
  if (mask & REG_ESI) fprintf(def, " | REG_ESI");
  if (mask & REG_RSI) fprintf(def, " | REG_RSI");

  if (mask & REG_DI)  fprintf(def, " | REG_DI");
  if (mask & REG_EDI) fprintf(def, " | REG_EDI");
  if (mask & REG_RDI) fprintf(def, " | REG_RDI");

  if (mask & REG_R8)  fprintf(def, " | REG_R8");
  if (mask & REG_R9)  fprintf(def, " | REG_R9");
  if (mask & REG_R10)  fprintf(def, " | REG_R10");
  if (mask & REG_R11)  fprintf(def, " | REG_R11");
  if (mask & REG_R12)  fprintf(def, " | REG_R12");
  if (mask & REG_R13)  fprintf(def, " | REG_R13");
  if (mask & REG_R14)  fprintf(def, " | REG_R14");
  if (mask & REG_R15)  fprintf(def, " | REG_R15");
}

int main(int argc, const char*argv[]) {
  char buf[2048];
  int  lineno = 0;
  char lastname[2048];
  char sanitized_name[2048];

  if (argc < 3)
    usage(argv);

  FILE *in = fopen(argv[1], "r");
  if (!in) {
    fprintf(stderr, "Cannot open table file: %s\n", argv[1]);
    usage(argv);
  }

  FILE *out = fopen("gen-opcodes.h", "w");
  if (!out) {
    fprintf(stderr, "Cannot open output file: gen-opcodes.h\n");
    usage(argv);
  }

  FILE *table = fopen("gen-opcodes-table.h", "w");
  if (!table) {
    fprintf(stderr, "Cannot open table file: gen-opcodes-table.h\n");
    usage(argv);
  }

  FILE *def = fopen("gen-defs.h", "w");
  if (!def) {
    fprintf(stderr, "Cannot open table file: gen-defs.c\n");
    usage(argv);
  }

  ReadSideEffects(argv[2]);

  fprintf(out,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n\n"
          "typedef enum MaoOpcode {\n"
          "  OP_invalid,\n");

  fprintf(table,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n\n"
          "#include \"gen-opcodes.h\"\n\n"
          "struct MaoOpcodeTable_ {\n"
          "   MaoOpcode    opcode;\n"
          "   const char  *name;\n"
          "} MaoOpcodeTable[] = {\n"
          "  { OP_invalid, \"invalid\" },\n");

  fprintf(def,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n"
          "DefEntry def_entries [] = {\n"
          "  { OP_invalid, 0, 0 },\n" );

  while (!feof(in)) {
    char *p, *str, *last, *name;
    if (fgets (buf, sizeof (buf), in) == NULL)
      break;

    lineno++;

    p = remove_leading_whitespaces(buf);

    /* Skip comments.  */
    str = strstr(p, "//");
    if (str != NULL)
      str[0] = '\0';

    /* Remove trailing white spaces.  */
    remove_trailing_whitespaces(p);

    switch (p[0]) {
      case '#':
        fprintf(out, "%s\n", p);
      case '\0':
        continue;
        break;
      default:
        break;
    }

    last = p + strlen(p);

    /* Find name.  */
    name = next_field(p, ',', &str);

    /* sanitize name */
    int len = strlen(name);
    for (int i = 0; i < len+1; i++)
      if (name[i] == '.' || name[i] == '-')
        sanitized_name[i] = '_';
      else
        sanitized_name[i] = name[i];


    /* compare and emit */
    if (strcmp(name, lastname)) {
      fprintf(out, "  OP_%s,\n", sanitized_name);
      fprintf(table, "  { OP_%s, \t\"%s\" },\n", sanitized_name, name);

      /* Emit def entry */
      MnemMap::iterator it = mnem_map.find(sanitized_name);
      if (it == mnem_map.end()) {
        fprintf(def, "  { OP_%s, DEF_OP_ALL, REG_ALL },\n", sanitized_name);
      } else {
        fprintf(def, "  { OP_%s, 0", sanitized_name);
        GenDefEntry *e = (*it).second;
        if (e->op_mask & DEF_OP0) fprintf(def, " | DEF_OP0");
        if (e->op_mask & DEF_OP1) fprintf(def, " | DEF_OP1");
        if (e->op_mask & DEF_OP2) fprintf(def, " | DEF_OP2");
        if (e->op_mask & DEF_OP3) fprintf(def, " | DEF_OP3");
        if (e->op_mask & DEF_OP4) fprintf(def, " | DEF_OP4");
        if (e->op_mask & DEF_OP5) fprintf(def, " | DEF_OP5");

        // populate reg_mask
        PrintRegMask(def, e->reg_mask);
        PrintRegMask(def, e->reg_mask8);
        PrintRegMask(def, e->reg_mask16);
        PrintRegMask(def, e->reg_mask32);
        PrintRegMask(def, e->reg_mask64);
        fprintf(def, " },\n");
      }
    }
    strcpy(lastname, name);
  }

  fprintf(out, "};  // MaoOpcode\n\n"
          "MaoOpcode GetOpcode(const char *opcode);\n");

  fprintf(table, "  { OP_invalid, 0 }\n");
  fprintf(table, "};\n");

  fprintf(def, "};\n");

  fclose(in);
  fclose(out);
  fclose(table);
  fclose(def);

  return 0;
}
