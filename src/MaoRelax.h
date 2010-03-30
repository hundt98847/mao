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

// Relaxer calculates the sizes of all instructions inside a a given
// section. The result can be given as sizemap (maps entries to sizes)
// or as an offsetmap (maps entries to its address inside the
// section).  Results are cached, and the results must be invalidated
// using InvalidateSizeMap() if the IR is changed.
//
// The name comes from the algorithm used.
// Usage:
//   MaoEntryIntMap *sizes = MaoRelaxer::GetSizeMap(unit_,
//                                                  function_->GetSection());
//   int entry_size = (*sizes)[entry];

#ifndef MAORELAX_H_
#define MAORELAX_H_

#include <map>
#include <vector>

#include "MaoDebug.h"
#include "MaoPasses.h"
#include "MaoUnit.h"
#include "tc-i386-helper.h"

// The main relaxation class.
//
// Use the static methods MaoRelaxer::GetSizeMap() and
// MaoRelaxer::GetOffsetMap() to access the results. Results are
// cached per section and needs to be explicitly invalidated using
// MaoRelaxer::InvalidateSizeMap() if the section is updated.
class MaoRelaxer : public MaoPass {
 public:
  MaoRelaxer(MaoUnit *mao_unit, Section *section, MaoEntryIntMap *size_map,
             MaoEntryIntMap *offset_map);
  bool Go();

  // Used when a pass needs the sizes for a given section.
  // Returns the sizemap, which holds entries for the whole section.
  static MaoEntryIntMap *GetSizeMap(MaoUnit *mao, Section *section);
  // Used when a pass needs the offsets for a given section.
  // Returns the offsetmap, which holds entries for the whole section.
  static MaoEntryIntMap *GetOffsetMap(MaoUnit *mao, Section *section);
  // Checks if a section has a sizemap and an offsetmap computed.
  static bool HasSizeMap(Section *section);
  // Invalidates the sizemap and offsetmap for the section.
  static void InvalidateSizeMap(Section *section);

 private:
  typedef std::map<struct frag *, MaoEntry *> FragToEntryMap;

  static struct frag *BuildFragments(
      MaoUnit *mao, Section *section, MaoEntryIntMap *size_map,
      FragToEntryMap *relax_map);

  static int SizeOfFloat(DirectiveEntry *entry);

  static void UpdateSymbol(const char *symbol_name,
                           struct frag *frag);

  static struct frag *EndFragmentInstruction(
      InstructionEntry *entry, struct frag *frag, bool new_frag);

  static struct frag *EndFragmentAlign(
      bool code, int alignment, int max, struct frag *frag, bool new_frag);

  static struct frag *EndFragmentLeb128(
      const DirectiveEntry::Operand *value, bool is_signed,
      struct frag *frag, bool new_frag);

  static struct frag *HandleSpace(
      DirectiveEntry *entry, int mult, struct frag *frag,
      bool new_frag, MaoEntryIntMap *size_map, FragToEntryMap *relax_map);

  static struct frag *HandleFill(
      DirectiveEntry *entry, struct frag *frag,
      bool new_frag, MaoEntryIntMap *size_map, FragToEntryMap *relax_map);

  static void HandleString(
      DirectiveEntry *entry, int multiplier, bool null_terminate,
      struct frag *frag, MaoEntryIntMap *size_map);

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

  static int SectionSize(MaoEntryIntMap *size_map);
  static int FunctionSize(Function *function, MaoEntryIntMap *size_map);

  static void CacheSizeAndOffsetMap(MaoUnit *mao, Section *section);

  // The functions below makes is possible to revert any changes
  // to the IR the relaxation does. The relaxer will change the
  // base opcode of jump instruction in md_estimate_size_before_relax()
  // Maps a fragment to the original opcode.
  typedef std::map<const struct frag *, unsigned int> FragState;
  void SaveState(const struct frag *fragments, FragState *state);
  void RestoreState(const struct frag *fragments, FragState *state);

  Section *section_;
  MaoEntryIntMap *size_map_;
  MaoEntryIntMap *offset_map_;

  bool collect_stat_;
  bool dump_sizemap_;
  bool dump_function_stat_;

  class RelaxStat : public Stat {
   public:
    RelaxStat()  {}
    ~RelaxStat() {}
    void AddFunction(const Function *func, int size) {
      function_sizes_.push_back(std::make_pair(func, size));
    }
    virtual void Print(FILE *out) {
      // Iterate over the functions
       for (std::vector<std::pair<const Function *, int> >::const_iterator iter
                = function_sizes_.begin();
            iter != function_sizes_.end();
            ++iter) {
         fprintf(out, "MaoRelax functionsize %-60s %4d\n",
                 iter->first->name().c_str(), iter->second);
       }
    }

   private:
    std::vector<std::pair<const Function *, int> > function_sizes_;
  };

  RelaxStat *relax_stat_;
};

// External entry point
void Relax(MaoUnit *mao, Section *section, MaoEntryIntMap *size_map,
           MaoEntryIntMap *offset_map);

#endif  // MAORELAX_H_
