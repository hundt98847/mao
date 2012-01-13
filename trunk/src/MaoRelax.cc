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

#include <map>
#include <string>
#include <vector>

#include "Mao.h"
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
  void convert_to_bignum(expressionS *exp);
  int output_big_leb128(char *p, LITTLENUM_TYPE *bignum, int size, int sign);
  extern i386_cpu_flags cpu_arch_flags;
}


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(RELAX, "Runs a relaxation algorithm to compute sizes and" \
                   " offsets of all instructions", 3) {
  OPTION_BOOL("collect_stats", false,
              "Collect and print a table with statistics about relaxer "
              "from all the processed functions."),
  OPTION_BOOL("dump_sizemap", false, "Dump the sizemap to stderr"),
  OPTION_BOOL("dump_function_stat", false, "Dump information about each function"
              " to stderr"),
};



MaoRelaxer::MaoRelaxer(MaoUnit *mao_unit, Section *section,
                       MaoEntryIntMap *size_map, MaoEntryIntMap *offset_map)
    : MaoPass("RELAX",  GetStaticOptionPass("RELAX"), mao_unit),
      section_(section), size_map_(size_map), offset_map_(offset_map) {
  collect_stat_ = GetOptionBool("collect_stats");
  dump_sizemap_ = GetOptionBool("dump_sizemap");
  dump_function_stat_ = GetOptionBool("dump_function_stat");
  if (collect_stat_) {
    if (unit_->GetStats()->HasStat("RELAX")) {
      relax_stat_ =
          static_cast<RelaxStat *>(unit_->GetStats()->GetStat("RELAX"));
    } else {
      relax_stat_ = new RelaxStat();
      unit_->GetStats()->Add("RELAX", relax_stat_);
    }
  }
}


void MaoRelaxer::SaveState(const struct frag *fragments, FragState *state) {
  // Save the old opcodes, so we can restore them!
  for (const struct frag *fr_iter = fragments; fr_iter;
       fr_iter = fr_iter->fr_next) {
    if (fr_iter->fr_opcode) {
      (*state)[fr_iter] = *((unsigned int *)(fr_iter->fr_opcode));
    }
  }
}

void MaoRelaxer::RestoreState(const struct frag *fragments, FragState *state) {
  // Save the old opcodes, so we can restore them!
  for (const struct frag *fr_iter = fragments; fr_iter;
       fr_iter = fr_iter->fr_next) {
    if (fr_iter->fr_opcode) {
      *((unsigned int *)fr_iter->fr_opcode) = (*state)[fr_iter];
    }
  }
}

