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

#ifndef MAORELAX_H_
#define MAORELAX_H_

#include "MaoDebug.h"
#include "MaoPasses.h"
#include "MaoUnit.h"
#include "tc-i386-helper.h"

#include <map>

class MaoRelaxer : public MaoPass {
 public:
  typedef std::map<MaoEntry *, int> SizeMap;

  MaoRelaxer(MaoUnit *mao_unit);
  void RelaxSection(Section *section, SizeMap *size_map);

  // Used when a pass needs the sizes for a given function.
  // Returns the sizemap, which holds entries for the whole section.
  static SizeMap *GetSizeMap(MaoUnit *mao, Function *function);
  // Checks if function have a sizemap computed
  static bool HasSizeMap(Function *function);

  // Invalidates the sizemap for the section the function is member of.
  static void InvalidateSizeMap(Function *function);

 private:
  typedef std::map<struct frag *, MaoEntry *> FragToEntryMap;

  static struct frag *BuildFragments(
      MaoUnit *mao, Section *section, SizeMap *size_map,
      FragToEntryMap *relax_map);

  static struct frag *EndFragmentInstruction(
      InstructionEntry *entry, struct frag *frag, bool new_frag);

  static struct frag *EndFragmentAlign(
      bool code, int alignment, int max, struct frag *frag, bool new_frag);

  static struct frag *EndFragmentLeb128(
      const DirectiveEntry::Operand *value, bool is_signed,
      struct frag *frag, bool new_frag);

  static struct frag *HandleSpace(
      DirectiveEntry *entry, int mult, struct frag *frag,
      bool new_frag, SizeMap *size_map, FragToEntryMap *relax_map);

  static void HandleString(
      DirectiveEntry *entry, int multiplier, bool null_terminate,
      struct frag *frag, SizeMap *size_map);

  static int StringSize(
      DirectiveEntry *entry, int multiplier, bool null_terminate);

  static struct frag *FragVar(
      relax_stateT type, int var, relax_substateT subtype,
      symbolS *symbol, offsetT offset, char *opcode, struct frag *frag,
      bool new_frag);

  static void FragInitOther(struct frag *frag);

  static struct frag *NewFragment() {
    struct frag *frag =
        static_cast<struct frag *>(calloc(1, sizeof(struct frag)));
    MAO_ASSERT(frag);
    return frag;
  }

  static void FreeFragments(struct frag *fragments) {
    struct frag *next;
    for (next = fragments->fr_next; fragments; fragments = next) {
      next = fragments->fr_next;
      free(fragments);
    }
  }

  static int SectionSize(SizeMap *size_map);
  static int FunctionSize(Function *function, SizeMap *size_map);


  MaoUnit *mao_unit_;
  bool collect_stat_;
  bool dump_sizemap_;
  bool verification_symbols_;

  class RelaxStat : public Stat {
   public:
    RelaxStat()  {}
    ~RelaxStat() {}
    void AddFunction(const Function *func, int size) {
      function_sizes_.push_back(std::make_pair(func, size));
    }
    virtual void Print(FILE *out) {
      // Iterate over the functions
       for (std::vector<std::pair<const Function *, int> >::const_iterator iter =
                function_sizes_.begin();
            iter != function_sizes_.end();
            ++iter) {
         fprintf(out, "MaoRelax functionsize %-60s %4d\n", iter->first->name().c_str(),
                 iter->second);
       }
    }

   private:
    std::vector<std::pair<const Function *, int> > function_sizes_;
  };

  RelaxStat *relax_stat_;
};

// External entry point
void Relax(MaoUnit *mao, Section *section, MaoRelaxer::SizeMap *size_map);

#endif  // MAORELAX_H_
