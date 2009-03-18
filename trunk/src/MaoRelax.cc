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

#include "MaoOptions.h"
#include "MaoRelax.h"
#include "tc-i386-helper.h"

/* TODO(nvachhar): Unparsed directives that end fragments:
   s_fill <- ".fill"
   do_org <- s_org <- ".org"
          <- assign_symbol <- s_set <- ".equ", ".equiv", ".eqv", ".set"
                           <- equals <- read_a_source_file <- ??
   bss_alloc <- ??
   do_align <- read_a_source_file <- ??
*/

extern "C" {
  extern bfd *stdoutput;
  int relax_segment(struct frag *segment_frag_root, void *segT, int pass);
  void convert_to_bignum(expressionS *exp);
  int sizeof_leb128(valueT value, int sign);
  int output_big_leb128(char *p, LITTLENUM_TYPE *bignum, int size, int sign);
  const char *S_GET_NAME(symbolS *s);
  symbolS *symbol_find (const char *name);
  void symbol_set_frag (symbolS *s, fragS *f);
  extern int finalize_syms;
}


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(RELAX, 2) {
  OPTION_BOOL("stat", false, "Collect and print statistics about relaxer"),
  OPTION_BOOL("dump_sizemap", false, "Dump the sizemap to stderr")
};



MaoRelaxer::MaoRelaxer(MaoUnit *mao_unit)
    : MaoPass("RELAX",  mao_unit->mao_options(), MAO_OPTIONS(RELAX), true),
      mao_unit_(mao_unit) {
  collect_stat_ = GetOptionBool("stat");
  dump_sizemap_ = GetOptionBool("dump_sizemap");
}

void MaoRelaxer::RelaxSection(Section *section, SizeMap *size_map) {
  // Build the fragments (and initial sizes)
  FragToEntryMap relax_map;

  // This makes sure that gas do not finalize syms after
  // its first relaxation. We want to keep the symbols
  // unresolved so that we can update the IR and still
  // get valid sizes from the relaxer.
  finalize_syms = 0;

  asection *bfd_section = bfd_get_section_by_name(stdoutput,
                                                  section->name().c_str());
  MAO_ASSERT(bfd_section);

  struct frag *fragments =
      BuildFragments(mao_unit_, section, size_map, &relax_map);

  // Run relaxation
  for (int change = 1, pass = 0; change; pass++)
    change = relax_segment(fragments, bfd_section, pass);

  // Update sizes based on relaxation
  for (struct frag *frag = fragments; frag; frag = frag->fr_next) {
    std::map<struct frag *, MaoEntry *>::iterator entry =
        relax_map.find(frag);
    if (entry == relax_map.end()) continue;

    // fr_next is guaranteed to be non-null because frag was found in
    // the relax map.
    int var_size = frag->fr_next->fr_address - frag->fr_address - frag->fr_fix;
    (*size_map)[entry->second] += var_size;
  }

  // Throw away the fragments
  FreeFragments(fragments);

  if (dump_sizemap_) {
    int offset = 0;
    for (SizeMap::const_iterator iter = size_map->begin();
         iter != size_map->end();
         ++iter) {
      fprintf(stderr, "%4x:  ", offset);
      iter->first->PrintIR(stderr);
      fprintf(stderr, "\n");
      offset += iter->second;
    }
  }

  if (collect_stat_) {
    // Print out the size for the given section, and for each function in this
    // section.
    fprintf(stderr, "Size for section %s [%d] is %d\n", section->name().c_str(),
            section->id(), SectionSize(size_map));

    // Functions?
    // TODO(martint): Clean up code. Now I added code to trigger
    // LinkBefore and LinkAfter methods.
    for (MaoUnit::ConstFunctionIterator iter = mao_unit_->ConstFunctionBegin();
         iter != mao_unit_->ConstFunctionEnd();
         ++iter) {
      Function *function = *iter;
      if (function->GetSection() == section) {
        fprintf(stderr, "Size for function %s [%d] is %d\n",
                function->name().c_str(), function->id(),
                FunctionSize(function,size_map));
        // create symbols in the object file for these?
        DirectiveEntry::OperandVector operands;
        SubSection *ss = mao_unit_->GetSubSection(function->last_entry());
        MAO_ASSERT(ss);
	//.size	<functionname>_veri, <calculated size>
        char func_verification_name[strlen(function->name().c_str())+6];
        sprintf(func_verification_name, "%s_veri", function->name().c_str());
        operands.push_back(new DirectiveEntry::Operand(func_verification_name));
        operands.push_back(new DirectiveEntry::Operand(FunctionSize(function,
                                                                    size_map)));
        DirectiveEntry *d_entry = mao_unit_->CreateDirective(
            DirectiveEntry::SIZE, operands,
            function, ss);
        function->last_entry()->LinkAfter(d_entry);

        DirectiveEntry::OperandVector operands2;
	//.type	foo, @function
        operands2.push_back(new DirectiveEntry::Operand(func_verification_name));
        operands2.push_back(new DirectiveEntry::Operand("@function"));
        DirectiveEntry *d_entry2 = mao_unit_->CreateDirective(
            DirectiveEntry::TYPE, operands2, function, ss);
        function->last_entry()->LinkBefore(d_entry2);

      }
    }
  }
}