bool MaoRelaxer::Go() {
  // Build the fragments (and initial sizes)
  FragToEntryMap relax_map;

  // This makes sure that gas do not finalize syms after
  // its first relaxation. We want to keep the symbols
  // unresolved so that we can update the IR and still
  // get valid sizes from the relaxer.
  finalize_syms = 0;

  asection *bfd_section = bfd_get_section_by_name(stdoutput,
                                                  section_->name().c_str());
  MAO_ASSERT(bfd_section);

  struct frag *fragments =
      BuildFragments(unit_, section_, size_map_, &relax_map);

  // The relaxer updates the instruction through the fragment. The opcode
  // will change. Since we want to be able to relax several times,
  // we save the state here and restore it after the sizemap has been built.
  FragState fragment_state;
  SaveState(fragments, &fragment_state);

  // Relaxation normally only changes the fr_var part of the
  // fragment. There are some cases (see md_estimate_size_before_relax())
  // where fr_fix is changed as well. Therefore we need to keep track
  // of the fr_fix values before relaxation. We do that in old_fr_fix.
  std::vector<int> old_fr_fix;
  for (struct frag *frag = fragments; frag; frag = frag->fr_next) {
    old_fr_fix.push_back(frag->fr_fix);
  }

  // Run relaxation
  for (int change = 1, pass = 0; change; pass++)
    change = relax_segment(fragments, bfd_section, pass);

  // Update sizes based on relaxation
  int frag_index = 0;
  for (struct frag *frag = fragments; frag; frag = frag->fr_next) {
    std::map<struct frag *, MaoEntry *>::iterator entry =
        relax_map.find(frag);
    if (entry == relax_map.end()) continue;

    // fr_next is guaranteed to be non-null because frag was found in
    // the relax map.
    int var_size = frag->fr_next->fr_address - frag->fr_address - frag->fr_fix;
    (*size_map_)[entry->second] += var_size;
    // Check if fr_fix have changed in relaxation:
    if (frag->fr_fix != old_fr_fix[frag_index]) {
      (*size_map_)[entry->second] += (frag->fr_fix - old_fr_fix[frag_index]);
    }
    frag_index++;
  }

  // calculate offset map
  int offset = 0;
  for (EntryIterator iter = section_->EntryBegin();
       iter != section_->EntryEnd(); ++iter) {
    (*offset_map_)[*iter] = offset;
    offset += (*size_map_)[*iter];
  }


  // Restore fragments/instructions to the originial state.
  RestoreState(fragments, &fragment_state);

  // Throw away the fragments
  FreeFragments(fragments);

  if (dump_sizemap_) {
    for (MaoEntryIntMap::const_iterator iter = size_map_->begin();
         iter != size_map_->end();
         ++iter) {
      fprintf(stderr, "%4x:  ", (*offset_map_)[iter->first]);
      fprintf(stderr, "%4x:  ", iter->second);
      iter->first->PrintIR(stderr);
      fprintf(stderr, "\n");
    }
  }

  if (dump_function_stat_) {
    fprintf(stderr, "Function information for section: %s",
            section_->name().c_str());
    fprintf(stderr, "%20s %10s %10s\n",
            "Function:", "Offset", "Size");
    for (MaoUnit::ConstFunctionIterator iter = unit_->ConstFunctionBegin();
         iter != unit_->ConstFunctionEnd();
         ++iter) {
      Function *function = *iter;
      if (function->GetSection() == section_) {
        fprintf(stderr, "%20s %10x %10d\n",
                function->name().c_str(),
                (*offset_map_)[function->first_entry()],
                FunctionSize(function, size_map_));
      }
    }
  }


  if (collect_stat_) {
    for (MaoUnit::ConstFunctionIterator iter = unit_->ConstFunctionBegin();
         iter != unit_->ConstFunctionEnd();
         ++iter) {
      Function *function = *iter;
      if (function->GetSection() == section_) {
        relax_stat_->AddFunction(function, FunctionSize(function,
                                                        size_map_));
      }
    }
  }

  return true;
}


MaoEntryIntMap *MaoRelaxer::GetSizeMap(MaoUnit *mao, Section *section) {
  CacheSizeAndOffsetMap(mao, section);
  return section->sizes();
}

MaoEntryIntMap *MaoRelaxer::GetOffsetMap(MaoUnit *mao, Section *section) {
  CacheSizeAndOffsetMap(mao, section);
  return section->offsets();
}

void MaoRelaxer::CacheSizeAndOffsetMap(MaoUnit *mao, Section *section) {
  MAO_ASSERT(section);
  MaoEntryIntMap *offsets, *sizes = section->sizes();
  if (sizes == NULL) {
    sizes   = new MaoEntryIntMap();
    offsets = new MaoEntryIntMap();

    Relax(mao, section, sizes, offsets);

    section->set_sizes(sizes);
    section->set_offsets(offsets);
  }
}


bool MaoRelaxer::HasSizeMap(Section *section) {
  return (section->sizes() != NULL);
}

void MaoRelaxer::InvalidateSizeMap(Section *section) {
  section->set_sizes(NULL);
  section->set_offsets(NULL);
}


