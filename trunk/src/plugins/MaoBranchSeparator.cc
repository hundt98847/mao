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

#include "Mao.h"

#include <list>
#include <map>
#include <vector>



namespace {

PLUGIN_VERSION

#define FETCH_LINE_SIZE 16

// Pass that finds paths in inner loops that fits in 4 16-byte lines
// and alligns all (chains of) basicb locks within the paths.
class BranchSeparatorPass : public MaoFunctionPass {
 public:
  explicit BranchSeparatorPass(MaoOptionMap *options, MaoUnit *mao,
                               Function *function);
  bool Go();

 private:
  // Path is a list of basic blocks that form a way through the loop
//   typedef std::vector<BasicBlock *> Path;
//   typedef std::vector<Path *> Paths;

  // This is the sizes of all the instructions in the section
  // as found by the relaxer.
  MaoEntryIntMap *sizes_;
  // Collect stat during the pass?
  bool collect_stat_;
  //Required distance between branches
  int min_branch_distance_;

  bool last_byte_;
  bool profitable;


  class BranchSeparatorStat : public Stat {
   public:
    BranchSeparatorStat(int distance) : num_branches_(0), num_branches_realigned_(0),
                             relaxations_(0), distance_(distance) {
      nop_counts_ = new int[distance];
      memset (nop_counts_, 0, sizeof(int)*distance_);
    }

    ~BranchSeparatorStat() {delete [] nop_counts_;}

    void FoundBranch() {
      num_branches_++;
    }
    void Relaxed() {
      relaxations_++;
    }

    void RealigningBranch(int nops) {
      num_branches_realigned_++;
      nop_counts_[nops]++;
    }

    virtual void Print(FILE *out) {
      int bytes=0;
      fprintf(out, "Branch Separator stats\n");
      fprintf(out, "  # Branches: %d\n",
              num_branches_);
      fprintf(out, "  # Branches realigned : %d\n",
              num_branches_realigned_);
      for(int i=0; i<distance_; i++) {
        if (nop_counts_[i] != 0) {
          fprintf(out, "  # %d byte nops inserted: %d\n",
                i, nop_counts_[i]);
          bytes += i*nop_counts_[i];
        }
      }
      fprintf(out, "  # additional bytes: %d\n",
              bytes);
      fprintf(out, "  # Relaxations: %d\n",
              relaxations_);
    }

   private:
    int                num_branches_;
    int                num_branches_realigned_;
    int                relaxations_;
    int                *nop_counts_;
    int                distance_;
  };

  BranchSeparatorStat *branch_separator_stat_;

  bool IsCondBranch (MaoEntry *);
  void InsertNopsBefore (Function *, MaoEntry *, int n);
  void AlignEntry(Function *, MaoEntry *, bool);
  bool IsProfitable (Function *);

};


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(BRSEP, "Separate branches to avoid BTB interference and "\
                   "other microarchitectural effects", 4) {
  OPTION_INT("min_branch_distance", 16, "Minimum distance required between "
                              "any two branches"),
  OPTION_BOOL("collect_stats", false, "Collect and print a table with "
                                      "statistics about all processed loops."),
  OPTION_BOOL("last_byte", false, "Align based on the "
                             "last byte of the branch"),
  OPTION_STR("function_list", "",
             "A comma separated list of mangled function names"
             " on which this pass is applied."
             " An empty string means the pass is applied on all functions"),
};

BranchSeparatorPass::BranchSeparatorPass(MaoOptionMap *options, MaoUnit *mao,
                                         Function *function)
    : MaoFunctionPass("BRSEP", options, mao, function), sizes_(NULL) {

  collect_stat_      = GetOptionBool("collect_stats");
  last_byte_= GetOptionBool("last_byte");
  min_branch_distance_ = GetOptionInt("min_branch_distance");
  profitable = IsProfitable (function);
  Trace(2, "Mao branch separator");

  if (collect_stat_) {
    // check if a stat object already exists?
    if (unit_->GetStats()->HasStat("BRSEP")) {
      branch_separator_stat_ = static_cast<BranchSeparatorStat *>(
          unit_->GetStats()->GetStat("BRSEP"));
    } else {
      branch_separator_stat_ = new BranchSeparatorStat(min_branch_distance_);
      unit_->GetStats()->Add("BRSEP", branch_separator_stat_);
    }
  }
}

bool BranchSeparatorPass::Go() {
  if(!profitable)
    return true;
  sizes_ = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());


  MAO_ASSERT(function_->GetSection());
  // Build an offset map fromt he size map we have.
  // TODO(martint): Optimize the code so that the map is not
  // rebuild for each function.
  int offset = 0;
  int prev_branch_offset;
  prev_branch_offset = -1*min_branch_distance_;
  bool change = false, rerelax=false;
  int alignment = (int)(log2(min_branch_distance_));
  char prev_branch_str[1024];

