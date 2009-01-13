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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef IRLINK_H_
#define IRLINK_H_

#define MAX_OPERANDS_STRING_LENGTH 1024
#define MAX_VERBATIM_ASSEMBLY_STRING_LENGTH 1024
#define MAX_SYMBOL_NAME_LENGTH 1024
#define MAX_SEGMENT_NAME_LENGTH 1024
#define MAX_DIRECTIVE_NAME_LENGTH 1024
#define MAX_REGISTER_NAME_LENGTH 16

#ifdef __cplusplus
extern "C" {
#endif
  struct symbol;
  struct _i386_insn;
  enum SymbolVisibility {
    LOCAL,
    GLOBAL,
    WEAK
  };
  enum SymbolType {
    OBJECT_SYMBOL = 0,
    FUNCTION_SYMBOL,
    NOTYPE_SYMBOL,
    FILE_SYMBOL,
    SECTION_SYMBOL
  };

  // Link back an instruction from gas
  void link_insn(struct _i386_insn *i, size_t SizeOfInsn,
                 const char *line_verbatim);
  // Link back a label from gas
  void link_label(const char *name, const char *line_verbatim);
  // Link back directives (e.g. .file, .data, .size) from gas
  void link_directive(const char *key, const char *value,
                      const char *line_verbatim);
  // Link back debug info (e.g. dwarf information) from gas.
  void link_debug(const char *key, const char *value,
                  const char *line_verbatim);
  // Link back sections from gas (i.e. data, text, bss and section directives)
  void link_section(const char *name, const char *arguments,
                    const char *create_op, const char *line_verbatim);
  // Link symbols from gas
  void link_symbol(const char *name, enum SymbolVisibility symbol_visibility,
                   const char *line_verbatim);
  // Link common symbols from gas
  void link_comm(const char *name, unsigned int common_size,
                 unsigned int common_align, const char *line_verbatim);
  // Link .type directives from gas.
  void link_type(const char *name, enum SymbolType symbol_type,
                 const char *line_verbatim);
  // Link .size directives from gas
  void link_size(const char *name, unsigned int size,
                 const char *line_verbatim);
  // Callback to mao code before as starts and when while file is parsed.
  void mao_callback(void (*callback)(void *unit));

#ifdef __cplusplus
}
#endif

#endif  // IRLINK_H_