struct frag *MaoRelaxer::BuildFragments(MaoUnit *mao, Section *section,
                                        MaoEntryIntMap *size_map,
                                        FragToEntryMap *relax_map) {
  struct frag *fragments, *frag;
  fragments = frag = NewFragment();

  bool is_text = !section->name().compare(".text");

  for (EntryIterator iter = section->EntryBegin();
       iter != section->EntryEnd(); ++iter) {
    MaoEntry *entry = *iter;
    switch (entry->Type()) {
      case MaoEntry::INSTRUCTION: {
        InstructionEntry *ientry = static_cast<InstructionEntry*>(entry);
        X86InstructionSizeHelper size_helper(
            ientry->instruction());
        std::pair<int, bool> size_pair =
            size_helper.SizeOfInstruction(ientry->GetFlag());
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
          case DirectiveEntry::SET:
          case DirectiveEntry::FILE:
          case DirectiveEntry::SECTION:
          case DirectiveEntry::GLOBAL:
          case DirectiveEntry::LOCAL:
          case DirectiveEntry::WEAK:
          case DirectiveEntry::TYPE:
          case DirectiveEntry::SIZE:
          case DirectiveEntry::EQUIV:
          case DirectiveEntry::WEAKREF:
          case DirectiveEntry::ARCH:
          case DirectiveEntry::LINEFILE:
          case DirectiveEntry::LOC:
          case DirectiveEntry::ALLOW_INDEX_REG:
          case DirectiveEntry::DISALLOW_INDEX_REG:
            (*size_map)[entry] = 0;
            break;
          case DirectiveEntry::ORG:
            // TODO(martint): Add support for ORG directives in the relaxer.
            MAO_ASSERT_MSG(false, ".org directive unsupported in relaxer.");
          case DirectiveEntry::CODE16:
          case DirectiveEntry::CODE16GCC:
          case DirectiveEntry::CODE32:
          case DirectiveEntry::CODE64:
            (*size_map)[entry] = 0;
            break;
          case DirectiveEntry::DC_D:
          case DirectiveEntry::DC_S:
          case DirectiveEntry::DC_X:
            (*size_map)[entry] = SizeOfFloat(dentry);
            break;
          case DirectiveEntry::HIDDEN:
            (*size_map)[entry] = 0;
            break;
          case DirectiveEntry::FILL:
            frag = HandleFill(dentry, frag, true, size_map, relax_map);
            break;
          case DirectiveEntry::STRUCT:
            // TODO(martint): Add support for .struct/.offset directives
            MAO_ASSERT_MSG(false, ".struct directive unsupported in relaxer.");
          case DirectiveEntry::INCBIN:
            // TODO(martint): Add support for .struct/.offset directives
            MAO_ASSERT_MSG(false, ".struct directive unsupported in relaxer.");
          case DirectiveEntry::SYMVER:
          case DirectiveEntry::LOC_MARK_LABELS:
          case DirectiveEntry::CFI_STARTPROC:
          case DirectiveEntry::CFI_ENDPROC:
          case DirectiveEntry::CFI_DEF_CFA:
          case DirectiveEntry::CFI_DEF_CFA_REGISTER:
          case DirectiveEntry::CFI_DEF_CFA_OFFSET:
          case DirectiveEntry::CFI_ADJUST_CFA_OFFSET:
          case DirectiveEntry::CFI_OFFSET:
          case DirectiveEntry::CFI_REL_OFFSET:
          case DirectiveEntry::CFI_REGISTER:
          case DirectiveEntry::CFI_RETURN_COLUMN:
          case DirectiveEntry::CFI_RESTORE:
          case DirectiveEntry::CFI_UNDEFINED:
          case DirectiveEntry::CFI_SAME_VALUE:
          case DirectiveEntry::CFI_REMEMBER_STATE:
          case DirectiveEntry::CFI_RESTORE_STATE:
          case DirectiveEntry::CFI_WINDOW_SAVE:
          case DirectiveEntry::CFI_ESCAPE:
          case DirectiveEntry::CFI_SIGNAL_FRAME:
          case DirectiveEntry::CFI_PERSONALITY:
          case DirectiveEntry::CFI_LSDA:
          case DirectiveEntry::CFI_VAL_ENCODED_ADDR:
            (*size_map)[entry] = 0;
            break;
          case DirectiveEntry::NUM_OPCODES: // should never happen..
          default:
            MAO_ASSERT_MSG(0, "Unhandled directive: %d", dentry->op());
        }
        break;
      }
      case MaoEntry::LABEL: {
        LabelEntry *le;
        // Assign the frag to the symbol
        le = entry->AsLabel();
        // Only assign frags to labels that have a symbol
        // entry int the gas symbol table.
        if (le->from_assembly()) {
          UpdateSymbol(le->name(), frag);
          // Check if there are any "equal" symbols that needs to be updated
          // for the relaxer to work.
          Symbol *s = mao->GetSymbolTable()->Find(le->name());
          MAO_ASSERT(s != NULL);
          for (Symbol::EqualIterator iter = s->EqualBegin();
               iter != s->EqualEnd();
               ++iter) {
            UpdateSymbol((*iter)->name(), frag);
          }
        }
        break;
      }
      case MaoEntry::UNDEFINED:
        // Nothing to do
      default:
        MAO_ASSERT(0);
    }
  }

  EndFragmentAlign(is_text, 0, 0, frag, false);

  return fragments;
}


