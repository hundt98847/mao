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
#include <list>
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
  GenDefEntry() {}
  unsigned int       op_mask;
  BitString reg_mask;
  BitString reg_mask8;
  BitString reg_mask16;
  BitString reg_mask32;
  BitString reg_mask64;
};

typedef std::map<char *, GenDefEntry *, ltstr> MnemMap;
static MnemMap mnem_map;

class RegEntry {
 public:
  RegEntry(char *name, int num) :
    name_(strdup(name)), num_(num) {}
  char *name_;
  int   num_;
};

typedef std::list<RegEntry *> RegList;
static RegList reg_list;

void ReadRegisterTable(const char *op_table) {
  char buff[1024];
  strcpy(buff, op_table);
  char *p = strrchr(buff, '/');
  strcpy(p+1, "i386-reg.tbl");

  FILE *regs = fopen(buff, "r");
  if (!regs) {
    fprintf(stderr, "Cannot open register table file: %s\n", buff);
    exit(1);
  }

  int count = 0;
  while (!feof(regs)) {
    char *p = fgets(buff, 1024, regs);
    if (!p) break;

    if (buff[0] == '/' && buff[1] == '/') continue;
    if (buff[0] == '#' || buff[0] == '\n') continue;
    char *reg = next_field(buff, ',', &p);

    RegEntry *r = new RegEntry(reg, count++);
    reg_list.push_back(r);
  }

  fclose(regs);
}

// linear search through list to find a register by name
//
RegEntry *FindRegister(const char *str) {
  for (RegList::iterator it = reg_list.begin();
       it != reg_list.end(); ++it) {
    if (!strcasecmp((*it)->name_, str))
      return *it;
  }
  return NULL;
}


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

    BitString *mask = &e->reg_mask;

    while (p && *p && (p < end)) {
      char *q = next_field(p, ' ', &p);
      if (!*q) break;
      if (!strcmp(q, "all:"))    mask = &e->reg_mask; else
      if (!strcmp(q, "addr8:"))  mask = &e->reg_mask8; else
      if (!strcmp(q, "addr16:")) mask = &e->reg_mask16; else
      if (!strcmp(q, "addr32:")) mask = &e->reg_mask32; else
      if (!strcmp(q, "addr64:")) mask = &e->reg_mask64; else

      if (!strcmp(q, "op0"))  e->op_mask |= DEF_OP0; else
      if (!strcmp(q, "src"))  e->op_mask |= DEF_OP0; else

      if (!strcmp(q, "op1"))  e->op_mask |= DEF_OP1; else
      if (!strcmp(q, "dest")) e->op_mask |= DEF_OP1; else

      if (!strcmp(q, "op2"))  e->op_mask |= DEF_OP2; else
      if (!strcmp(q, "op3"))  e->op_mask |= DEF_OP3; else
      if (!strcmp(q, "op4"))  e->op_mask |= DEF_OP4; else
      if (!strcmp(q, "op5"))  e->op_mask |= DEF_OP5; else {
        RegEntry *r = FindRegister(q);
        if (r)
          mask->Set(r->num_);
        else {
          fprintf(stderr, "Unknown string: %s <%s>\n", q, buff);
          exit(1);
        }
      }
      if (p > end) break;
    }
  }

  fclose(f);
}

static void PrintRegMask(FILE *def, BitString &mask) {
  mask.PrintInitializer(def);
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

  ReadRegisterTable(argv[1]);
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
          "#define BNULL BitString(0x0ull, 0x0ull, 0x0ull, 0x0ull)\n"
          "#define BALL  BitString(-1ull, -1ull, -1ull, -1ull)\n"
          "DefEntry def_entries [] = {\n"
          "  { OP_invalid, 0, BNULL, BNULL, BNULL, BNULL, BNULL },\n" );

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
        fprintf(def, "  { OP_%s, DEF_OP_ALL, BALL, BALL, BALL, BALL, BALL },\n", sanitized_name);
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
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask8);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask16);
        fprintf(def, ", ");
        PrintRegMask(def, e->reg_mask32);
        fprintf(def, ", ");
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

  fprintf(def, "};\n"
          "const unsigned int def_entries_size = "
          "sizeof(def_entries) / sizeof(DefEntry);\n"
          );

  fclose(in);
  fclose(out);
  fclose(table);
  fclose(def);

  return 0;
}
