//
// Copyright 2009 and later Google Inc.
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
// along with this program; if not, write to the
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#include <stdio.h>
#include <stdlib.h>

#include "MaoRelax.h"
#include "tc-i386-helper.h"

extern "C" {
  extern bfd *stdoutput;
  int relax_segment(struct frag *segment_frag_root, void *segT, int pass);
}

void MaoRelaxer::Relax() {
  struct frag *fragments = BuildFragments();

  // TODO(nvachhar): Why is the dot('.') omitted from the section
  // name?  Can we add it back to avoid this hackery?
  std::string section_name(".");
  section_name.append(section_->name());
  asection *section = bfd_get_section_by_name(stdoutput, section_name.c_str());

  for (int change = 1, pass = 0; change; pass++)
    change = relax_segment(fragments, section, pass);

  FreeFragments(fragments);
}

struct frag *MaoRelaxer::BuildFragments() {
  struct frag *fragments, *frag;
  fragments = frag = NewFragment();

  for (SectionEntryIterator iter = section_->EntryBegin(mao_);
       iter != section_->EntryEnd(mao_); ++iter) {
    MaoUnitEntryBase *entry = *iter;
    switch (entry->Type()) {
      case MaoUnitEntryBase::INSTRUCTION: {
        InstructionEntry *ientry = static_cast<InstructionEntry*>(entry);
        X86InstructionSizeHelper size_helper(
            ientry->instruction()->instruction());
        std::pair<int, bool> size_pair = size_helper.SizeOfInstruction();
        frag->fr_fix += size_pair.first;

        if (size_pair.second)
          frag = EndFragmentInstruction(ientry, frag, true);

        break;
      }
      case MaoUnitEntryBase::DIRECTIVE: {
        DirectiveEntry *directive = static_cast<DirectiveEntry*>(entry);
        printf("%s %s\n", directive->key().c_str(), directive->value().c_str());
        break;
      }
      case MaoUnitEntryBase::LABEL:
      case MaoUnitEntryBase::DEBUG:
        /* Nothing to do */
        break;
      case MaoUnitEntryBase::UNDEFINED:
      default:
        MAO_ASSERT(0);
    }
  }

  if (!strcmp(section_->name(), "text"))
    EndFragmentCodeAlign(0, 0, frag, false);
  else
    // TODO(nvachhar):
    MAO_ASSERT(false);

  return fragments;
}


struct frag *MaoRelaxer::EndFragmentInstruction(InstructionEntry *entry,
                                                struct frag *frag,
                                                bool new_frag) {
/* Types.  */
#define UNCOND_JUMP 0
#define COND_JUMP 1
#define COND_JUMP86 2

/* Sizes.  */
#define CODE16	1
#define SMALL	0
#define SMALL16 (SMALL | CODE16)
#define BIG	2
#define BIG16	(BIG | CODE16)

#define ENCODE_RELAX_STATE(type, size) \
  ((relax_substateT) (((type) << 2) | (size)))

  i386_insn *insn = entry->instruction()->instruction();

  // Only jumps should end fragments
  MAO_ASSERT (insn->tm.opcode_modifier.jump);

  int code16 = 0;
  if (flag_code == CODE_16BIT)
    code16 = CODE16;

  if (insn->prefix[X86InstructionSizeHelper::DATA_PREFIX] != 0)
    code16 ^= CODE16;

  relax_substateT subtype;
  if (insn->tm.base_opcode == JUMP_PC_RELATIVE)
    subtype = ENCODE_RELAX_STATE (UNCOND_JUMP, SMALL);
  else if (cpu_arch_flags.bitfield.cpui386)
    subtype = ENCODE_RELAX_STATE (COND_JUMP, SMALL);
  else
    subtype = ENCODE_RELAX_STATE (COND_JUMP86, SMALL);
  subtype |= code16;

  symbolS *sym = insn->op[0].disps->X_add_symbol;
  offsetT off = insn->op[0].disps->X_add_number;

  if (insn->op[0].disps->X_op != O_constant &&
      insn->op[0].disps->X_op != O_symbol) {
    /* Handle complex expressions.  */
    sym = make_expr_symbol (insn->op[0].disps);
    off = 0;
  }

  return FragVar(rs_machine_dependent, insn->reloc[0],
                 subtype, sym, off,
                 reinterpret_cast<char*>(&insn->tm.base_opcode),
                 frag, new_frag);

#undef UNCOND_JUMP
#undef COND_JUMP
#undef COND_JUMP86

#undef CODE16
#undef SMALL
#undef SMALL16
#undef BIG
#undef BIG16

#undef ENCODE_RELAX_STATE
}


struct frag *MaoRelaxer::EndFragmentCodeAlign(int alignment, int max,
                                              struct frag *frag,
                                              bool new_frag) {
  return FragVar(rs_align_code, 1,
                 static_cast<relax_substateT>(max), NULL,
                 static_cast<offsetT>(alignment), NULL,
                 frag, new_frag);
}


struct frag *MaoRelaxer::FragVar(relax_stateT type, int var,
                                 relax_substateT subtype,
                                 symbolS *symbol, offsetT offset,
                                 char *opcode, struct frag *frag,
                                 bool new_frag) {
  frag->fr_var = var;
  frag->fr_type = type;
  frag->fr_subtype = subtype;
  frag->fr_symbol = symbol;
  frag->fr_offset = offset;
  frag->fr_opcode = opcode;
  FragInitOther(frag);

  if (new_frag)
    frag->fr_next = NewFragment();
  return frag->fr_next;
}


void MaoRelaxer::FragInitOther(struct frag *frag) {
#ifdef USING_CGEN
  frag->fr_cgen.insn = 0;
  frag->fr_cgen.opindex = 0;
  frag->fr_cgen.opinfo = 0;
#endif
#ifdef TC_FRAG_INIT
  TC_FRAG_INIT (frag);
#endif
}