MaoRelaxer::SizeMap *MaoRelaxer::GetSizeMap(MaoUnit *mao, Function *function) {
  if (function->sizes() == NULL) {
    // Run relaxer!
    // TODO(martint): set the sizemap per section instead
    MaoRelaxer::SizeMap *sizes = new MaoRelaxer::SizeMap();
    Section *section = function->GetSection();
    MAO_ASSERT(section);
    Relax(mao, section, sizes);
    function->set_sizes(sizes);
  }
  return function->sizes();
}

bool MaoRelaxer::HasSizeMap(Function *function) {
  return (function->sizes() != NULL);
}

void MaoRelaxer::InvalidateSizeMap(Function *function) {
  function->set_sizes(NULL);
}


struct frag *MaoRelaxer::BuildFragments(MaoUnit *mao, Section *section,
                                        SizeMap *size_map,
                                        FragToEntryMap *relax_map) {
  struct frag *fragments, *frag;
  fragments = frag = NewFragment();

  LabelEntry *le;
  symbolS *symbolP;

  bool is_text = !section->name().compare(".text");

  for (SectionEntryIterator iter = section->EntryBegin();
       iter != section->EntryEnd(); ++iter) {
    MaoEntry *entry = *iter;
    switch (entry->Type()) {
      case MaoEntry::INSTRUCTION: {
        InstructionEntry *ientry = static_cast<InstructionEntry*>(entry);
        X86InstructionSizeHelper size_helper(
            ientry->instruction());
        std::pair<int, bool> size_pair = size_helper.SizeOfInstruction();
        frag->fr_fix += size_pair.first;
        (*size_map)[entry] = size_pair.first;

        if (size_pair.second) {
          (*relax_map)[frag] = entry;
          frag = EndFragmentInstruction(ientry, frag, true);
        }

        break;
      }
      case MaoEntry::DIRECTIVE: {
        DirectiveEntry *dentry = static_cast<DirectiveEntry*>(entry);
        switch (dentry->op()) {
          case DirectiveEntry::P2ALIGN:
          case DirectiveEntry::P2ALIGNW:
          case DirectiveEntry::P2ALIGNL: {
            MAO_ASSERT(dentry->NumOperands() == 3);
            const DirectiveEntry::Operand *alignment = dentry->GetOperand(0);
            const DirectiveEntry::Operand *max = dentry->GetOperand(2);

            MAO_ASSERT(alignment->type == DirectiveEntry::INT);
            MAO_ASSERT(max->type == DirectiveEntry::INT);

            (*size_map)[entry] = 0;
            (*relax_map)[frag] = entry;
            frag = EndFragmentAlign(is_text, alignment->data.i,
                                    max->data.i, frag, true);
            break;
          }
          case DirectiveEntry::SLEB128:
          case DirectiveEntry::ULEB128: {
            bool is_signed = dentry->op() == DirectiveEntry::SLEB128;
            MAO_ASSERT(dentry->NumOperands() == 1);
            const DirectiveEntry::Operand *value = dentry->GetOperand(0);
            MAO_ASSERT(value->type == DirectiveEntry::EXPRESSION);
            expressionS *expr = value->data.expr;

            if (expr->X_op == O_constant && is_signed &&
                (expr->X_add_number < 0) != !expr->X_unsigned) {
              // TODO(nvachhar): Should we assert instead of changing the IR?
              // We're outputting a signed leb128 and the sign of X_add_number
              // doesn't reflect the sign of the original value.  Convert EXP
              // to a correctly-extended bignum instead.
              convert_to_bignum(expr);
            }

            if (expr->X_op == O_constant) {
              // If we've got a constant, compute its size right now
              int size =
                  sizeof_leb128(expr->X_add_number, is_signed ? 1 : 0);
              frag->fr_fix += size;
              (*size_map)[entry] = size;
            } else if (expr->X_op == O_big) {
              // O_big is a different sort of constant.
              int size =
                  output_big_leb128(NULL, generic_bignum,
                                    expr->X_add_number, is_signed ? 1 : 0);
              (*size_map)[entry] = size;
              frag->fr_fix += size;
            } else {
              // Otherwise, end the fragment
              (*size_map)[entry] = 0;
              (*relax_map)[frag] = entry;
              frag = EndFragmentLeb128(value, is_signed, frag, true);
            }
            break;
          }
          case DirectiveEntry::BYTE:
            frag->fr_fix++;
            (*size_map)[entry] = 1;
            break;
          case DirectiveEntry::WORD:
            frag->fr_fix += 2;
            (*size_map)[entry] = 2;
            break;
          case DirectiveEntry::RVA:
          case DirectiveEntry::LONG:
            frag->fr_fix += 4;
            (*size_map)[entry] = 4;
            break;
          case DirectiveEntry::QUAD:
            frag->fr_fix += 8;
            (*size_map)[entry] = 8;
            break;
          case DirectiveEntry::ASCII:
            HandleString(dentry, 1, false, frag, size_map);
            break;
          case DirectiveEntry::STRING8:
            HandleString(dentry, 1, true, frag, size_map);
            break;
          case DirectiveEntry::STRING16:
            HandleString(dentry, 2, true, frag, size_map);
            break;
          case DirectiveEntry::STRING32:
            HandleString(dentry, 4, true, frag, size_map);
            break;
          case DirectiveEntry::STRING64:
            HandleString(dentry, 8, true, frag, size_map);
            break;
          case DirectiveEntry::SPACE:
            frag = HandleSpace(dentry, 0, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::DS_B:
            frag = HandleSpace(dentry, 1, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::DS_W:
            frag = HandleSpace(dentry, 2, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::DS_L:
            frag = HandleSpace(dentry, 4, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::DS_D:
            frag = HandleSpace(dentry, 8, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::DS_X:
            frag = HandleSpace(dentry, 12, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::COMM:
            // TODO(martint): verify that its safe to handle COMM this way
            (*size_map)[entry] = 0;
            // Nothing to do
            break;
          case DirectiveEntry::IDENT:
            // TODO(martint): Update relaxer to handle the comment section
            // properly for the ident directive
            (*size_map)[entry] = 0;
            break;
          case DirectiveEntry::FILE:
          case DirectiveEntry::SECTION:
          case DirectiveEntry::GLOBAL:
          case DirectiveEntry::LOCAL:
          case DirectiveEntry::WEAK:
          case DirectiveEntry::TYPE:
          case DirectiveEntry::SIZE:
          case DirectiveEntry::SET:
          case DirectiveEntry::EQUIV:
          case DirectiveEntry::WEAKREF:
          case DirectiveEntry::ARCH:
          case DirectiveEntry::LINEFILE:
          case DirectiveEntry::LOC:
          case DirectiveEntry::NUM_OPCODES:
            (*size_map)[entry] = 0;
            // Nothing to do
            break;
        }
        break;
      }
      case MaoEntry::LABEL:
        // Assign the frag to the symbol
        le = entry->AsLabel();
        // Only assign frags to labels that have a symbol
        // entry int the gas symbol table.
        if (le->from_assembly()) {
          symbolP = symbol_find(le->name());
          MAO_ASSERT(symbolP != NULL);
          symbol_set_frag (symbolP, frag);
        }
        break;
      case MaoEntry::UNDEFINED:
        // Nothing to do
      default:
        MAO_ASSERT(0);
    }
  }

  EndFragmentAlign(is_text, 0, 0, frag, false);

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
#define CODE16  1
#define SMALL   0
#define SMALL16 (SMALL | CODE16)
#define BIG     2
#define BIG16   (BIG | CODE16)

#define ENCODE_RELAX_STATE(type, size) \
  ((relax_substateT) (((type) << 2) | (size)))

  i386_insn *insn = entry->instruction();

  // Only jumps should end fragments
  MAO_ASSERT(insn->tm.opcode_modifier.jump);

  int code16 = 0;
  if (flag_code == CODE_16BIT)
    code16 = CODE16;

  if (insn->prefix[X86InstructionSizeHelper::DATA_PREFIX] != 0)
    code16 ^= CODE16;

  relax_substateT subtype;
  if (insn->tm.base_opcode == JUMP_PC_RELATIVE)
    subtype = ENCODE_RELAX_STATE(UNCOND_JUMP, SMALL);
  else if (cpu_arch_flags.bitfield.cpui386)
    subtype = ENCODE_RELAX_STATE(COND_JUMP, SMALL);
  else
    subtype = ENCODE_RELAX_STATE(COND_JUMP86, SMALL);
  subtype |= code16;

  symbolS *sym = insn->op[0].disps->X_add_symbol;
  offsetT off = insn->op[0].disps->X_add_number;

  if (insn->op[0].disps->X_op != O_constant &&
      insn->op[0].disps->X_op != O_symbol) {
    /* Handle complex expressions.  */
    sym = make_expr_symbol(insn->op[0].disps);
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


struct frag *MaoRelaxer::EndFragmentAlign(bool code,
                                          int alignment, int max,
                                          struct frag *frag,
                                          bool new_frag) {
  relax_stateT state = code ? rs_align_code : rs_align_code;
  return FragVar(state, 1,
                 static_cast<relax_substateT>(max), NULL,
                 static_cast<offsetT>(alignment), NULL,
                 frag, new_frag);
}


struct frag *MaoRelaxer::EndFragmentLeb128(
    const DirectiveEntry::Operand *value,
    bool is_signed,
    struct frag *frag,
    bool new_frag) {
  // TODO(nvachhar): Ugh... we have to create a symbol
  // here to store in the fragment.  This means each
  // execution of relaxation allocates memory that will
  // never be freed.  Let's hope relaxation doesn't run
  // too often.
  return FragVar(rs_leb128, 0,
                 static_cast<relax_substateT>(is_signed),
                 make_expr_symbol(value->data.expr),
                 static_cast<offsetT>(0), NULL,
                 frag, new_frag);
}


struct frag *MaoRelaxer::HandleSpace(DirectiveEntry *entry,
                                     int mult,
                                     struct frag *frag,
                                     bool new_frag,
                                     SizeMap *size_map,
                                     FragToEntryMap *relax_map) {
  MAO_ASSERT(entry->NumOperands() == 2);
  const DirectiveEntry::Operand *size_opnd = entry->GetOperand(0);
  MAO_ASSERT(size_opnd->type == DirectiveEntry::EXPRESSION);
  expressionS *size = size_opnd->data.expr;

  if (size->X_op == O_constant) {
    int increment = size->X_add_number * (mult ? mult : 1);
    MAO_ASSERT(increment > 0);
    frag->fr_fix += increment;
    (*size_map)[entry] = increment;
  } else {
    MAO_ASSERT(mult == 0 || mult == 1);
    // TODO(nvachhar): Ugh... we have to create a symbol
    // here to store in the fragment.  This means each
    // execution of relaxation allocates memory that will
    // never be freed.  Let's hope relaxation doesn't run
    // too often.
    (*size_map)[entry] = 0;
    (*relax_map)[frag] = entry;
    frag = FragVar(rs_space, 1, (relax_substateT) 0, make_expr_symbol(size),
                   (offsetT) 0, NULL, frag, new_frag);
  }

  return frag;
}


void MaoRelaxer::HandleString(DirectiveEntry *entry, int multiplier,
                              bool null_terminate, struct frag *frag,
                              SizeMap *size_map) {
  int size = StringSize(entry, multiplier, null_terminate);
  (*size_map)[entry] = size;
  frag->fr_fix += size;
}


int MaoRelaxer::StringSize(DirectiveEntry *entry, int multiplier,
                           bool null_terminate) {
  MAO_ASSERT(entry->NumOperands() == 1);
  const DirectiveEntry::Operand *value = entry->GetOperand(0);
  MAO_ASSERT(value->type == DirectiveEntry::STRING);

  // Subtract 2 for the quotes, add the null terminator if necessary
  // and then multiply by the character size.
  return multiplier * ((value->data.str->length() - 2) +
                       (null_terminate ? 1 : 0));
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
  TC_FRAG_INIT(frag);
#endif
}


int MaoRelaxer::SectionSize(SizeMap *size_map) {
  int size = 0;
  for (SizeMap::const_iterator iter = size_map->begin();
       iter != size_map->end();
       ++iter) {
    size += iter->second;
  }
  return size;
}

int MaoRelaxer::FunctionSize(Function *function, SizeMap *size_map) {
  int size = 0;
  for(SectionEntryIterator iter = function->EntryBegin();
      iter != function->EntryEnd();
      ++iter) {
    if (!(*iter)->IsLabel()) {
      MAO_ASSERT(size_map->find(*iter) != size_map->end());
      size += (*size_map)[*iter];
    }
  }
  return size;
}


// --------------------------------------------------------------------
// External entry point
// --------------------------------------------------------------------
void Relax(MaoUnit *mao, Section *section, MaoRelaxer::SizeMap *size_map) {
  MaoRelaxer relaxer(mao);
  relaxer.RelaxSection(section, size_map);
}
