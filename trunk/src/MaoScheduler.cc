//
// Copyright 2008 Google Inc.
//
// This program is free software; you can redistribute it and/or to
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
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

// Scheduler that minimizes effects such as reservation station bottlenecks
//
#include "MaoDebug.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
#include "MaoUtil.h"

#define LCD_HEIGHT_ADJUSTMENT 10
#define HOT_REGISTER_BONUS 1

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(SCHEDULER, 4) {
  OPTION_STR("function_list", "",
             "A comma separated list of mangled function names"
             " on which this pass is applied."
             " An empty string means the pass is applied on all functions"),
  OPTION_STR("functions_file", "",
             " "),
  OPTION_INT("start_func", 0,
             "Number of the first function that is optimized"),
  OPTION_INT("end_func", 1000000000,
             "Number of the last  function that is optimized"),
};

#define MAX_REGS 256

#define NO_DEP 0
#define TRUE_DEP 1
#define OUTPUT_DEP 2
#define ANTI_DEP 4
#define MEM_DEP 8
#define CTRL_DEP 16
#define ALL_DEPS (~NO_DEP)
class SchedulerPass : public MaoFunctionPass {
 public:
  class DependenceDag {
   public:
    DependenceDag (int num_instructions, std::string *insn_str) {
      dag_insn_str_ = insn_str;
      num_instructions_ = num_instructions;
      adj_matrix_ = new char[num_instructions*num_instructions];
      memset (adj_matrix_, 0, num_instructions*num_instructions);
    }

    ~DependenceDag () {
      delete [] adj_matrix_;
      delete [] dag_insn_str_;
    }

    void AddEdge (int u, int v, int type) {
      adj_matrix_[u*num_instructions_+v] |= type;
    }

    inline char GetEdge (int u, int v) {
      return adj_matrix_[u*num_instructions_+v];
    }

    void GetPredEdges (int u, std::list<int> *edges) {
      for (int i=0; i<num_instructions_; i++) {
        if (adj_matrix_[i*num_instructions_+u])
          edges->push_back(i);
      }
    }

    void GetSuccEdges (int u, std::list<int> *edges) {
      for (int i=0; i<num_instructions_; i++) {
        if (adj_matrix_[u*num_instructions_+i])
          edges->push_back(i);
      }
    }

    int NodeCount () {
      return num_instructions_;
    }

    std::string * GetInstructionStrings () {
      return dag_insn_str_;
    }


    void Print (FILE *file) {
      fprintf (file, "#instructions = %d\n", num_instructions_);
      for (int i=0; i<num_instructions_; i++) {
        fprintf (file, "(%d) %s -> ", i, dag_insn_str_[i].c_str());
        for (int j=0; j<num_instructions_; j++) {
          if(adj_matrix_[i*num_instructions_ + j] != NO_DEP)
            fprintf (file, "(%d) %s[%d],  ", j, dag_insn_str_[j].c_str(), adj_matrix_[i*num_instructions_ + j]);
        }
        fprintf (file, "\n");
      }
    }


    std::list<int>* GetExits (int edge_mask=ALL_DEPS) {
      std::list<int>* exits= new std::list<int>;
      for (int i=0; i<num_instructions_; i++) {
        if (NumSuccessors (i, edge_mask) ==0)
          exits->push_back (i);
      }
      return exits;
    }

    std::list<int>* GetEntries (int edge_mask=ALL_DEPS) {
      std::list<int>* entries= new std::list<int>;
      for (int i=0; i<num_instructions_; i++) {
        if (NumPredecessors (i, edge_mask) ==0)
          entries->push_back (i);
      }
      return entries;
    }

    std::list<int>* GetSuccessors (int node, int edge_mask=ALL_DEPS) {
      std::list<int>* successors = new std::list<int>;
      for (int i=0; i<num_instructions_; i++) {
        if (adj_matrix_[node*num_instructions_+i]&edge_mask)
          successors->push_back (i);
      }
      return successors;
    }

