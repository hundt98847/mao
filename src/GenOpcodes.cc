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
#include <unistd.h>

#include <map>
#include <list>
#include <algorithm>

#include "opcodes/i386-opc.h"
#include "MaoDefs.h"

static bool emit_warnings = false;

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

void usage(char *const argv[]) __attribute__ ((noreturn));

void usage(char *const argv[]) {
  fprintf(stderr,
          "USAGE:\n "
          " %s [-p outputpath] optable-file regtable-file sideeffect-file \n\n",
          argv[0]);
  fprintf(stderr,
          "Creates headerfiles in in directory outputpath, "
          "defaults to current path\n");
  exit(1);
}

class GenDefUseEntry {
 public:
  GenDefUseEntry(char *op_str) :
    op_str_(op_str) {
    flags.CF = flags.PF = flags.AF = flags.ZF = flags.SF =
    flags.TP = flags.IF = flags.DF = flags.OF = flags.IOPL =
    flags.NT = flags.RF = flags.VM = flags.AC = flags.VIF =
    flags.VIP = flags.ID = false;
    found = false;
  }
  char *op_str() { return op_str_; }
  unsigned int       op_mask;
  BitString reg_mask;
  BitString reg_mask8;
  BitString reg_mask16;
  BitString reg_mask32;
  BitString reg_mask64;
  struct flags_ {
    bool  CF :1;
    bool  PF :1;
    bool  AF :1;
    bool  ZF :1;
    bool  SF :1;
    bool  TP :1;
    bool  IF :1;
    bool  DF :1;
    bool  OF :1;
    bool  IOPL :1;
    bool  NT :1;
    bool  RF :1;
    bool  VM :1;
    bool  AC :1;
    bool  VIF:1;
    bool  VIP :1;
    bool  ID :1;
  } flags;
  bool found :1;
  char *op_str_;
};

typedef std::map<char *, GenDefUseEntry *, ltstr> MnemMap;
static MnemMap mnem_def_map, mnem_use_map;

class RegEntry {
 public:
  RegEntry(char *name, int num) :
    name_(strdup(name)), num_(num) {}
  char *name_;
  int   num_;
};

typedef std::list<RegEntry *> RegList;
static RegList reg_list;