int MaoRelaxer::SizeOfFloat(DirectiveEntry *entry) {
  MAO_ASSERT(entry->NumOperands() == 1);
  const DirectiveEntry::Operand *size_opnd = entry->GetOperand(0);
  MAO_ASSERT(size_opnd->type == DirectiveEntry::STRING);
  std::string *str = size_opnd->data.str;
  if (str->compare(0, 3, "0x:") == 0) {
    // Hexadecmial floats always have a fixed size.
    switch (entry->op()) {
      case DirectiveEntry::DC_D:
        return 8;
        break;
      case DirectiveEntry::DC_S:
        return 4;
        break;
      case DirectiveEntry::DC_X:
        return 12;
        break;
      default:
        MAO_ASSERT(false);
    }
  } else {
    switch (entry->op()) {
#define F_PRECISION    2
#define D_PRECISION    4
#define X_PRECISION    5
#define P_PRECISION    5
      case DirectiveEntry::DC_D:
        return D_PRECISION * sizeof (LITTLENUM_TYPE);
        break;
      case DirectiveEntry::DC_S:
        return F_PRECISION * sizeof (LITTLENUM_TYPE);
        break;
      case DirectiveEntry::DC_X:
        return P_PRECISION * sizeof (LITTLENUM_TYPE);
        break;
      default:
        MAO_ASSERT(false);
    }
  }
  return 1;
}


// Update the symbol to hold a reference to the current frag
void MaoRelaxer::UpdateSymbol(const char *symbol_name,
                              struct frag *frag) {
  symbolS *symbolP = symbol_find(symbol_name);
  MAO_ASSERT(symbolP != NULL);
  symbol_set_frag(symbolP, frag);
  // TODO(martint): find out where OCTETS_PER_BYTE is defined
  S_SET_VALUE(symbolP, frag->fr_fix);  //  / OCTETS_PER_BYTE
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
  if (entry->GetFlag() == CODE_16BIT)
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
                                     MaoEntryIntMap *size_map,
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


struct frag *MaoRelaxer::HandleFill(DirectiveEntry *entry,
                                    struct frag *frag,
                                    bool new_frag,
                                    MaoEntryIntMap *size_map,
                                    FragToEntryMap *relax_map) {
  MAO_ASSERT(entry->NumOperands() == 3);
  const DirectiveEntry::Operand *repeat_opnd = entry->GetOperand(0);
  const DirectiveEntry::Operand *size_opnd = entry->GetOperand(1);
  MAO_ASSERT(repeat_opnd->type == DirectiveEntry::EXPRESSION);
  MAO_ASSERT(size_opnd->type == DirectiveEntry::INT);

  expressionS *repeat_exp = repeat_opnd->data.expr;
  int size = size_opnd->data.i;
  MAO_ASSERT(size >= 1);

  if (repeat_exp->X_op == O_constant) {
    frag->fr_fix +=  size * repeat_exp->X_add_number;
    (*size_map)[entry] = size * repeat_exp->X_add_number;
  } else {
    // TODO(nvachhar): Ugh... we have to create a symbol
    // here to store in the fragment.  This means each
    // execution of relaxation allocates memory that will
    // never be freed.  Let's hope relaxation doesn't run
    // too often.

    symbol *rep_sym = make_expr_symbol(repeat_exp);
    if (size != 1) {
      // simple case
      rep_sym = make_expr_symbol (repeat_exp);
    } else {
      // Need to create temporary expression. Ugh... (again)
      // The size is repeat * size
      expressionS size_exp;
      size_exp.X_op = O_constant;
      size_exp.X_add_number = size;

      repeat_exp->X_op = O_multiply;
      repeat_exp->X_add_symbol = rep_sym;
      repeat_exp->X_op_symbol = make_expr_symbol (&size_exp);
      repeat_exp->X_add_number = 0;
      rep_sym = make_expr_symbol (repeat_exp);
    }

    (*size_map)[entry] = 0;
    (*relax_map)[frag] = entry;
    frag = FragVar(rs_space, 1, (relax_substateT) 0, rep_sym,
                     (offsetT) 0, NULL, frag, new_frag);
  }
  return frag;
}



void MaoRelaxer::HandleString(DirectiveEntry *entry, int multiplier,
                              bool null_terminate, struct frag *frag,
                              MaoEntryIntMap *size_map) {
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


int MaoRelaxer::SectionSize(MaoEntryIntMap *size_map) {
  int size = 0;
  for (MaoEntryIntMap::const_iterator iter = size_map->begin();
       iter != size_map->end();
       ++iter) {
    size += iter->second;
  }
  return size;
}

int MaoRelaxer::FunctionSize(Function *function, MaoEntryIntMap *size_map) {
  int size = 0;
  for (EntryIterator iter = function->EntryBegin();
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
void Relax(MaoUnit *mao, Section *section, MaoEntryIntMap *size_map,
           MaoEntryIntMap *offset_map) {
  MaoRelaxer relaxer(mao, section, size_map, offset_map);
  relaxer.Go();
}

void InitRelax() {
  RegisterStaticOptionPass("RELAX", new MaoOptionMap);
}