    std::list<int>* GetPredecessors (int node, int edge_mask=ALL_DEPS) {
      std::list<int>* predecessors = new std::list<int>;
      for (int i=0; i<num_instructions_; i++) {
        if (adj_matrix_[i*num_instructions_+node]&edge_mask)
          predecessors->push_back (i);
      }
      return predecessors;
    }

    int NumSuccessors (int node, int edge_mask=ALL_DEPS) {
      int num_successors = 0;
      for (int i=0; i<num_instructions_; i++) {
        if (adj_matrix_[node*num_instructions_+i]&edge_mask)
          num_successors++;
      }
      return num_successors;
    }

   int NumPredecessors (int node, int edge_mask=ALL_DEPS) {
      int  num_predecessors = 0;
      for (int i=0; i<num_instructions_; i++) {
        if (adj_matrix_[i*num_instructions_+node]&edge_mask)
          num_predecessors++;
      }
      return num_predecessors;
    }

   private:
    int num_instructions_;
    char *adj_matrix_;
    std::string *dag_insn_str_;
  };

  SchedulerPass(MaoOptionMap *options, MaoUnit *mao, Function *func)
    : MaoFunctionPass("SCHEDULER", options, mao, func) {

      profitable_ = IsProfitable (func);
  }

  bool Go() {
    int start_func = GetOptionInt("start_func");
    int end_func = GetOptionInt("end_func");
    const char* functions_file = GetOptionString("functions_file");
    FILE *fp = NULL;
    if (strcmp(functions_file, "")) {
      fp= fopen (functions_file, "r");
      const char *this_func_name = (function_->name()).c_str();
      char line[4096];
      int func_num=0;
      while (!feof(fp)) {
        fscanf (fp, "%s", line);
        if (!strcmp(line, this_func_name))
          break;
        func_num++;
      }
      fclose(fp);
      Trace (0, "Function %d: %s", func_num, this_func_name);


      if (func_num < start_func ||
          func_num > end_func)
        return true;
    }
    if (!profitable_)
      return true;
    CFG *cfg = CFG::GetCFG (unit_, function_);
    FindBBsInStraightLineLoops ();
    FORALL_CFG_BB(cfg, bb_iterator) {
      MaoEntry *first = *((*bb_iterator)->EntryBegin());
      MaoEntry *last = *((*bb_iterator)->EntryEnd());
      std::string first_str, last_str;
      lock_set_.clear();
      if (first)
        first->ToString(&first_str);
      if (last)
        last->ToString(&last_str);
      Trace (2, "BB start = %s, BB end = %s", first_str.c_str(), last_str.c_str());
      DependenceDag *dag = FormDependenceDag(*bb_iterator);
      if (dag != NULL) {
        Trace (2, "Dag for new bb:");
        if (tracing_level() >= 2)
          dag->Print(stderr);
        int *dependence_heights = ComputeDependenceHeights(dag);
        for (int i=0; i<dag->NodeCount(); i++) {
          Trace (2, "%s: %d", insn_str_[i].c_str(), dependence_heights[i]);
        }
        MaoEntry *head = (*bb_iterator)->first_entry();
        if (head->IsInstruction())
          head = head->prev();
        else {
          while (!head->next()->IsInstruction())
            head = head->next();
        }

        MaoEntry *last_entry = Schedule (dag, dependence_heights, head);
        PrefixLocks (head->next(), last_entry);
      }

    }


    return true;
  }

 private:
  std::map<std::string, int, ltstr> insn_map_;
  std::set<MaoEntry *> lock_set_;
  std::string *insn_str_;
  MaoEntry **entries_;
  bool profitable_;
  static int function_count_;
  std::set<BasicBlock *> bbs_in_stline_loops_;

  //An array used to keep track of instructions that
  //are sources of some loop carried dependence
  char *is_lcd_source_;

