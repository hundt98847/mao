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
#include <map>
#include <math.h>


#include "MaoBranchSeparator.h"


// Pass that finds paths in inner loops that fits in 4 16-byte lines
// and alligns all (chains of) basicb locks within the paths.
class BranchSeparatorPass : public MaoPass {
 public:
  explicit BranchSeparatorPass(MaoUnit *mao, Function *function);
  void DoBranchSeparate();

 private:
  // Path is a list of basic blocks that form a way through the loop
//   typedef std::vector<BasicBlock *> Path;
//   typedef std::vector<Path *> Paths;

  MaoUnit  *mao_unit_;

  Function *function_;
  // This is the sizes of all the instructions in the section
  // as found by the relaxer.
  MaoEntryIntMap *sizes_;
  // Collect stat during the pass?
  bool collect_stat_;
  //Required distance between branches
  int min_branch_distance_;

  bool profitable;

  class BranchSeparatorStat : public Stat {
   public:
    BranchSeparatorStat() : num_branches_(0), num_branches_realigned_(0),
                            nops_inserted_(0) {
    }

    ~BranchSeparatorStat() {;}

    void FoundBranch() {
      num_branches_++;
    }

    void RealigningBranch(int nops) {
      num_branches_realigned_++;
      nops_inserted_+=nops;
    }

    virtual void Print(FILE *out) {
      fprintf(out, "Branch Separator stats\n");
      fprintf(out, "  # Branches: %d\n",
              num_branches_);
      fprintf(out, "  # Branches realigned : %d\n",
              num_branches_realigned_);
      fprintf(out, "  # Nops inserted : %d\n",
              nops_inserted_);
    }

   private:
    int                num_branches_;
    int                num_branches_realigned_;
    int                nops_inserted_;
  };

  BranchSeparatorStat *branch_separator_stat_;

  bool IsBranch (MaoEntry *);
  void InsertNopsBefore (Function *, MaoEntry *, int n);
  void AlignEntry(Function *, MaoEntry *);
  bool IsProfitable (Function *);

};


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(BRSEP, 3) {
  OPTION_INT("min_branch_distance", 32, "Minimum distance required between "
                              "any two branches"),
  OPTION_BOOL("stat", false, "Collect and print(trace) "
                             "statistics about loops."),
  OPTION_STR("function_list", "",
             "A comma separated list of mangled function names"
             " on which this pass is applied."
             " An empty string means the pass is applied on all functions"),
};

BranchSeparatorPass::BranchSeparatorPass(MaoUnit *mao, Function *function)
    : MaoPass("BRSEP", mao->mao_options(), MAO_OPTIONS(BRSEP), false),
      mao_unit_(mao), function_(function), sizes_(NULL) {

  collect_stat_      = GetOptionBool("stat");
  min_branch_distance_ = GetOptionInt("min_branch_distance");
  profitable = IsProfitable (function);
  Trace(2, "Mao branch separator");

  if (collect_stat_) {
    // check if a stat object already exists?
    if (mao_unit_->GetStats()->HasStat("BRSEP")) {
      branch_separator_stat_ = static_cast<BranchSeparatorStat *>(
          mao_unit_->GetStats()->GetStat("BRSEP"));
    } else {
      branch_separator_stat_ = new BranchSeparatorStat();
      mao_unit_->GetStats()->Add("BRSEP", branch_separator_stat_);
    }
  }
}

void BranchSeparatorPass::DoBranchSeparate() {
  if(!profitable)
    return;
  sizes_ = MaoRelaxer::GetSizeMap(mao_unit_, function_->GetSection());


  MAO_ASSERT(function_->GetSection());
  // Build an offset map fromt he size map we have.
  // TODO(martint): Optimize the code so that the map is not
  // rebuild for each function.
  int offset = 0;
  int prev_branch_offset;
  prev_branch_offset = -1*min_branch_distance_;
  bool change = false;

  for (SectionEntryIterator iter = function_->EntryBegin();
       iter != function_->EntryEnd();
       ++iter) {
    if (IsBranch(*iter)) {
      if(collect_stat_)
        branch_separator_stat_->FoundBranch();
      Trace(2, "Found branch  " );
      if(offset - prev_branch_offset < min_branch_distance_) {
        int num_nops = min_branch_distance_ - (offset - prev_branch_offset);
        if(collect_stat_)
          branch_separator_stat_->RealigningBranch(num_nops);
        Trace(2, "Inserting %d nops ", num_nops );
        AlignEntry(function_, *iter);
        offset += num_nops;
        change = true;
      }
      prev_branch_offset = offset;
    }
    offset += (*sizes_)[*iter];
  }
  if(change) {
    //Relaxation has to be performed again
    MaoRelaxer::InvalidateSizeMap(function_->GetSection());
    //Align function begining based on min_branch_distance
    AlignEntry(function_, *(function_->EntryBegin()));
  }
  return;
}

//Use .p2align
void BranchSeparatorPass::AlignEntry(Function *function, MaoEntry *entry) {
  SubSection *ss = function_->GetSubSection();
  DirectiveEntry::OperandVector operands;
  int alignment = (int)(log2(min_branch_distance_));
  operands.push_back(new DirectiveEntry::Operand(alignment));
  operands.push_back(new DirectiveEntry::Operand());// Not used in relaxation
  operands.push_back(new DirectiveEntry::Operand(min_branch_distance_-1));
  DirectiveEntry *align_entry = mao_unit_->CreateDirective(
      DirectiveEntry::P2ALIGN, operands,
      function_, ss);
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
  DirectiveEntry *align_entry = mao_unit_->CreateDirective(
      DirectiveEntry::P2ALIGN, operands,
      function_, ss);
  entry->LinkBefore(align_entry);

}
bool BranchSeparatorPass::IsBranch (MaoEntry *entry) {
  if (!entry->IsInstruction())
    return false;
  InstructionEntry *ins_entry = entry->AsInstruction();
  return ins_entry->HasTarget();

}


// Is the transformation profitable for this function
// Right now it checks a list of function names passed as
// a parameter to deciede if the function is profitable or
// not
bool BranchSeparatorPass::IsProfitable(Function *function){
  //List of comma separated functions to apply this pass
  const char *function_list;
  function_list = GetOptionString("function_list");
  const char *func_name = (function->name()).c_str();
  Trace(2, "Function name = %s\n", func_name);
  const char *comma_pos;


  Trace(2, "Function list = %s\n", function_list);
  if(strcmp (function_list, ""))
    {
      while ( (comma_pos = strchr (function_list, ',')) != NULL) {
        Trace(2, "Function list = %s\n", function_list);
        int len = comma_pos - function_list;
        if(strncmp(func_name, function_list, len) == 0)
          return true;
        function_list = comma_pos+1;
      }
      //The function list might be just a single function name and so
      //we need to compare the list against the given function name
      if( strcmp (function_list, func_name) == 0 )
        return true;
      return false;
    }
  else
    {
      return true;
    }
}

// External Entry Point
void DoBranchSeparate(MaoUnit *mao,
                 Function *function) {
  // Make sure the analysis have been run on this function
  BranchSeparatorPass separator(mao,
                      function);
  if (separator.enabled()) {
    separator.set_timed();
    separator.DoBranchSeparate();
  }
  return;
}
