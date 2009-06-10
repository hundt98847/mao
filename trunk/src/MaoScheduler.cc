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


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_OPTIONS_DEFINE(SCHEDULER, 0) {
};

#define MAX_REGS 256

#define NO_DEP 0
#define TRUE_DEP 1
#define OUTPUT_DEP 2
#define ANTI_DEP 4
#define MEM_DEP 8
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
        fprintf (file, "%s -> ", dag_insn_str_[i].c_str());
        for (int j=0; j<num_instructions_; j++) {
          if(adj_matrix_[i*num_instructions_ + j] != NO_DEP)
            fprintf (file, "%s[%d],  ", dag_insn_str_[j].c_str(), adj_matrix_[i*num_instructions_ + j]);
        }
        fprintf (file, "\n");
      }
    }

    DependenceDag *Reverse () {
      DependenceDag *reverse_dag = new DependenceDag (num_instructions_, dag_insn_str_);
      for (int i=0; i<num_instructions_; i++) {
        for (int j=0; j<num_instructions_; j++) {
          reverse_dag->adj_matrix_[i*num_instructions_ +j] = adj_matrix_[j*num_instructions_ +i];
        }
      }
      return reverse_dag;
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
      : MaoFunctionPass("SCHEDULER", options, mao, func) { }

  bool Go() {
    CFG *cfg = CFG::GetCFG (unit_, function_);
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
        dag->Print(stderr);
        int *dependence_heights = ComputeDependenceHeights(dag);
        for (int i=0; i<dag->NodeCount(); i++) {
          Trace (2, "%s: %d", insn_str_[i].c_str(), dependence_heights[i]);
        }
        Schedule (dag, dependence_heights, (*bb_iterator)->first_entry());
        PrefixLocks (*bb_iterator);
      }

    }


    return true;
  }

 private:
  std::map<std::string, int, ltstr> insn_map_;
  std::set<MaoEntry *> lock_set_;
  std::string *insn_str_;
  MaoEntry **entries_;
  BitString GetSrcRegisters (InstructionEntry *insn);
  BitString GetDestRegisters (InstructionEntry *insn);
  DependenceDag *FormDependenceDag(BasicBlock *bb);
  bool CanReorder (InstructionEntry *entry);
  bool IsMemOperation (InstructionEntry *entry);
  bool IsLock (InstructionEntry *entry);
  int *ComputeDependenceHeights (DependenceDag *dag);
  int RemoveTallest (std::list<int> *list, int *heights);
  void ScheduleNode (int node, MaoEntry **head);
  void Schedule (DependenceDag *dag, int *dependence_heights, MaoEntry *head);
  void PrefixLocks (BasicBlock *bb);



};

void SchedulerPass::Schedule (DependenceDag *dag, int *dependence_heights, MaoEntry *head) {
  char *scheduled = new char[dag->NodeCount()];
  memset (scheduled, 0, dag->NodeCount());
  //Get instructions that are ready to be scheduled
  std::list<int> *ready = dag->GetEntries();

  //Schedule the available instruction with the maximum height
  //as the first instruction of the function

  while (!ready->empty()) {
    int node = RemoveTallest (ready, dependence_heights);
    ScheduleNode (node, &head);
    scheduled[node]=1;
    //Schedule the successors depthwise till no further scheduling
    //can be done
    std::list<int> *successors = dag->GetSuccessors(node);
    while (!successors->empty()) {
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
            Trace (2, "Predecessor node %s not scheduled ", insn_str_[pred].c_str());
            can_schedule=false;
            break;
          }
        }
        if (can_schedule) {
          ready->push_back(succ);
          Trace (2, "Adding successor node %s  with dep %d to the ready queue", insn_str_[succ].c_str(), dag->GetEdge(node, succ));
          if (dependence_heights[succ] > best_height && 
              (dag->GetEdge(node, succ) & TRUE_DEP) ) {
            best_height = dependence_heights[succ];
            best_node = succ;
            Trace (2, "New best node. Height = %d", best_height);
          }
        }
      }
      if (best_node == -1)
        break;
      ScheduleNode (best_node, &head);
      scheduled[best_node]=1;
      ready->remove (best_node);
      successors = dag->GetSuccessors (best_node);
    }

  }
}

void SchedulerPass::PrefixLocks (BasicBlock *bb) {
  for(SectionEntryIterator entry_iter = bb->EntryBegin();
      entry_iter != bb->EntryEnd(); ++entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (lock_set_.find(entry) != lock_set_.end())
      entry->LinkBefore(unit_->CreateLock(function_));
  }
}
void SchedulerPass::ScheduleNode (int node, MaoEntry **head) {
  MaoEntry *entry = entries_[node];
  //Don't do anything if the node to be scheduled is the head node
  if (entry == *head)
    return;
  entry->Unlink();
  (*head)->LinkAfter(entry);
  std::string str1, str2;
  (*head)->ToString (&str1);
  entry->ToString (&str2);
  Trace (2, "Scheduling %s before %s", str2.c_str(), str1.c_str());
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
  delete work_list;
  delete new_work_list;
  return heights;


}