  BitString GetSrcRegisters (InstructionEntry *insn);
  BitString GetDestRegisters (InstructionEntry *insn);
  DependenceDag *FormDependenceDag(BasicBlock *bb);
  bool CanReorder (InstructionEntry *entry);
  bool IsMemOperation (InstructionEntry *entry);
  bool IsControlOperation (InstructionEntry *entry);
  bool IsLock (InstructionEntry *entry);
  int *ComputeDependenceHeights (DependenceDag *dag);
  int RemoveTallest (std::list<int> *list, int *heights);
  void ScheduleNode (int node, MaoEntry **head);
  MaoEntry* Schedule (DependenceDag *dag, int *dependence_heights, MaoEntry *head);
  void PrefixLocks (MaoEntry *first, MaoEntry* last);
  bool IsProfitable (Function *);
  void FindBBsInStraightLineLoops();
  void FindBBsInStraightLineLoops(SimpleLoop *loop);
  void RemoveLocks (std::vector<MaoEntry*> *locks, BasicBlock *bb);
  void InitializeLastWriter (BasicBlock *bb, int *last_writer);



};

int SchedulerPass::function_count_ = 0;


void SchedulerPass::FindBBsInStraightLineLoops () {
  LoopStructureGraph *loop_graph =  LoopStructureGraph::GetLSG(unit_, function_);
  SimpleLoop *root = loop_graph->root();
  FindBBsInStraightLineLoops(root);
}

void SchedulerPass::FindBBsInStraightLineLoops ( SimpleLoop *loop) {
  if (loop->header() == loop->bottom() && loop->header() != NULL) {
    bbs_in_stline_loops_.insert(loop->header());
    //This must be an innermost loop
    return;
  }
  //Recurse
  for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
       liter != loop->GetChildren()->end(); ++liter)
    FindBBsInStraightLineLoops (*liter);

}

MaoEntry* SchedulerPass::Schedule (DependenceDag *dag, int *dependence_heights, MaoEntry *head) {
  char *scheduled = new char[dag->NodeCount()];
  memset (scheduled, 0, dag->NodeCount());
  //Get instructions that are ready to be scheduled
  std::list<int> *ready = dag->GetEntries();

  //Schedule the available instruction with the maximum height
  //as the first instruction of the function
  MaoEntry *last_entry=NULL;
  while (!ready->empty()) {
    int node = RemoveTallest (ready, dependence_heights);
    ScheduleNode (node, &head);
    scheduled[node]=1;
    last_entry = entries_[node];
    //Schedule the successors depthwise till no further scheduling
    //can be done
    std::list<int> *successors = dag->GetSuccessors(node);
    //Find that successor with the largest dependence height, all of
    //whose predecessors are already scheduled
    int best_height = -1, best_node = -1;
    for (std::list<int>::iterator succ_iter = successors->begin();
         succ_iter != successors->end(); ++succ_iter) {
      int succ = *succ_iter;
      std::list<int> *predecessors = dag->GetPredecessors(succ);
      bool can_schedule=true;
      for (std::list<int>::iterator pred_iter = predecessors->begin();
           pred_iter != predecessors->end(); ++pred_iter) {
        int pred = *pred_iter;
        if(!scheduled[pred]) {
          Trace (2, "Predecessor  %s of %s not scheduled ", insn_str_[pred].c_str(), insn_str_[succ].c_str());
          can_schedule=false;
          break;
        }
      }
      if (can_schedule) {
        ready->push_back(succ);
        Trace (2, "Adding successor node (%d) %s  with dep %d and height %d to the ready queue", succ, insn_str_[succ].c_str(), dag->GetEdge(node, succ), dependence_heights[succ]);
        if (!IsMemOperation(entries_[node]->AsInstruction()) &&
            (dag->GetEdge(node, succ) & TRUE_DEP) ) {
          dependence_heights[succ] += HOT_REGISTER_BONUS;
          Trace (2, "New best node. Height = %d", best_height);
        }
      }
    }

  }
  return last_entry;
}