  for (EntryIterator iter = function_->EntryBegin();
       iter != function_->EntryEnd();
       ++iter) {
    int size = (*sizes_)[*iter];

    if (IsCondBranch(*iter)) {
      if (collect_stat_)
        branch_separator_stat_->FoundBranch();
      std::string op_str;
      (*iter)->ToString(&op_str);
      Trace(2, "Found branch  : %s", op_str.c_str() );
      if( (!last_byte_ && (offset>>alignment) == (prev_branch_offset>>alignment)) ||
          (last_byte_ && ((offset+size-1)>>alignment == (prev_branch_offset>>alignment)))) {
        int num_nops = min_branch_distance_ - (offset - prev_branch_offset);
        if(collect_stat_)
          branch_separator_stat_->RealigningBranch(num_nops);
        Trace(2, "Inserting %d nops between \"%s\" and \"%s\"", num_nops,  prev_branch_str, op_str.c_str());
        bool insert_jump = num_nops >= (FETCH_LINE_SIZE + 2);
        AlignEntry(function_, *iter, insert_jump);
        offset += num_nops;
        change = true;
        rerelax = true;
      }
      prev_branch_offset = offset;
      strcpy (prev_branch_str, op_str.c_str());
      if (last_byte_)
        prev_branch_offset += size-1;
    }
    else if ((*iter)->IsDirective()) {
      //If we have separated a branch and then we see a directive,
      //we need to re-relax 
      if (rerelax) {
        MaoRelaxer::InvalidateSizeMap(function_->GetSection());
        sizes_ = MaoRelaxer::GetSizeMap(unit_, function_->GetSection());
        Trace (2, "Re-relaxing");
        rerelax=false;
        if (collect_stat_)
          branch_separator_stat_->Relaxed();
      }
    }
    offset += size;
  }
  if(change) {
    if(rerelax)
      //Relaxation has to be performed again
      MaoRelaxer::InvalidateSizeMap(function_->GetSection());
    //Align function begining based on min_branch_distance
    AlignEntry(function_, *(function_->EntryBegin()), false);
    if (collect_stat_)
      branch_separator_stat_->Relaxed();
  }
  return true;
}

//Use .p2align
void BranchSeparatorPass::AlignEntry(Function *function, MaoEntry *entry, bool insert_jump=false) {
  SubSection *ss = function->GetSubSection();
  DirectiveEntry::OperandVector operands;
  int alignment = (int)(log2(min_branch_distance_));
  operands.push_back(new DirectiveEntry::Operand(alignment));
  operands.push_back(new DirectiveEntry::Operand());// Not used in relaxation
  operands.push_back(new DirectiveEntry::Operand(min_branch_distance_-1));
  DirectiveEntry *align_entry = unit_->CreateDirective(
      DirectiveEntry::P2ALIGN, operands,
      function, ss);
  if (insert_jump) {
    //Create a new label

    LabelEntry *label_entry = unit_->CreateLabel(
            MaoUnit::BBNameGen::GetUniqueName(),
            function,
            ss
            );
    Trace (2, "Created new label: %s\n", label_entry->name());
    InstructionEntry *jump_entry = unit_->CreateUncondJump(label_entry, function);
    std::string op_str;
    jump_entry->ToString(&op_str);
    Trace (2, "Created new jump instruction: %s\n", op_str.c_str());

    entry->LinkBefore(label_entry);
    label_entry->LinkBefore(jump_entry);
    label_entry->LinkBefore(align_entry);
  }
  else
    entry->LinkBefore(align_entry);

}
//Use .p2align
void BranchSeparatorPass::InsertNopsBefore (Function *function,
                                            MaoEntry *entry, int num_nops) {

  SubSection *ss = function_->GetSubSection();
  DirectiveEntry::OperandVector operands;
  int alignment = (int)(log2(min_branch_distance_));
  operands.push_back(new DirectiveEntry::Operand(alignment));
  operands.push_back(new DirectiveEntry::Operand());// Not used in relaxation
  operands.push_back(new DirectiveEntry::Operand(num_nops));
  DirectiveEntry *align_entry = unit_->CreateDirective(
      DirectiveEntry::P2ALIGN, operands,
      function_, ss);
  entry->LinkBefore(align_entry);
}

bool BranchSeparatorPass::IsCondBranch (MaoEntry *entry) {
  if (!entry->IsInstruction())
    return false;
  InstructionEntry *ins_entry = entry->AsInstruction();
  return ins_entry->IsCondJump();
}

// Is the transformation profitable for this function
// Right now it checks a list of function names passed as
// a parameter to deciede if the function is profitable or
// not
bool BranchSeparatorPass::IsProfitable(Function *function) {
  //List of comma separated functions to apply this pass
  const char *function_list;
  function_list = GetOptionString("function_list");
  const char *func_name = (function->name()).c_str();
  Trace(2, "Function name = %s\n", func_name);
  const char *comma_pos;


  if(strcmp (function_list, ""))
    {
      while ( (comma_pos = strchr (function_list, ';')) != NULL) {
        int len = comma_pos - function_list;
        if(strncmp(func_name, function_list, len) == 0)
          return true;
        function_list = comma_pos+1;
      }
      //The function list might be just a single function name and so
      //we need to compare the list against the given function name
      if( strcmp (function_list, func_name) == 0 ) {
        Trace(2, "Profitable");
        return true;
      }
      Trace(2, "Not Profitable");
      return false;
    }
  else
    {
      return true;
    }
}

REGISTER_PLUGIN_FUNC_PASS("BRSEP", BranchSeparatorPass)
}  // namespace