void ReadRegisterTable(FILE *regs) {
  char buff[1024];
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


void ReadSideEffects(const char *fname, MnemMap &mnem_map) {
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

    GenDefUseEntry *e = new GenDefUseEntry(strdup(mnem));
    mnem_map[strdup(mnem)] = e;

    BitString *mask = &e->reg_mask;

    bool eflags=false;
    while (p && *p && (p < end)) {
      char *q = next_field(p, ' ', &p);
      if (!*q) break;
      if (!strcasecmp(q, "all:"))    mask = &e->reg_mask; else
      if (!strcasecmp(q, "addr8:"))  mask = &e->reg_mask8; else
      if (!strcasecmp(q, "addr16:")) mask = &e->reg_mask16; else
      if (!strcasecmp(q, "addr32:")) mask = &e->reg_mask32; else
      if (!strcasecmp(q, "addr64:")) mask = &e->reg_mask64; else

      if (!strcasecmp(q, "flags:"))  /* TODO(rhundt): refine */; else
      if (!strcasecmp(q, "clear:"))  /* TODO(rhundt): refine */; else
      if (!strcasecmp(q, "undef:"))  /* TODO(rhundt): refine */; else

      if (!strcasecmp(q, "CF")) eflags = true; else
      if (!strcasecmp(q, "PF")) eflags = true; else
      if (!strcasecmp(q, "AF")) eflags = true; else
      if (!strcasecmp(q, "ZF")) eflags = true; else
      if (!strcasecmp(q, "SF")) eflags = true; else
      if (!strcasecmp(q, "TP")) eflags = true; else
      if (!strcasecmp(q, "IF")) eflags = true; else
      if (!strcasecmp(q, "DF")) eflags = true; else
      if (!strcasecmp(q, "OF")) eflags = true; else
      if (!strcasecmp(q, "IOPL")) eflags = true; else
      if (!strcasecmp(q, "NT")) eflags = true; else
      if (!strcasecmp(q, "RF")) eflags = true; else
      if (!strcasecmp(q, "VM")) eflags = true; else
      if (!strcasecmp(q, "AC")) eflags = true; else
      if (!strcasecmp(q, "VIF")) eflags = true; else
      if (!strcasecmp(q, "VIP")) eflags = true; else
      if (!strcasecmp(q, "ID")) eflags = true; else 
      /*
      if (!strcasecmp(q, "CF")) e->flags.CF = true; else
      if (!strcasecmp(q, "PF")) e->flags.PF = true; else
      if (!strcasecmp(q, "AF")) e->flags.AF = true; else
      if (!strcasecmp(q, "ZF")) e->flags.ZF = true; else
      if (!strcasecmp(q, "SF")) e->flags.SF = true; else
      if (!strcasecmp(q, "TP")) e->flags.TP = true; else
      if (!strcasecmp(q, "IF")) e->flags.IF = true; else
      if (!strcasecmp(q, "DF")) e->flags.DF = true; else
      if (!strcasecmp(q, "OF")) e->flags.OF = true; else
      if (!strcasecmp(q, "IOPL")) e->flags.IOPL = true; else
      if (!strcasecmp(q, "NT")) e->flags.NT = true; else
      if (!strcasecmp(q, "RF")) e->flags.RF = true; else
      if (!strcasecmp(q, "VM")) e->flags.VM = true; else
      if (!strcasecmp(q, "AC")) e->flags.AC = true; else
      if (!strcasecmp(q, "VIF")) e->flags.VIF = true; else
      if (!strcasecmp(q, "VIP")) e->flags.VIP = true; else
      if (!strcasecmp(q, "ID")) e->flags.ID = true; else */

      if (!strcasecmp(q, "op0"))  e->op_mask |= REG_OP0; else
      if (!strcasecmp(q, "src"))  e->op_mask |= REG_OP0; else

      if (!strcasecmp(q, "op1"))  e->op_mask |= REG_OP1; else
      if (!strcasecmp(q, "dest")) e->op_mask |= REG_OP1; else

      if (!strcasecmp(q, "op2"))  e->op_mask |= REG_OP2; else
      if (!strcasecmp(q, "op3"))  e->op_mask |= REG_OP3; else
      if (!strcasecmp(q, "op4"))  e->op_mask |= REG_OP4; else
      if (!strcasecmp(q, "op5"))  e->op_mask |= REG_OP5; else
      if (!strcasecmp(q, "exp"))  e->op_mask |= USE_OP_ALL; else {
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
    if (eflags)
      e->reg_mask.Set(FindRegister("eflags")->num_);


  }

  fclose(f);
}

static void PrintRegMask(FILE *def, BitString &mask) {
  mask.PrintInitializer(def);
}

int fail_on_open(char *const argv[], const char *filename)
    __attribute__ ((noreturn));

int fail_on_open(char *const argv[], const char *filename) {
  fprintf(stderr, "Cannot open output file: %s\n", filename);
  usage(argv);
  exit(1);
}

int main(int argc, char *const argv[]) {
  char buf[2048];
  int  lineno = 0;
  char lastname[2048];
  char sanitized_name[2048];

  // Options processing
  //
  // Mandatory arguments are:
  //  - optable-file
  //  - regtable-file
  //  - sideeffect-file
  if (argc < 4)
    usage(argv);

  const char *out_path = NULL; // Default to current directory.
  opterr = 0;

  int c;
  while ((c = getopt (argc, argv, "p:w")) != -1) {
    switch (c) {
      case 'w':
        emit_warnings = true;
        break;
      case 'p':
        out_path = optarg;
        break;
      case '?':
        if (optopt == 'p')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        return 1;
      default:
        abort ();
    }
  }

  // there shoule be four more arguments
  if ((argc - optind) != 4)
    usage(argv);


  const char *op_table = argv[optind];
  const char *reg_table = argv[optind+1];
  const char *side_effect_table = argv[optind+2];

  // Open the various input and output files,
  // emit top portion in generated files.
  //
  FILE *op = fopen(op_table, "r");
  if (!op) {
    fprintf(stderr, "Cannot open table file: %s\n", op_table);
    usage(argv);
  }

  FILE *reg = fopen(reg_table, "r");
  if (!reg) {
    fprintf(stderr, "Cannot open register file: %s\n", reg_table);
    usage(argv);
  }

  ReadRegisterTable(reg);
  ReadSideEffects(side_effect_table, mnem_def_map);

  side_effect_table = argv[optind+3];
  ReadSideEffects(side_effect_table, mnem_use_map);

  const char *out_filename;
  const char *table_filename;
  const char *defs_filename;
  const char *uses_filename;

  if (out_path != NULL) {
    char out_filename_buf[2048];
    char table_filename_buf[2048];
    char defs_filename_buf[2048];
    char uses_filename_buf[2048];

    sprintf(out_filename_buf, "%s%s",   out_path, "/gen-opcodes.h");
    out_filename = out_filename_buf;
    sprintf(table_filename_buf, "%s%s", out_path, "/gen-opcodes-table.h");
    table_filename = table_filename_buf;
    sprintf(defs_filename_buf, "%s%s",  out_path, "/gen-defs.h");
    defs_filename = defs_filename_buf;
    sprintf(uses_filename_buf, "%s%s",  out_path, "/gen-uses.h");
    uses_filename  = uses_filename_buf;
  } else {
    out_filename   = "gen-opcodes.h";
    table_filename = "gen-opcodes-table.h";
    defs_filename  = "gen-defs.h";
    uses_filename  = "gen-uses.h";
  }
  FILE *out, *table, *def, *use;

  (out   = fopen(out_filename,   "w")) || fail_on_open(argv, out_filename);
  (table = fopen(table_filename, "w")) || fail_on_open(argv, table_filename);
  (def   = fopen(defs_filename,  "w")) || fail_on_open(argv, defs_filename);
  (use   = fopen(uses_filename,  "w")) || fail_on_open(argv, uses_filename);

  fprintf(out,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n\n"
          "#ifndef GEN_OPCODES_H_\n"
          "#define GEN_OPCODES_H_\n"
          "enum MaoOpcode {\n"
          "  OP_invalid,\n");

  fprintf(table,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n\n"
          "#ifndef GEN_OPCODES_TABLE_MAODEFS_H_\n"
          "#define GEN_OPCODES_TABLE_MAODEFS_H_\n"
          "#include \"gen-opcodes.h\"\n\n"
          "struct MaoOpcodeTable_ {\n"
          "   MaoOpcode    opcode;\n"
          "   const char  *name;\n"
          "} MaoOpcodeTable[] = {\n"
          "  { OP_invalid, \"invalid\" },\n");

  // TODO(martint): Update 256 to use the number of actual registers.
  fprintf(def,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n"
          "#ifndef GEN_DEFS_MAODEFS_H_\n"
          "#define GEN_DEFS_MAODEFS_H_\n"
          "#define BNULL BitString(256, 4, 0x0ull, 0x0ull, 0x0ull, 0x0ull)\n"
          "#define BALL  BitString(256, 4, -1ull, -1ull, -1ull, -1ull)\n"
          "DefEntry def_entries [] = {\n"
          "  { OP_invalid, 0, BNULL, BNULL, BNULL, BNULL, BNULL },\n");

  fprintf(use,
          "// DO NOT EDIT - this file is automatically "
          "generated by GenOpcodes\n//\n"
          "#ifndef GEN_USES_MAODEFS_H_\n"
          "#define GEN_USES_MAODEFS_H_\n"
          "#define BNULL BitString(256, 4, 0x0ull, 0x0ull, 0x0ull, 0x0ull)\n"
          "#define BALL  BitString(256, 4, -1ull, -1ull, -1ull, -1ull)\n"
          "UseEntry use_entries [] = {\n"
          "  { OP_invalid, 0, BNULL, BNULL, BNULL, BNULL, BNULL },\n");
  // Read through the instruction description file, isolate the first
  // field, which contains the opcode, and generate an
  //   OP_... into the gen-opcodes.h file.
  //
  // Also, lookup side effects, and generate an entry in the side
  // effect tables in
  //    gen-defs.h
  //
  while (!feof(op)) {
    char *p, *str, *name;
    if (fgets (buf, sizeof (buf), op) == NULL)
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
      MnemMap::iterator def_it = mnem_def_map.find(sanitized_name);
      if (def_it == mnem_def_map.end()) {
        fprintf(def, "  { OP_%s, DEF_OP_ALL, BALL, BALL, BALL, BALL, BALL },\n", sanitized_name);
        if (emit_warnings)
          fprintf(stderr, "Warning: No side-effects for: %s\n", sanitized_name);
      } else {
        fprintf(def, "  { OP_%s, 0", sanitized_name);
        GenDefUseEntry *e = (*def_it).second;
        e->found = true;
        if (e->op_mask & REG_OP0) fprintf(def, " | REG_OP0");
        if (e->op_mask & REG_OP1) fprintf(def, " | REG_OP1");
        if (e->op_mask & REG_OP2) fprintf(def, " | REG_OP2");
        if (e->op_mask & REG_OP3) fprintf(def, " | REG_OP3");
        if (e->op_mask & REG_OP4) fprintf(def, " | REG_OP4");
        if (e->op_mask & REG_OP5) fprintf(def, " | REG_OP5");

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

      /* Emit use entry */
      MnemMap::iterator use_it = mnem_use_map.find(sanitized_name);
      if (use_it == mnem_use_map.end()) {
        fprintf(use, "  { OP_%s, USE_OP_ALL, BALL, BALL, BALL, BALL, BALL },\n", sanitized_name);
        if (emit_warnings)
          fprintf(stderr, "Warning: No side-effects for: %s\n", sanitized_name);
      } else {
        fprintf(use, "  { OP_%s, 0", sanitized_name);
        GenDefUseEntry *e = (*use_it).second;
        e->found = true;
        if ( ((e->op_mask | REG_OP_BASE | REG_OP_INDEX) & USE_OP_ALL) == USE_OP_ALL)
          fprintf(use, " | USE_OP_ALL");
        else {
          if (e->op_mask & REG_OP0) fprintf(use, " | REG_OP0");
          if (e->op_mask & REG_OP1) fprintf(use, " | REG_OP1");
          if (e->op_mask & REG_OP2) fprintf(use, " | REG_OP2");
          if (e->op_mask & REG_OP3) fprintf(use, " | REG_OP3");
          if (e->op_mask & REG_OP4) fprintf(use, " | REG_OP4");
          if (e->op_mask & REG_OP5) fprintf(use, " | REG_OP5");

          //Always include base and disp register in the use
          fprintf(use, " | REG_OP_BASE | REG_OP_INDEX");
        }

        // populate reg_mask
        fprintf(use, ", ");
        PrintRegMask(use, e->reg_mask);
        fprintf(use, ", ");
        PrintRegMask(use, e->reg_mask8);
        fprintf(use, ", ");
        PrintRegMask(use, e->reg_mask16);
        fprintf(use, ", ");
        PrintRegMask(use, e->reg_mask32);
        fprintf(use, ", ");
        PrintRegMask(use, e->reg_mask64);
        fprintf(use, " },\n");
      }
    }
    strcpy(lastname, name);
  }

  // quality check - see which DefEntries remain unused
  //
  if (emit_warnings)
    for (MnemMap::iterator it = mnem_def_map.begin();
         it != mnem_def_map.end(); ++it) {
      if (!(*it).second->found)
        fprintf(stderr, "Warning: Unused side-effects description: %s\n",
                (*it).second->op_str());
    }

  fprintf(out, "};  // MaoOpcode\n\n"
          "MaoOpcode GetOpcode(const char *opcode);\n"
          "#endif  // GEN_OPCODES_H_\n");

  fprintf(table, "  { OP_invalid, 0 }\n");
  fprintf(table, "};\n"
          "#endif  // GEN_OPCODES_TABLE_MAODEFS_H_\n");

  fprintf(def, "};\n"
          "const unsigned int def_entries_size = "
          "sizeof(def_entries) / sizeof(DefEntry);\n"
          "#endif  // GEN_DEFS_MAODEFS_H_\n"
          );

  fprintf(use, "};\n"
          "const unsigned int use_entries_size = "
          "sizeof(use_entries) / sizeof(UseEntry);\n"
          "#endif  // GEN_USES_MAODEFS_H_\n"
          );

  fclose(op);
  fclose(reg);
  fclose(out);
  fclose(table);
  fclose(def);
  fclose(use);

  return 0;
}