void SchedulerPass::PrefixLocks (MaoEntry *first, MaoEntry *last) {
  for (MaoEntry *entry = first;
       entry != last->next(); entry = entry->next()) {
    if (lock_set_.find(entry) != lock_set_.end())
      entry->LinkBefore(unit_->CreateLock(function_));
  }
}
void SchedulerPass::ScheduleNode (int node, MaoEntry **head) {
  MaoEntry *entry = entries_[node];
  //Don't do anything if the node to be scheduled is the head node
  if (entry == *head)
    return;
  MaoEntry *prev_entry = entry->prev();
  entry->Unlink();
  (*head)->LinkAfter(entry);
  if (prev_entry->IsDirective()) {
    DirectiveEntry *insn = prev_entry->AsDirective();
    if (insn->op() == DirectiveEntry::P2ALIGN ||
        insn->op() == DirectiveEntry::P2ALIGNW ||
        insn->op() == DirectiveEntry::P2ALIGNL) {
        prev_entry->Unlink();
        entry->LinkBefore (prev_entry);
    }
  }
  std::string str1, str2;
  (*head)->ToString (&str1);
  entry->ToString (&str2);
  Trace (2, "Scheduling (%d) %s after %s", node, str2.c_str(), str1.c_str());
  *head = entry;
}

int SchedulerPass::RemoveTallest (std::list<int> *list, int *heights) {
  int best_height = -1, best_node = -1;
  for (std::list<int>::iterator iter = list->begin();
       iter != list->end(); ++iter) {
    int node = *iter;
    if (heights[node] > best_height) {
      best_height = heights[node];
      best_node = node;
    }
    else if ( (heights[node] == best_height) &&
              (node < best_node)) {
      best_height = heights[node];
      best_node = node;
    }
  }
  list->remove (best_node);
  return best_node;
}

int *SchedulerPass::ComputeDependenceHeights (DependenceDag *dag) {
  int *heights, *visited;
  std::list<int> *work_list = dag->GetExits(TRUE_DEP|MEM_DEP);
  std::list<int> *new_work_list = new std::list<int>;
  heights = new int[dag->NodeCount()];
  visited = new int[dag->NodeCount()];
  memset (heights, 0xFF, dag->NodeCount()*sizeof(int));
  memset (visited, 0, dag->NodeCount()*sizeof(int));

  while (!work_list->empty()) {
    for (std::list<int>::iterator iter = work_list->begin();
         iter != work_list->end(); ++iter) {
      int height=0;
      int node = *iter;
      bool reprocess = false;
      std::list<int> *succ_nodes = dag->GetSuccessors (node, TRUE_DEP|MEM_DEP);
      for (std::list<int>::iterator succ_iter = succ_nodes->begin();
           succ_iter != succ_nodes->end(); ++succ_iter) {
        int succ_node = *succ_iter;
        int succ_height = heights[succ_node];
        if (succ_height == -1) {
          reprocess = true;
          break;
        }
        if (succ_height+1 > height)
          height = succ_height+1;
      }
      delete succ_nodes;

      if (reprocess)
        new_work_list->push_back (node);
      else {
        //If the node is the exit of the dag, but there is a loop carried
        //dependence originating from it, set the height to LCD_HEIGHT_ADJUSTMENT
        //
        heights[node] = height;

        std::list<int> *pred_nodes = dag->GetPredecessors (node, TRUE_DEP|MEM_DEP);
        for (std::list<int>::iterator pred_iter = pred_nodes->begin();
             pred_iter != pred_nodes->end(); ++pred_iter) {
          int pred_node = *pred_iter;
          if (!visited[pred_node]) {
            new_work_list->push_back (pred_node);
            visited[pred_node] = 1;
          }
        }
        delete pred_nodes;

      }
    }
    delete work_list;
    work_list = new_work_list;
    new_work_list =  new std::list<int>;
  }
  for (int i=0; i<dag->NodeCount(); i++)
    if (is_lcd_source_[i])
      heights[i] += LCD_HEIGHT_ADJUSTMENT;

  delete work_list;
  delete new_work_list;
  delete [] is_lcd_source_;
  is_lcd_source_ = NULL;
  return heights;


}

