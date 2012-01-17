//
// Copyright 2010 Google Inc.
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
#include "Mao.h"

namespace {
PLUGIN_VERSION

#define LCD_HEIGHT_ADJUSTMENT 10
#define HOT_REGISTER_BONUS 1

// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(SCHEDULER, "Schedules instructions at the assembly level", \
                   5) {
  // The next four options are helpful in debugging the scheduler
  // by limiting  the functions to which the transformation is applied
  OPTION_STR("function_list", "",
             "A comma separated list of mangled function names "
             "on which this pass is applied. "
             "An empty string means the pass is applied on all functions"),
  OPTION_STR("functions_file", "",
             "A file with a list of mangled function names. "
             "The position in this file gives a unique number to the "
             "functions"),
  OPTION_INT("start_func", 0,
             "Number of the first function in functions_file "
             "that is optimized."),
  OPTION_INT("end_func", 1000000000,
             "Number of the last  function in functions_file "
             "that is optimized"),
  OPTION_INT("max_steps", 1000000000,
             "Maximum number of scheduling operations performed in "
             "any function"),
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
  /* A simple graph data structure to represent dependence graphs in basic
   * blocks. Uses an adjacency matrix representation.
   */
  class DependenceDag {
   public:
    DependenceDag(int num_instructions, std::string *insn_str) {
      dag_insn_str_ = insn_str;
      num_instructions_ = num_instructions;
      adj_matrix_ = new char[num_instructions*num_instructions];
      memset(adj_matrix_, 0, num_instructions*num_instructions);
    }

    ~DependenceDag() {
      delete [] adj_matrix_;
    }

    void AddEdge(int u, int v, int type) {
      adj_matrix_[u*num_instructions_+v] |= type;
    }

    inline char GetEdge(int u, int v) {
      return adj_matrix_[u*num_instructions_+v];
    }

    void GetPredEdges(int u, std::list<int> *edges) {
      for (int i = 0; i < num_instructions_; i++) {
        if (adj_matrix_[i*num_instructions_+u])
          edges->push_back(i);
      }
    }

    void GetSuccEdges(int u, std::list<int> *edges) {
      for (int i = 0; i < num_instructions_; i++) {
        if (adj_matrix_[u*num_instructions_+i])
          edges->push_back(i);
      }
    }

    int NodeCount() {
      return num_instructions_;
    }

    std::string * GetInstructionStrings() {
      return dag_insn_str_;
    }


    void Print(FILE *file) {
      fprintf(file, "#instructions = %d\n", num_instructions_);
      for (int i = 0; i < num_instructions_; i++) {
        fprintf(file, "(%d) %s -> ", i, dag_insn_str_[i].c_str());
        for (int j = 0; j < num_instructions_; j++) {
          if (adj_matrix_[i*num_instructions_ + j] != NO_DEP)
            fprintf(file, "(%d) %s[%d],  ", j,
                     dag_insn_str_[j].c_str(),
                     adj_matrix_[i*num_instructions_ + j]);
        }
        fprintf(file, "\n");
      }
    }


    std::list<int>* GetExits(int edge_mask = ALL_DEPS) {
      std::list<int>* exits= new std::list<int>;
      for (int i = 0; i < num_instructions_; i++) {
        if (NumSuccessors (i, edge_mask) ==0)
          exits->push_back(i);
      }
      return exits;
    }

    std::list<int>* GetEntries(int edge_mask = ALL_DEPS) {
      std::list<int>* entries= new std::list<int>;
      for (int i = 0; i < num_instructions_; i++) {
        if (NumPredecessors(i, edge_mask) == 0)
          entries->push_back(i);
      }
      return entries;
    }

    std::list<int>* GetSuccessors(int node, int edge_mask = ALL_DEPS) {
      std::list<int>* successors = new std::list<int>;
      for (int i = 0; i < num_instructions_; i++) {
        if (adj_matrix_[node*num_instructions_+i]&edge_mask)
          successors->push_back(i);
      }
      return successors;
    }

    std::list<int>* GetPredecessors(int node, int edge_mask = ALL_DEPS) {
      std::list<int>* predecessors = new std::list<int>;
      for (int i = 0; i < num_instructions_; i++) {
        if (adj_matrix_[i*num_instructions_+node]&edge_mask)
          predecessors->push_back(i);
      }
      return predecessors;
    }

    int NumSuccessors(int node, int edge_mask = ALL_DEPS) {
      int num_successors = 0;
      for (int i = 0; i < num_instructions_; i++) {
        if (adj_matrix_[node*num_instructions_+i]&edge_mask)
          num_successors++;
      }
      return num_successors;
    }

    int NumPredecessors(int node, int edge_mask = ALL_DEPS) {
      int  num_predecessors = 0;
      for (int i = 0; i < num_instructions_; i++) {
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

  /* A scheduler node represents a set of consecutive entries that are treated
   * as an indivisible entity during scheduling. Certain entries should always
   * apear consecutively in code. For instance a lock should always precede the
   * instruction following it even after scheduling. Grouping entries into a
   * scheduler node guarantees this.
   */
  struct SchedulerNode {
    /* The first and last entries in the group */
    MaoEntry *first, *last;
    std::string ToString(std::string *out_str) {
      for (MaoEntry *entry = first; entry != last->next();
           entry = entry->next()) {
        std::string entry_str;
        entry->ToString(&entry_str);
        out_str->append(entry_str);
      }
      return *out_str;
    }
  };
  typedef std::vector<SchedulerNode *>::iterator SchedulerNodeIterator;
  typedef std::vector<SchedulerNode *>::reverse_iterator
      SchedulerNodeReverseIterator;

  SchedulerPass(MaoOptionMap *options, MaoUnit *mao, Function *func)
    : MaoFunctionPass("SCHEDULER", options, mao, func) {

      profitable_ = IsProfitable(func);
      rsp_pointer_ = GetRegFromName("rsp");
      // The default CFA register is RSP for 64 bit code and ESP for 32 bit
      // code. Since the scheduler doesn't differentiate sub-registers and
      // parent registers differently when computing dependences, it is ok to
      // assign rsp_pointer_ to cfa_reg_
      cfa_reg_ = rsp_pointer_;
  }

  bool Go() {
    int start_func = GetOptionInt("start_func");
    int end_func = GetOptionInt("end_func");
    max_steps_ = GetOptionInt("max_steps");
    num_steps_ = 0;
    const char* functions_file = GetOptionString("functions_file");


    // Read the functions_file and find the position of the current function
    // in the file. If it is out of the binary-search range, do nothing.
    FILE *fp = NULL;
    if (strcmp(functions_file, "")) {
      fp = fopen(functions_file, "r");
      const char *this_func_name = (function_->name()).c_str();
      char line[4096];
      int func_num = 0;
      while (!feof(fp)) {
        int ret = fscanf(fp, "%s", line);
        if (ret == EOF)
          break;
        if (!strcmp(line, this_func_name))
          break;
        func_num++;
      }
      fclose(fp);
      Trace(0, "Function %d: %s", func_num, this_func_name);


      if (func_num < start_func ||
          func_num > end_func)
        return true;
    }
    if (!profitable_)
      return true;

    CFG *cfg = CFG::GetCFG(unit_, function_, true);
    // Compute the set of trivial (single BB) loops. Useful
    // when computing the cost function later.
    FindBBsInStraightLineLoops();

    // Schedule each BB in the function
    FORALL_CFG_BB(cfg, bb_iterator) {
      MaoEntry *first = *((*bb_iterator)->EntryBegin());
      MaoEntry *last = *((*bb_iterator)->EntryEnd());
      std::string first_str, last_str;
      lock_set_.clear();
      if (first)
        first->ToString(&first_str);
      if (last)
        last->ToString(&last_str);
      Trace(2, "BB start = %s, BB end = %s",
            first_str.c_str(), last_str.c_str());
      DependenceDag *dag = FormDependenceDag(*bb_iterator);
      if (dag != NULL) {
        Trace(2, "Dag for new bb:");
        if (tracing_level() >= 2)
          dag->Print(stderr);
        int *dependence_heights = ComputeDependenceHeights(dag);
        for (int i = 0; i < dag->NodeCount(); i++) {
          Trace(2, "%s: %d", insn_str_[i].c_str(), dependence_heights[i]);
        }

        // The head should point to the entry before the first
        // instruction in the BB.
        MaoEntry *head = (*bb_iterator)->first_entry();
        if (head->IsInstruction()) {
          head = head->prev();
        } else {
          while (!head->next()->IsInstruction())
            head = head->next();
        }
        MaoEntry *last_entry = (*bb_iterator)->last_entry();

        last_entry = Schedule(dag, dependence_heights, head, last_entry);
        // Free memory allocated in FormDependenceDag
        delete [] insn_str_;
        delete [] is_lcd_source_;
        delete dag;
      }
    }
    Trace(1, "Number of scheduler operations : %d ", num_steps_);
    return true;
  }

 private:
  std::set<MaoEntry *> lock_set_;
  std::string *insn_str_;
  std::vector<SchedulerNode *> entries_;
  int num_steps_, max_steps_;
  bool profitable_;
  // The set of BBs that form a single BB loops
  std::set<BasicBlock *> bbs_in_stline_loops_;

  // An array used to keep track of instructions that
  // are sources of some loop carried dependence
  char *is_lcd_source_;

  const reg_entry *rsp_pointer_;
  const reg_entry *cfa_reg_;

  BitString GetSrcRegisters(SchedulerNode *node);
  BitString GetDestRegisters(SchedulerNode *node);
  DependenceDag *FormDependenceDag(BasicBlock *bb);
  bool HasMemOperation(SchedulerNode *node) const;
  bool IsMemOperation(InstructionEntry *insn) const;
  bool IsMemCFIDirective(MaoEntry *entry) const;
  bool HasControlOperation(SchedulerNode *node) const;
  bool IsControlOperation(InstructionEntry *insn) const;
  bool HasPredicateOperation(SchedulerNode *node) const;
  bool IsPredicateOperation(InstructionEntry *insn) const;
  int *ComputeDependenceHeights(DependenceDag *dag);
  int RemoveTallest(std::list<int> *list, int *heights);
  void ScheduleNode(int node, MaoEntry **head, MaoEntry **last);
  MaoEntry* Schedule(DependenceDag *dag,
                     int *dependence_heights,
                     MaoEntry *head,
                     MaoEntry *last);
  bool IsProfitable(Function *function);
  void FindBBsInStraightLineLoops();
  void FindBBsInStraightLineLoops(SimpleLoop *loop);
  void InitializeLastWriter(int *last_writer);
  int  CreateSchedulerNodes(MaoEntry *head, BasicBlock *bb);
};

void SchedulerPass::FindBBsInStraightLineLoops() {
  LoopStructureGraph *loop_graph = LoopStructureGraph::GetLSG(unit_,
                                                              function_,
                                                              true);
  SimpleLoop *root = loop_graph->root();
  FindBBsInStraightLineLoops(root);
}

// If a loop has a single BB, add it to bbs_in_stline_loops_.
// Recursively apply the method to inner loops
void SchedulerPass::FindBBsInStraightLineLoops(SimpleLoop *loop) {
  if (loop->header() == loop->bottom() && loop->header() != NULL) {
    bbs_in_stline_loops_.insert(loop->header());
    // This must be an innermost loop since it has a single BB
    return;
  }
  // Recurse
  for (SimpleLoop::LoopSet::iterator liter = loop->GetChildren()->begin();
       liter != loop->GetChildren()->end(); ++liter)
    FindBBsInStraightLineLoops(*liter);
}

// Given a dependence dag and the dependence height (from sink) of nodes
// in the dag, apply the scheduling heuristic
MaoEntry* SchedulerPass::Schedule(DependenceDag *dag,
                                  int *dependence_heights,
                                  MaoEntry *head,
                                  MaoEntry *last_entry) {
  char *scheduled = new char[dag->NodeCount()];
  memset(scheduled, 0, dag->NodeCount());
  // Get instructions that are ready to be scheduled
  std::list<int> *ready = dag->GetEntries();

  // Schedule the available instruction with the maximum height
  // as the first instruction of the function
  int *num_scheduled_predecessors = new int[dag->NodeCount()];
  int *num_predecessors = new int[dag->NodeCount()];
  memset(num_scheduled_predecessors, 0,
         dag->NodeCount()*sizeof(*num_scheduled_predecessors));
  memset(num_predecessors, 0, dag->NodeCount()*sizeof(*num_predecessors));

  for (int i = 0; i < dag->NodeCount(); i++)
    num_predecessors[i] = dag->NumPredecessors(i);

  while (!ready->empty()) {
    int node = RemoveTallest(ready, dependence_heights);
    ScheduleNode(node, &head, &last_entry);
    scheduled[node]=1;
    num_steps_++;
    // Stop scheduling if we have reached the scheduling threshold
    if (num_steps_ >= max_steps_)
      break;
    // Schedule the successors depthwise till no further scheduling
    // can be done
    std::list<int> *successors = dag->GetSuccessors(node);
    // Find that successor with the largest dependence height, all of
    // whose predecessors are already scheduled
    int best_height = -1;
    for (std::list<int>::iterator succ_iter = successors->begin();
         succ_iter != successors->end(); ++succ_iter) {
      int succ = *succ_iter;
      num_scheduled_predecessors[succ]++;
      /*
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
      */
      // If all the predecessors of this node is scheduled, this node can
      // be added to the ready queue
      if (num_scheduled_predecessors[succ] == num_predecessors[succ]) {
        ready->push_back(succ);
        Trace(2, "Adding successor node (%d) %s  with dep %d and height"
              "%d to the ready queue",
              succ,
              insn_str_[succ].c_str(),
              dag->GetEdge(node, succ),
              dependence_heights[succ]);
        if (!HasMemOperation(entries_[node]) &&
            (dag->GetEdge(node, succ) & TRUE_DEP) ) {
          dependence_heights[succ] += HOT_REGISTER_BONUS;
          Trace(2, "New best node. Height = %d", best_height);
        }
      }
    }
  }
  delete [] num_scheduled_predecessors;
  delete [] num_predecessors;
  delete [] scheduled;

  return last_entry;
}

/*
 * Schedules a node immediately after the head node, making it the new head
 * node. If the scheduled node is the last node, update the last node.
 */
void SchedulerPass::ScheduleNode(int node_index, MaoEntry **head,
                                 MaoEntry **last) {
  SchedulerNode *node = entries_[node_index];
  // Don't do anything if the node to be scheduled is the head node
  if (node->first == *head) {
    *head = node->last;
    return;
  }
  MaoEntry *prev_entry = node->first->prev();
  // If the node to be scheduled is right after the head node
  // just change head to point to the new node
  std::string str11, str12, str13, str14, str15;
  node->first->ToString(&str11);
  node->last->ToString(&str12);
  prev_entry->ToString(&str13);
  Trace(2, "node->first = %s\n", str11.c_str());
  Trace(2, "node->last = %s\n", str12.c_str());
  if (prev_entry != NULL)
    Trace(2, "prev  = %s\n", str13.c_str());

  if (prev_entry == *head) {
    *head = node->last;
    return;
  }
  // If we are scheduling the last entry in the BB, recompute the new last entry
  if (node->last == *last) {
    *last = node->first->prev();
  }
  node->first->Unlink(node->last);
  (*head)->LinkAfter(node->first);
  if (prev_entry->IsDirective()) {
    DirectiveEntry *insn = prev_entry->AsDirective();
    if (insn->op() == DirectiveEntry::P2ALIGN ||
        insn->op() == DirectiveEntry::P2ALIGNW ||
        insn->op() == DirectiveEntry::P2ALIGNL) {
        prev_entry->Unlink();
        node->first->LinkBefore(prev_entry);
    }
  }
  std::string str1, str2;
  (*head)->ToString(&str1);
  node->first->ToString(&str2);
  Trace(2, "Scheduling (%d) %s after %s", node_index, str2.c_str(),
        str1.c_str());
  *head = node->last;
}

int SchedulerPass::RemoveTallest(std::list<int> *list, int *heights) {
  int best_height = -1, best_node = -1;
  for (std::list<int>::iterator iter = list->begin();
       iter != list->end(); ++iter) {
    int node = *iter;
    if (heights[node] > best_height) {
      best_height = heights[node];
      best_node = node;
    } else if ((heights[node] == best_height) &&
              (node < best_node)) {
      best_height = heights[node];
      best_node = node;
    }
  }
  list->remove(best_node);
  return best_node;
}

int *SchedulerPass::ComputeDependenceHeights(DependenceDag *dag) {
  int *heights, *visited;
  std::list<int> *work_list = dag->GetExits(TRUE_DEP|MEM_DEP);
  std::list<int> *new_work_list = new std::list<int>;
  heights = new int[dag->NodeCount()];
  visited = new int[dag->NodeCount()];
  // Initialize heights to -1 to denote that the height is not yet computed
  memset(heights, 0xFF, dag->NodeCount()*sizeof(heights[0]));
  memset(visited, 0, dag->NodeCount()*sizeof(visited[0]));

  while (!work_list->empty()) {
    for (std::list<int>::iterator iter = work_list->begin();
         iter != work_list->end(); ++iter) {
      int height = 0;
      int node = *iter;
      bool reprocess = false;
      std::list<int> *succ_nodes = dag->GetSuccessors(node, TRUE_DEP|MEM_DEP);
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

      if (reprocess) {
        new_work_list->push_back(node);
      } else {
        // If the node is the exit of the dag, but there is a loop
        // carried dependence originating from it, set the height
        // to LCD_HEIGHT_ADJUSTMENT

        heights[node] = height;

        std::list<int> *pred_nodes = dag->GetPredecessors(node,
                                                          TRUE_DEP|MEM_DEP);
        for (std::list<int>::iterator pred_iter = pred_nodes->begin();
             pred_iter != pred_nodes->end(); ++pred_iter) {
          int pred_node = *pred_iter;
          if (!visited[pred_node]) {
            new_work_list->push_back(pred_node);
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
  for (int i = 0; i < dag->NodeCount(); i++)
    if (is_lcd_source_[i])
      heights[i] += LCD_HEIGHT_ADJUSTMENT;

  delete work_list;
  delete new_work_list;
  delete [] is_lcd_source_;
  is_lcd_source_ = NULL;
  return heights;
}


BitString SchedulerPass::GetSrcRegisters(SchedulerNode *node) {
  BitString use_mask;
  for (MaoEntry *entry = node->first; entry != node->last->next();
       entry = entry->next()) {
    if (entry->IsInstruction()) {
      InstructionEntry *insn = entry->AsInstruction();
      use_mask = use_mask | GetRegisterUseMask(insn, true);
    } else if (entry->IsDirective()) { // Handle .cfi directives
      DirectiveEntry *de = entry->AsDirective();
      DirectiveEntry::Opcode opcode = de->op();
      const DirectiveEntry::Operand *operand = NULL;
      const reg_entry *reg;
      switch (opcode) {
        case DirectiveEntry::CFI_DEF_CFA:
        case DirectiveEntry::CFI_DEF_CFA_REGISTER:
        case DirectiveEntry::CFI_OFFSET:
          // All these directives are assumed to use the *current* cfa_reg_ to
          // prevent scheduling across instructions that write to the current
          // cfa_reg_
          use_mask = use_mask | GetMaskForRegister(cfa_reg_);
          if (opcode != DirectiveEntry::CFI_OFFSET) {
            // For .cfi_def_cfa and .cfi_def_cfa_register, add the new cfa
            // register (which is the first operand of these directives) to the
            // use_mask
            bool is_64_bit = (entry->GetFlag() == CODE_64BIT);
            operand = de->GetOperand(0);
            const char *reg_num_str = operand->data.str->c_str();
            char *endptr = NULL;
            int reg_num = (int)(strtoul(reg_num_str, &endptr, 10));
            // In the .cfi directives we have seen, a register is represented by
            // a dwarf register number. However, this operand gets passed to MAO
            // as string. Just in case there are .cfi directives with actual
            // register names, the following assert checks if the operand is
            // indeed a number.
            MAO_RASSERT_MSG( (*endptr == 0),
                             "Not a valid dwarf2 register number");
            reg = GetRegFromDwarfNumber(reg_num, is_64_bit);
            use_mask = use_mask | GetMaskForRegister(reg);
            cfa_reg_ = reg;
          }
          break;
        default:
          {/*Do Nothing*/}
      }
    }
  }
  return use_mask;
}

BitString SchedulerPass::GetDestRegisters(SchedulerNode *node) {
  BitString def_mask;
  for (MaoEntry *entry = node->first; entry != node->last->next();
       entry = entry->next()) {
    if (entry->IsInstruction()) {
      InstructionEntry *insn = entry->AsInstruction();
      def_mask = def_mask | GetRegisterDefMask(insn, true);
    } else if (entry->IsDirective()) {
      DirectiveEntry *de = entry->AsDirective();
      DirectiveEntry::Opcode opcode = de->op();
      const DirectiveEntry::Operand *operand = NULL;
      const reg_entry *reg;
      switch (opcode) {
        case DirectiveEntry::CFI_DEF_CFA:
        case DirectiveEntry::CFI_DEF_CFA_REGISTER:
          {
            // For .cfi_def_cfa and .cfi_def_cfa_register, add the new cfa
            // register (which is the first operand of these directives) to the
            // def_mask
            bool is_64_bit = false;
            operand = de->GetOperand(0);
            const char *reg_num_str = operand->data.str->c_str();
            char *endptr = NULL;
            int reg_num = (int)(strtoul(reg_num_str, &endptr, 10));
            MAO_RASSERT_MSG( (*endptr == 0),
                             "Not a valid dwarf2 register number");
            if (entry->GetFlag() == CODE_64BIT)
              is_64_bit = true;
            reg = GetRegFromDwarfNumber(reg_num, is_64_bit);
            def_mask = def_mask | GetMaskForRegister(reg);
          }
          break;
        default:
          {/*Do nothing. */}
      }
    }
  }
  return def_mask;
}

void SchedulerPass::InitializeLastWriter(int *last_writer) {
  int insns_in_bb = 0;
  for (SchedulerNodeIterator entry_iter = entries_.begin();
      entry_iter != entries_.end(); ++entry_iter) {
    SchedulerNode *sn = *entry_iter;

    BitString dest_regs_mask = GetDestRegisters(sn);
    int index = 0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      // Trace (2, "dest reg = %d", index);
      last_writer[index]=insns_in_bb;
      index++;
    }
    insns_in_bb++;
  }
}

/* Split the entries in bb, starting from head, into scheduler nodes.
 * Multiple entries are grouped into a single SchedulerNode in the following
 * cases:
 * 1. A sequence of non-instruction  entries is grouped with the instruction
 * entry that follows the sequence.
 * 2. A lock instruction is grouped with the immediately following instruction.
 * 3. A thunk call (a call that gets the current IP) is grouped with the
 * immediately following instruction.
 * 4. A sequence of entries that access a thread level variable are grouped
 * together. The code sequence for TLS access for various relocations is
 * described in http://people.redhat.com/drepper/tls.pdf */

int  SchedulerPass::CreateSchedulerNodes(MaoEntry *head, BasicBlock *bb) {
  int retain_next = 0;
  MaoEntry *first = NULL;
  SchedulerNode *sn = NULL;
  for (EntryIterator entry_iter(head);
       entry_iter != bb->EntryEnd(); ++entry_iter) {
    MaoEntry *entry = *entry_iter;
    if (entry == NULL)
      break;
    if (retain_next > 0) {
      retain_next--;
      continue;
    }
    if (first == NULL)
      first = entry;
    if (!entry->IsInstruction())
      continue;
    InstructionEntry *insn = entry->AsInstruction();
    if (insn->IsThunkCall())
      continue;
    if (insn->IsLock())
      continue;
    // Handle TLS sequences
    switch (insn->GetReloc(0)) {
      case BFD_RELOC_X86_64_TLSGD:
        /* The sequence looks like:
          .byte 0x66
          leaq x@tlsgd(%rip),%rdi
          .word 0x6666
          rex64
          call __tls_get_addr@plt */
        retain_next = 2;
        continue;

      case BFD_RELOC_X86_64_TLSLD:
        /* The sequence looks like:
           leaq x1@tlsld(%rip),%rdi
           call __tls_get_addr@plt */
      case BFD_RELOC_386_TLS_GD:
        /* The sequence looks like:
           leal x@tlsgd(,%ebx,1),%eax
           call __tls_get_addr@plt */
      case BFD_RELOC_386_TLS_LDM:
        /* The sequence looks like:
           leal x1@tlsldm(%ebx),%eax
           call __tls_get_addr@plt */
        continue;

      case BFD_RELOC_X86_64_TPOFF32:
        /* The sequence looks like:
           movq %fs:0,%rax
           leaq (or movq) x@tpoff(%rax),%rax
           or a single instruction:
           movq %fs:x@tpoff,%rax */
      case BFD_RELOC_386_TLS_LE:
        /* The sequence looks like:
          movl %gs:0,%eax
          leal (or movl) x@ntpoff(%eax),%eax */
        if (entries_.size() > 0) {
          sn = entries_.back();
          entries_.pop_back();
          first = sn->first;
          delete sn;
          break;
        }
      case BFD_RELOC_386_TLS_GOTIE:
        /* The sequence looks like:
           movl %gs:0,%eax
           addl x@gotntpoff(%ebx),%eax
           (or)
           movl x@gotntpoff(%ebx),%eax
           movl %gs:(%eax),%eax */
      case BFD_RELOC_386_TLS_IE:
        /* The sequence looks like:
           movl %gs:0,%eax
           addl x@indntpoff,%eax
           (or)
           movl x@indntpoff,%ecx
           movl %gs:(%ecx),%eax */
      case BFD_RELOC_X86_64_GOTTPOFF:
        /* The sequence looks like:
          movq %fs:0,%rax
          addq x@gottpoff(%rip),%rax
          (or)
          movq x@gottpoff(%rip),%rax
          movq %fs:(%rax),%rax */

        if (insn->op() == OP_add) {
          sn = entries_.back();
          entries_.pop_back();
          first = sn->first;
          delete sn;
          break;
        } else {
          continue;
        }
      default:
        {} /*Do Nothing*/
    }

    sn = new SchedulerNode;
    sn->last = insn;
    sn->first = first;
    first = NULL;
    entries_.push_back(sn);
  }
  return entries_.size();
}

SchedulerPass::DependenceDag *SchedulerPass::FormDependenceDag(BasicBlock *bb) {
  int last_writer[MAX_REGS];
  std::vector<int> writers[MAX_REGS];
  int nodes_in_bb = 0;
  entries_.clear();
  MaoEntry *ins_start = NULL;

  for (EntryIterator entry_iter = bb->EntryBegin();
      entry_iter != bb->EntryEnd(); ++entry_iter) {
    if ((*entry_iter)->IsInstruction()) {
      ins_start = *entry_iter;
      break;
    }
  }
  nodes_in_bb = CreateSchedulerNodes(ins_start, bb);
  // Scheduling makes sense only if there is more than one node.
  if (nodes_in_bb <= 1)
    return NULL;

  insn_str_ = new std::string[nodes_in_bb];
  is_lcd_source_ = new char[nodes_in_bb];
  memset(is_lcd_source_, 0, nodes_in_bb*sizeof(is_lcd_source_[0]));
  DependenceDag *dag = new DependenceDag(nodes_in_bb, insn_str_);
  nodes_in_bb = 0;
  memset(last_writer, 0xFF, MAX_REGS*sizeof(last_writer[0]));
  int prev_mem_operation = -1;
  std::vector<int> ctrl_dep_sources;
  if (bbs_in_stline_loops_.find(bb) != bbs_in_stline_loops_.end()) {
    // This BB forms a straightline loop
    InitializeLastWriter(last_writer);
  }
  BitString rsp_mask = GetMaskForRegister(rsp_pointer_);

  for (SchedulerNodeIterator entry_iter = entries_.begin();
       entry_iter != entries_.end(); entry_iter++) {
    SchedulerNode *sn = *entry_iter;
    sn->ToString(&insn_str_[nodes_in_bb]);
    Trace(2, "Instruction %d: %s\n",
          nodes_in_bb, insn_str_[nodes_in_bb].c_str());
    BitString src_regs_mask = GetSrcRegisters(sn);
    BitString dest_regs_mask = GetDestRegisters(sn);
    /* Predicate operations require stricter WAW dependence enforcement.
     * Consider the sequence:
     *   mov  %edx, %ebx (1)
     *   cmov %eax, %ebx (2)
     *   test
     *   cmov %ecx, %ebx (3)
     *   <use of %ebx>
     *
     *  The scheduler could swap 1 and 2 since they only have a WAW dependence
     *  and are followed by another write of %ebx before the use of %ebx. To
     *  enforce a stricter dependence, treat as if (2) also reads %ebx so that
     *  (1) and (2) never gets reordered.
     */
    if (HasPredicateOperation(sn)) {
      src_regs_mask = src_regs_mask | dest_regs_mask;
    }
    char src_str[128], dest_str[128];
    src_regs_mask.ToString(src_str, 128);
    dest_regs_mask.ToString(dest_str, 128);

    Trace(4, "Src registers: %s", src_str);
    Trace(4, "Dest  registers: %s", dest_str);

    // An instruction that modifies SP acts as a barrier for stack-relative
    // memory operations. Here, we are being conservative by preventing
    // reordering of other memory access operations around stack relative
    // accesses
    //
    if (HasMemOperation(sn) || (dest_regs_mask & rsp_mask).IsNonNull()) {
      if (prev_mem_operation != -1)
        dag->AddEdge(prev_mem_operation, nodes_in_bb, MEM_DEP);
      prev_mem_operation = nodes_in_bb;
    }
    if (HasControlOperation(sn)) {
      for (std::vector<int>::iterator src_iter = ctrl_dep_sources.begin();
           src_iter != ctrl_dep_sources.end(); ++src_iter)
        dag->AddEdge(*src_iter, nodes_in_bb, CTRL_DEP);
      ctrl_dep_sources.clear();
    }
    ctrl_dep_sources.push_back(nodes_in_bb);




    int index = 0;
    while ((index = src_regs_mask.NextSetBit(index)) != -1) {
      if (last_writer[index] >=0 && last_writer[index] < nodes_in_bb) {
        dag->AddEdge(last_writer[index], nodes_in_bb, TRUE_DEP);
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
            dag->AddEdge(writer, last_writer[index], OUTPUT_DEP);
        }
        writers[index].clear();
        writers[index].push_back(last_writer[index]);
      } else if (last_writer[index] >= nodes_in_bb) {
        is_lcd_source_[last_writer[index]] = 1;
      }
      index++;
    }

    index = 0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      last_writer[index]=nodes_in_bb;
      writers[index].push_back(nodes_in_bb);
      index++;
    }

    nodes_in_bb++;
  }
  // There may be multiple definitions of a reg with no uses
  // Created WAW dependences to the last writer in that case
  for (int i = 0; i < MAX_REGS; i++) {
      if (last_writer[i] >=0) {
        for (std::vector<int>::iterator w_iter = writers[i].begin();
             w_iter != writers[i].end(); ++w_iter) {
          int writer = *w_iter;
          if (writer != last_writer[i])
            dag->AddEdge(writer, last_writer[i], OUTPUT_DEP);
        }
      }
      writers[i].clear();
  }
  EntryIterator last_entry = bb->EntryEnd();
  EntryIterator first_entry = bb->EntryBegin();
  if (*first_entry == NULL || *last_entry == NULL)
    return dag;
  --first_entry;
  --last_entry;

  // WAR dependences
  memset(last_writer, 0xFF, MAX_REGS*sizeof(last_writer[0]));
  std::string insn_str;
  for (SchedulerNodeReverseIterator entry_iter = entries_.rbegin();
           entry_iter != entries_.rend(); ++entry_iter) {
    nodes_in_bb--;
    SchedulerNode *sn = *entry_iter;
    BitString src_regs_mask = GetSrcRegisters(sn);
    BitString dest_regs_mask = GetDestRegisters(sn);
    int index = 0;
    sn->ToString(&insn_str);
    while ((index = src_regs_mask.NextSetBit(index)) != -1) {
      for (std::vector<int>::iterator wr_iter = writers[index].begin();
           wr_iter != writers[index].end(); ++wr_iter)
        dag->AddEdge(nodes_in_bb, *wr_iter, ANTI_DEP);
      index++;
    }

    index = 0;
    while ((index = dest_regs_mask.NextSetBit(index)) != -1) {
      last_writer[index]=nodes_in_bb;
      writers[index].push_back(nodes_in_bb);
      index++;
    }
    insn_str.erase();
  }
  return dag;
}

bool SchedulerPass::HasMemOperation(SchedulerNode *node) const {
  for (MaoEntry *entry = node->first; entry != node->last->next();
       entry = entry->next()) {
    if (entry->IsInstruction()) {
      InstructionEntry *insn = entry->AsInstruction();
      if (IsMemOperation(insn))
        return true;
    } else if (IsMemCFIDirective(entry)) {
      return true;
    }
  }
  return false;
}

// .cf_offset and .cfi_restore can not move across a memory operation. This
// prevents a stack store from clobbering the location specified by .cfi_offset
// making the claimn of the .cfi_offset directive (the prev value of a register
// is in a specified location) incorrect. .cfi_restore is essentially similar
// to .cfi_offset since it is saying a specified register is at the same
// location as it was at an earlier point in the code.
bool SchedulerPass::IsMemCFIDirective(MaoEntry *entry) const {
  if(!entry->IsDirective())
    return false;
  DirectiveEntry *de = entry->AsDirective();
  DirectiveEntry::Opcode opcode = de->op();
  switch (opcode) {
    case DirectiveEntry::CFI_OFFSET:
    case DirectiveEntry::CFI_RESTORE:
      return true;
    default:
      return false;
  }
}

// An instruction is considered to touch memory if
// 1. It has base or index registers, buit not a lea
// 2. It is a call instruction
bool SchedulerPass::IsMemOperation(InstructionEntry *entry) const {
    if (entry->IsCall())
      return true;
    if (entry->op() == OP_lea)
      return false;
    if (entry->HasBaseRegister() || entry->HasIndexRegister())
      return true;
    /* The above does not handle the case where memory operand is a constant.
     * The code below takes care of that.
     * TODO: Confirm if the below code subsumes the above check and remove the
     * above
     */
    for (int i = 0; i < entry->NumOperands(); i++) {
      if (entry->IsMemOperand(i))
        return true;
    }
    if (entry->HasPrefix(REPE_PREFIX_OPCODE) ||
        entry->HasPrefix(REPNE_PREFIX_OPCODE))
      return true;

// Add others which have an implicit base/disp registers
    switch (entry->op()) {
      case OP_cmpxchg:
      case OP_cmpxchg8b:
      case OP_cmpxchg16b:
      case OP_lfence:
      case OP_mfence:
      case OP_sfence:
      case OP_lock:
      case OP_maskmovdqu:
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
      case OP_ins:
      case OP_stos:
      case OP_lods:
      case OP_scas:
      case OP_xadd:
      case OP_xchg:
      case OP_movs:

        return true;
      default:
        return false;
    }

    return false;
}

bool SchedulerPass::HasPredicateOperation(SchedulerNode *node) const {
  for (MaoEntry *entry = node->first; entry != node->last->next();
       entry = entry->next()) {
    if (entry->IsInstruction()) {
      InstructionEntry *insn = entry->AsInstruction();
      if (IsPredicateOperation(insn))
        return true;
    }
  }
  return false;
}

bool SchedulerPass::IsPredicateOperation(InstructionEntry *entry) const {
  switch (entry->op()) {
    case OP_cmovo:
    case OP_cmovno:
    case OP_cmovb:
    case OP_cmovc:
    case OP_cmovnae:
    case OP_cmovae:
    case OP_cmovnc:
    case OP_cmovnb:
    case OP_cmove:
    case OP_cmovz:
    case OP_cmovne:
    case OP_cmovnz:
    case OP_cmovbe:
    case OP_cmovna:
    case OP_cmova:
    case OP_cmovnbe:
    case OP_cmovs:
    case OP_cmovns:
    case OP_cmovp:
    case OP_cmovnp:
    case OP_cmovl:
    case OP_cmovnge:
    case OP_cmovge:
    case OP_cmovnl:
    case OP_cmovle:
    case OP_cmovng:
    case OP_cmovg:
    case OP_cmovnle:
      return true;
    default:
      return false;
  }
  return false;
}

bool SchedulerPass::HasControlOperation(SchedulerNode *node) const {
  for (MaoEntry *entry = node->first; entry != node->last->next();
       entry = entry->next()) {
    if (entry->IsInstruction()) {
      InstructionEntry *insn = entry->AsInstruction();
      if (IsControlOperation(insn))
        return true;
    }
  }
  return false;
}

bool SchedulerPass::IsControlOperation(InstructionEntry *entry) const {
  if (entry->IsReturn() || entry->IsJump() || entry->IsCondJump())
    return true;
  switch (entry->op()) {
    case OP_leave:
    case OP_hlt:
      return true;
    default:
      return false;
  }
  return false;
}

// Is the transformation profitable for this function
// Right now it checks a list of function names passed as
// a parameter to deciede if the function is profitable or
// not
bool SchedulerPass::IsProfitable(Function *function) {
  // List of comma separated functions to apply this pass
  const char *function_list;
  function_list = GetOptionString("function_list");
  const char *func_name = (function->name()).c_str();
  const char *comma_pos;


  if (strcmp(function_list, "")) {
      while ((comma_pos = strchr(function_list, ';')) != NULL) {
        int len = comma_pos - function_list;
        if (strncmp(func_name, function_list, len) == 0)
          return true;
        function_list = comma_pos+1;
      }
      // The function list might be just a single function name and so
      // we need to compare the list against the given function name
      if ( strcmp(function_list, func_name) == 0 ) {
        return true;
      }
      return false;
  } else {
    return true;
  }
}

REGISTER_PLUGIN_FUNC_PASS("SCHEDULER", SchedulerPass)
}  // namespace