BitString SchedulerPass::GetSrcRegisters (InstructionEntry *insn) {
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

}

BitString SchedulerPass::GetDestRegisters (InstructionEntry *insn) {
  BitString def_mask = GetRegisterDefMask(insn);
  return def_mask;
}

SchedulerPass::DependenceDag *SchedulerPass::FormDependenceDag (BasicBlock *bb) {
  int last_writer[MAX_REGS];
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
    if(!CanReorder(insn))
      continue;
    insns_in_bb++;
  }
  if (insns_in_bb == 0)
    return NULL;
  //Remove any lock instructions in the original code
  //lock acts more like a prefix in the sense that it applies to the
  //immediately following isntruction. However, it has a separate entry
  //in the code. To enforce the fact that the lock and the following
  //instruction have to be adjacent even after the scheduling, the
  //instructions following a lock are added to a separate set and the
  //lock is removed from the original stream. During scheduling if the
  //instruction is in lock_set_, a new lock instruction is created and
  //prefixed to it.
  for (std::vector<MaoEntry*>::iterator lock_iter=locks.begin();
       lock_iter != locks.end(); ++lock_iter) {
    MaoEntry *entry=*lock_iter;
    entry->Unlink();
  }

  insn_str_ = new std::string[insns_in_bb];
  entries_ = new MaoEntry*[insns_in_bb];

  DependenceDag *dag = new DependenceDag(insns_in_bb, insn_str_);
  insns_in_bb = 0;
  memset (last_writer,0xFF, MAX_REGS*sizeof(int));
  int prev_mem_operation = -1;
  for(SectionEntryIterator entry_iter = bb->EntryBegin();
      entry_iter != bb->EntryEnd(); ++entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (!entry->IsInstruction())
      continue;
    InstructionEntry *insn = entry->AsInstruction();
    if(!CanReorder(insn))
      continue;
    entries_[insns_in_bb] = insn;
    insn->ToString(&insn_str_[insns_in_bb]);
    //Trace (2, "Instruction %d: %s\n", insns_in_bb, insn_str_[insns_in_bb].c_str());
    if (IsMemOperation(insn)) {
      if (prev_mem_operation != -1)
        dag->AddEdge (prev_mem_operation, insns_in_bb, MEM_DEP);
      prev_mem_operation = insns_in_bb;
    }
    insn_map_[insn_str_[insns_in_bb]] = insns_in_bb;



    BitString src_regs_mask = GetSrcRegisters (insn);
    BitString dest_regs_mask = GetDestRegisters (insn);
    int index=0;
    while ((index = src_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "src reg = %d", index);
      if (last_writer[index] >=0)
        dag->AddEdge (last_writer[index], insns_in_bb, TRUE_DEP);
      index++;
    }

    index=0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "dest reg = %d", index);
      if (last_writer[index] >=0)
        dag->AddEdge (last_writer[index], insns_in_bb, OUTPUT_DEP);
      last_writer[index]=insns_in_bb;
      index++;
    }

    insns_in_bb++;

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
    if(!CanReorder(insn))
      continue;
    insns_in_bb--;
    BitString src_regs_mask = GetSrcRegisters (insn);
    BitString dest_regs_mask = GetDestRegisters (insn);
    int index=0;
    insn->ToString(&insn_str);
    //Trace (2, "Instruction %d: %s\n", insns_in_bb, insn_str.c_str());
    while ((index = src_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "src reg = %d", index);
      if (last_writer[index] >=0)
        dag->AddEdge (insns_in_bb, last_writer[index], ANTI_DEP);
      index++;
    }

    index=0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      //Trace (2, "dest reg = %d", index);
      last_writer[index]=insns_in_bb;
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
    switch (entry->op())
    {
      case OP_cmpxchg:
      case OP_cmpxchg8b:
      case OP_cmpxchg16b:
      case OP_lfence:
      case OP_lock:
      case OP_push:
      case OP_pusha:
      case OP_pushf:
      case OP_pop:
      case OP_popa:
      case OP_popf:
        return true;
      default:
        return false;
    }

    return false;

}
// Certain instructions can not be reordered and their positions
// need  to be maintained
bool SchedulerPass::CanReorder (InstructionEntry *entry) {
  if (entry->IsReturn() || entry->IsJump() || entry->IsCondJump() || entry->op() == OP_leave || entry->op() == OP_cmp)
    return false;
  else
    return true;
}

// External Entry Point
//

void InitScheduler() {
  RegisterFunctionPass(
      "SCHEDULER",
      MaoFunctionPassManager::GenericPassCreator<SchedulerPass>);
}