BitString SchedulerPass::GetSrcRegisters (InstructionEntry *insn) {
  BitString use_mask = GetRegisterUseMask(insn);

  return use_mask;
  /*
  int operands = insn->NumOperands();
  BitString mask;
  for (int i = 0; i < operands; i++) {
    if (!insn->IsRegisterOperand (i))
      continue;
    const reg_entry *reg = insn->GetRegisterOperand(i);
    mask = mask | GetMaskForRegister(reg->reg_name);
  }
  if (insn->HasBaseRegister()) {
    const reg_entry *reg = insn->GetBaseRegister();
    mask = mask | GetMaskForRegister(reg->reg_name);
  }
  if (insn->HasIndexRegister()) {
    const reg_entry *reg = insn->GetIndexRegister();
    mask = mask | GetMaskForRegister(reg->reg_name);
  }
  return mask;
  */
}

BitString SchedulerPass::GetDestRegisters (InstructionEntry *insn) {
  BitString def_mask = GetRegisterDefMask(insn);
  return def_mask;
}

void SchedulerPass::RemoveLocks (std::vector<MaoEntry*> *locks, BasicBlock *bb) {
  //Remove any lock instructions in the original code
  //lock acts more like a prefix in the sense that it applies to the
  //immediately following isntruction. However, it has a separate entry
  //in the code. To enforce the fact that the lock and the following
  //instruction have to be adjacent even after the scheduling, the
  //instructions following a lock are added to a separate set and the
  //lock is removed from the original stream. During scheduling if the
  //instruction is in lock_set_, a new lock instruction is created and
  //prefixed to it.
  for (std::vector<MaoEntry*>::iterator lock_iter=locks->begin();
       lock_iter != locks->end(); ++lock_iter) {
    MaoEntry *entry=*lock_iter;
    if (entry == bb->first_entry())
      bb->set_first_entry (entry->next());
    entry->Unlink();
  }
}

void SchedulerPass::InitializeLastWriter (BasicBlock *bb, int *last_writer) {
  int insns_in_bb = 0;
  for(SectionEntryIterator entry_iter = bb->EntryBegin();
      entry_iter != bb->EntryEnd(); ++entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (!entry->IsInstruction())
      continue;
    InstructionEntry *insn = entry->AsInstruction();

    BitString dest_regs_mask = GetDestRegisters (insn);
    /*Trace (2, "Dest  registers: ");
    dest_regs_mask.Print();*/

    int index=0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "dest reg = %d", index);
      last_writer[index]=insns_in_bb;
      index++;
    }
    insns_in_bb++;
  }
}

SchedulerPass::DependenceDag *SchedulerPass::FormDependenceDag (BasicBlock *bb) {
  int last_writer[MAX_REGS];
  std::vector<int> writers[MAX_REGS];
  int insns_in_bb = 0;
  bool lock_next=false;
  std::vector<MaoEntry*> locks;
  for(SectionEntryIterator entry_iter = bb->EntryBegin();
      entry_iter != bb->EntryEnd(); ++entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (!entry->IsInstruction())
      continue;
    InstructionEntry *insn = entry->AsInstruction();
    if (IsLock(insn)) {
      lock_next=true;
      locks.push_back(insn);
      continue;
    }
    if (lock_next)
      lock_set_.insert (insn);
    lock_next=false;
    insns_in_bb++;
  }
  if (insns_in_bb == 0)
    return NULL;
  RemoveLocks (&locks, bb);

  insn_str_ = new std::string[insns_in_bb];
  entries_ = new MaoEntry*[insns_in_bb];
  is_lcd_source_ = new char[insns_in_bb];
  memset (is_lcd_source_, 0, insns_in_bb*sizeof(char));

  DependenceDag *dag = new DependenceDag(insns_in_bb, insn_str_);
  insns_in_bb = 0;
  memset (last_writer,0xFF, MAX_REGS*sizeof(int));
  int prev_mem_operation = -1;
  std::vector<int> ctrl_dep_sources;
  if (bbs_in_stline_loops_.find(bb) != bbs_in_stline_loops_.end()) {
    //This BB forms a straightline loop
    InitializeLastWriter (bb, last_writer);
  }
  BitString rsp_mask = GetMaskForRegister("rsp");

  for(SectionEntryIterator entry_iter = bb->EntryBegin();
      entry_iter != bb->EntryEnd(); ++entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (!entry->IsInstruction())
      continue;
    InstructionEntry *insn = entry->AsInstruction();
    entries_[insns_in_bb] = insn;
    insn->ToString(&insn_str_[insns_in_bb]);
    Trace (2, "Instruction %d: %s\n", insns_in_bb, insn_str_[insns_in_bb].c_str());
    BitString src_regs_mask = GetSrcRegisters (insn);
    BitString dest_regs_mask = GetDestRegisters (insn);
    char src_str[128], dest_str[128];
    src_regs_mask.ToString (src_str);
    dest_regs_mask.ToString (dest_str);

    Trace (4, "Src registers: %s", src_str);
    Trace (4, "Dest  registers: %s", dest_str);

    //An instruction that modifies SP acts as a barrier for stack-relative
    //memory operations. Here, we are being conservative by preventing
    //reordering of other memory access operations around stack relative
    //accesses
    //
    if (IsMemOperation(insn) || (dest_regs_mask & rsp_mask).IsNonNull()) {
      if (prev_mem_operation != -1)
        dag->AddEdge (prev_mem_operation, insns_in_bb, MEM_DEP);
      prev_mem_operation = insns_in_bb;
    }
    if (IsControlOperation(insn)) {
      for (std::vector<int>::iterator src_iter = ctrl_dep_sources.begin();
           src_iter != ctrl_dep_sources.end(); ++src_iter)
        dag->AddEdge (*src_iter, insns_in_bb, CTRL_DEP);
      ctrl_dep_sources.clear();
    }
    ctrl_dep_sources.push_back (insns_in_bb);
    insn_map_[insn_str_[insns_in_bb]] = insns_in_bb;




    int index=0;
    while ((index = src_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "src reg = %d", index);
      if (last_writer[index] >=0 && last_writer[index] < insns_in_bb) {
        dag->AddEdge (last_writer[index], insns_in_bb, TRUE_DEP);
        // When an instruction uses a register, we know that the value written
        // by the last writer to that register is live. Now we can create
        // WAW dependences  from all prior writers to that register to the
        // last writer. It is unnecessary to create WAW dependences between
        // all instructions that write to a register since it severely limits
        // scheduling freedom especially due to the presence of eflags
        for (std::vector<int>::iterator w_iter = writers[index].begin();
             w_iter != writers[index].end(); ++w_iter) {
          int writer = *w_iter;
          if (writer != last_writer[index])
            dag->AddEdge (writer, last_writer[index], OUTPUT_DEP);
        }
        writers[index].clear();
        writers[index].push_back (last_writer[index]);
      }
      else if (last_writer[index] >= insns_in_bb)
        is_lcd_source_[last_writer[index]] = 1;
      index++;
    }

    index=0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "dest reg = %d", index);
      last_writer[index]=insns_in_bb;
      writers[index].push_back (insns_in_bb);
      index++;
    }

    insns_in_bb++;

  }
  // There may be multiple definitions of a reg with no uses
  //Created WAW dependences to the last writer in that case
  for (int i=0; i<MAX_REGS; i++) {
      if (last_writer[i] >=0) {
        for (std::vector<int>::iterator w_iter = writers[i].begin();
             w_iter != writers[i].end(); ++w_iter) {
          int writer = *w_iter;
          if (writer != last_writer[i])
            dag->AddEdge (writer, last_writer[i], OUTPUT_DEP);
        }
      }
      writers[i].clear();
  }
  SectionEntryIterator last_entry = bb->EntryEnd();
  SectionEntryIterator first_entry = bb->EntryBegin();
  if (*first_entry == NULL || *last_entry == NULL)
    return dag;
  --first_entry;
  --last_entry;

  //WAR dependences
  //Trace (2, "Reverse traversal for WAR deps");
  memset (last_writer,0xFF, MAX_REGS*sizeof(int));
  std::string insn_str;
  for (SectionEntryIterator entry_iter = last_entry;
           *entry_iter != *first_entry; --entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (!entry->IsInstruction())
      continue;
    InstructionEntry *insn = entry->AsInstruction();
    insns_in_bb--;
    BitString src_regs_mask = GetSrcRegisters (insn);
    BitString dest_regs_mask = GetDestRegisters (insn);
    int index=0;
    insn->ToString(&insn_str);
    //Trace (2, "Instruction %d: %s\n", insns_in_bb, insn_str.c_str());
    while ((index = src_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "src reg = %d", index);
      for (std::vector<int>::iterator wr_iter = writers[index].begin();
           wr_iter != writers[index].end(); ++wr_iter)
        dag->AddEdge (insns_in_bb, *wr_iter, ANTI_DEP);
      index++;
    }

    index=0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "dest reg = %d", index);
      last_writer[index]=insns_in_bb;
      writers[index].push_back (insns_in_bb);
      index++;
    }
    insn_str.erase();
    //Trace (2, "size after erase= %d", insn_str.size());


  }
  return dag;
}

bool SchedulerPass::IsLock(InstructionEntry *entry) {
    if (entry->op() == OP_lock)
      return true;
    else
      return false;
}
// An instruction is considered to touch memory if
// 1. It has base or index registers, buit not a lea
// 2. It is a call instruction
bool SchedulerPass::IsMemOperation (InstructionEntry *entry) {
    if (entry->IsCall())
      return true;
    if (entry->op() == OP_lea)
      return false;
    if (entry->HasBaseRegister() || entry->HasIndexRegister())
      return true;
    if (entry->HasPrefix (REPE_PREFIX_OPCODE) ||
        entry->HasPrefix (REPNE_PREFIX_OPCODE))
      return true;


    switch (entry->op())
    {
      case OP_cmpxchg:
      case OP_cmpxchg8b:
      case OP_cmpxchg16b:
      case OP_lfence:
      case OP_mfence:
      case OP_sfence:
      case OP_lock:
      case OP_push:
      case OP_pusha:
      case OP_pushf:
      case OP_pop:
      case OP_popa:
      case OP_popf:
      case OP_rep:
      case OP_repe:
      case OP_repz:
      case OP_repne:
      case OP_repnz:
      case OP_cmps:
      case OP_stos:
      case OP_lods:
      case OP_scas:

        return true;
      default:
        return false;
    }

    return false;

}
bool SchedulerPass::IsControlOperation (InstructionEntry *entry) {
  if (entry->IsReturn() || entry->IsJump() || entry->IsCondJump())
    return true;
  switch (entry->op())
  {
    case OP_leave:
      return true;
    default:
      return false;
  }
  return false;
}
// Certain instructions can not be reordered and their positions
// need  to be maintained
bool SchedulerPass::CanReorder (InstructionEntry *entry) {
  if (entry->IsReturn() || entry->IsJump() || entry->IsCondJump())
    return false;
  switch (entry->op())
  {
    case OP_leave:
      return false;
    default:
      return true;
  }
  return true;
}

// Is the transformation profitable for this function
// Right now it checks a list of function names passed as
// a parameter to deciede if the function is profitable or
// not
bool SchedulerPass::IsProfitable(Function *function) {
  //List of comma separated functions to apply this pass
  const char *function_list;
  function_list = GetOptionString("function_list");
  const char *func_name = (function->name()).c_str();
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
        return true;
      }
      return false;
    }
  else
    {
      return true;
    }
}

// External Entry Point
//

void InitScheduler() {
  RegisterFunctionPass(
      "SCHEDULER",
      MaoFunctionPassManager::GenericPassCreator<SchedulerPass>);
}
