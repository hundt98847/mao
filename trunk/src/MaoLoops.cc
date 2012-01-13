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
#include <list>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include "Mao.h"

namespace {

//
// Union/Find algorithm after Tarjan, R.E., 1983, Data Structures
// and Network Algorithms.
//
class UnionFindNode {
  public:
  UnionFindNode() : parent_(NULL), bb_(NULL), loop_(NULL), dfs_(0) {
  }

  // Initialize this node
  //
  void Init(BasicBlock *bb, int dfs_number) {
    MAO_ASSERT(bb);

    parent_ = this;
    bb_     = bb;
    dfs_    = dfs_number;
  }

  // Union/Find Algorithm - The find routine
  //
  // Implemented with Path Compression (inner loops are only
  // visited and collapsed once, however, deep nests would still
  // result in significant traversals)
  //
  UnionFindNode *FindSet() {
    MAO_ASSERT(parent());
    MAO_ASSERT(bb());

    typedef std::list<UnionFindNode *> NodeListType;
    NodeListType nodeList;

    UnionFindNode *node = this;
    while (node != node->parent()) {
      if (node->parent() != node->parent()->parent())
        nodeList.push_back(node);
      node = node->parent();
    }

    // Path Compression, all nodes' parents point to the 1st level parent
    NodeListType::iterator iter = nodeList.begin();
    NodeListType::iterator end  = nodeList.end();
    for (; iter != end; iter++)
      (*iter)->set_parent(node->parent());

    return node;
  }

  // Union/Find Algorithm - The union routine
  //
  // We rely on path compression
  //
  void Union(UnionFindNode *B) {
    MAO_ASSERT(B);

    set_parent(B);
  }


  // Getters/Setters
  //
  UnionFindNode *parent() { return parent_; }
  BasicBlock    *bb()     { return bb_; }
  SimpleLoop    *loop()   { return loop_; }
  int            dfs()    { return dfs_; }

  void           set_parent(UnionFindNode *parent) { parent_ = parent; }
  void           set_loop(SimpleLoop *loop) { loop_ = loop; }

  private:
  UnionFindNode *parent_;
  BasicBlock    *bb_;
  SimpleLoop    *loop_;
  int            dfs_;
};






//------------------------------------------------------------------
// Loop Recognition
//
// based on:
//   Paul Havlak, Nesting of Reducible and Irreducible Loops,
//      Rice University.
//
//   We avoid doing tree balancing and instead use path compression
//   to avoid traversing parent pointers over and over.
//
//   Most of the variable names and identifiers are taken literally
//   from this paper (and the original Tarjan paper mentioned above)
//-------------------------------------------------------------------

class HavlakLoopFinder {
 public:
  HavlakLoopFinder(const CFG *cfg, LoopStructureGraph *lsg) :
    CFG_(cfg), current_(0), lsg_(lsg) {
    MAO_ASSERT(CFG_);
    MAO_ASSERT(lsg_);
  }

  //
  // FindLoops
  //
  // DESCRIPTION:
  // Find loops and build loop forest using Havlak's algorithm, which
  // is derived from Tarjan. Variable names and step numbering has
  // been chosen to be identical to the nomenclature in Havlak's
  // paper (which is similar to the one used by Tarjan)
  //
  void FindLoops() {
    int                size = CFG_->GetNumOfNodes();
    if (!size) return;

    IntSetVector       nonBackPreds(size);
    IntListVector      backPreds(size);
    IntVector          header(size);
    CharVector         type(size);
    IntVector          last(size);
    NodeVector         nodes(size);
    BasicBlockMap      number;
    int                w;

    // Step a:
    //   - initialize all nodes as unvisited
    //   - depth-first traversal and numbering
    //   - unreached BB's are marked as dead
    //
    for (CFG::BBVector::const_iterator bb_iter = CFG_->Begin();
         bb_iter != CFG_->End(); ++bb_iter) {
      number[*bb_iter] = kUnvisited;
    }

    current_ = 0;
    DFS(*CFG_->Begin(), &nodes, &number, &last);

    // DEBUG code - leave it in here for now
    // for (std::list<BasicBlock *>::iterator bb_iter =
    //       CFG_->GetBasicBlocks()->begin();
    //     bb_iter != CFG_->GetBasicBlocks()->end(); ++bb_iter) {
    //  fprintf(stderr,
    //          "Number[bb%d]=%d last[%d]=%d\n",
    //          (*bb_iter)->index(), number[*bb_iter],
    //          (*bb_iter)->index(), last[number[*bb_iter]]
    //          );
    // }


    // Step b:
    //   - iterate over all nodes.
    //
    //   A backedge comes from a descendant in the DFS tree, and non-backedges
    //   from non-descendants (following Tarjan)
    //
    //   - check incoming edges 'v' and add them to either
    //     - the list of backedges (backpreds) or
    //     - the list of non-backedges (nonBackPreds)
    //
    for (w = 0; w < size; w++) {
      header[w] = 0;
      type[w] = BB_NONHEADER;

        BasicBlock *node_w = nodes[w].bb();
        if (!node_w) {
          type[w] = BB_DEAD;
          continue;  // dead BB
        }

        for (BasicBlock::ConstEdgeIterator inedges = node_w->BeginInEdges();
             inedges != node_w->EndInEdges(); ++inedges) {
          BasicBlockEdge *edge   = *inedges;
          BasicBlock     *node_v = edge->source();

          int v = number[ node_v ];
          if (v == kUnvisited) continue;  // dead node

          if (IsAncestor(w, v, last))
            backPreds[w].push_back(v);
          else
            nonBackPreds[w].insert(v);
        }
    }

    // Start node is root of all other loops
    header[0] = 0;

    // Step c:
    //
    // The outer loop, unchanged from Tarjan. It does nothing except
    // for those nodes which are the destinations of backedges.
    // For a header node w, we chase backward from the sources of the
    // backedges adding nodes to the set P, representing the body of
    // the loop headed by w.
    //
    // By running through the nodes in reverse of the DFST preorder,
    // we ensure that inner loop headers will be processed before the
    // headers for surrounding loops.
    //
    for (w = size-1; w >= 0; w--) {
      NodeList  P;
      BasicBlock *node_w = nodes[w].bb();
      if (!node_w) continue;  // dead BB

      // Step d:
      IntList::iterator back_pred_iter  = backPreds[w].begin();
      IntList::iterator back_pred_end   = backPreds[w].end();
      for (; back_pred_iter != back_pred_end; back_pred_iter++) {
        int v = *back_pred_iter;
        if ( v!= w)
          P.push_back(nodes[v].FindSet());
        else
          type[w] = BB_SELF;
      }

      // copy P to worklist
      //
      NodeList worklist;
      NodeList::iterator niter  = P.begin();
      NodeList::iterator nend   = P.end();
      for ( ;  niter != nend; niter++ )
        worklist.push_back(*niter);

      if ( P.size() != 0 )
        type[w] = BB_REDUCIBLE;

      // work the list...
      //
      while (worklist.size()) {
        UnionFindNode x = *worklist.front();
        worklist.pop_front();

        // Step e:
        //
        // Step e represents the main difference from Tarjan's method.
        // Chasing upwards from the sources of a node w's backedges. If
        // there is a node y' that is not a descendant of w, w is marked
        // the header of an irreducible loop, there is another entry
        // into this loop that avoids w.
        //

        // The algorithm has degenerated. Break and
        // return in this case
        //
        int non_back_size = nonBackPreds[x.dfs()].size();
        if (non_back_size > kMaxNonBackPreds) {
          lsg_->KillAll();
          return;
        }

        IntSet::iterator non_back_pred_iter = nonBackPreds[x.dfs()].begin();
        IntSet::iterator non_back_pred_end  = nonBackPreds[x.dfs()].end();
        for (; non_back_pred_iter != non_back_pred_end; non_back_pred_iter++) {
          UnionFindNode  y     = nodes[*non_back_pred_iter];
          UnionFindNode *ydash = y.FindSet();

          if (!IsAncestor(w, ydash->dfs(), last)) {
            type[w] = BB_IRREDUCIBLE;
            nonBackPreds[w].insert(ydash->dfs());
          } else {
            if (ydash->dfs() != w) {
              NodeList::iterator nfind = find(P.begin(), P.end(), ydash);
              if (nfind == P.end()) {
                worklist.push_back(ydash);
                P.push_back(ydash);
              }
            }
          }
        }
      }

      // Collapse/Unionize nodes in a SCC to a single node
      // For every SCC found, create a loop descriptor and link it in.
      //
      if (P.size() || (type[w] == BB_SELF)) {
        SimpleLoop* loop = lsg_->CreateNewLoop();

        loop->set_header(node_w, true);
        IntList::iterator backp_iter  = backPreds[w].begin();
        loop->set_bottom(nodes[*backp_iter].bb());
        loop->set_is_reducible(type[w] != BB_IRREDUCIBLE);

        // At this point, one can set attributes to the loop, such as:
        //
        // the bottom node:
        //    IntList::iterator iter  = backPreds[w].begin();
        //    loop bottom is: nodes[*backp_iter].node);
        //
        // the number of backedges:
        //    backPreds[w].size()
        //
        // whether this loop is reducible:
        //    type[w] != BB_IRREDUCIBLE
        //
        // TODO(rhundt): Define those interfaces in the Loop Forest
        //
        nodes[w].set_loop(loop);

        for (niter = P.begin(); niter != P.end(); niter++) {
          UnionFindNode  *node = (*niter);
          MAO_ASSERT(node);
          MAO_ASSERT(type[w] != BB_NONHEADER);

          // Add nodes to loop descriptor
          header[node->dfs()] = w;
          node->Union(&nodes[w]);

          // Nested loops are not added, but linked together
          if (node->loop())
            node->loop()->set_parent(loop);
          else
            loop->AddNode(node->bb());
        }

        lsg_->AddLoop(loop);
      }  // P.size
    }  // step c

    // Determine nesting relationship and link 1st level loops to root node.
    lsg_->CalculateNestingLevel();
  }  // FindLoops

 private:
  enum BasicBlockClass {
    BB_TOP,          // uninitialized
    BB_NONHEADER,    // a regular BB
    BB_REDUCIBLE,    // reducible loop
    BB_SELF,         // single BB loop
    BB_IRREDUCIBLE,  // irreducible loop
    BB_DEAD,         // a dead BB
    BB_LAST          // Sentinel
  };

  //
  // Local types used for Havlak algorithm, all carefully
  // selected to guarantee minimal complexity ;-)
  //
  typedef std::vector<UnionFindNode>          NodeVector;
  typedef std::map<BasicBlock*, int>          BasicBlockMap;
  typedef std::list<int>                      IntList;
  typedef std::set<int>                       IntSet;
  typedef std::list<UnionFindNode*>           NodeList;
  typedef std::vector<IntList>                IntListVector;
  typedef std::vector<IntSet>                 IntSetVector;
  typedef std::vector<int>                    IntVector;
  typedef std::vector<char>                   CharVector;

  //
  // Constants
  //
  static const int kUnvisited = INT_MAX;
  static const int kMaxNonBackPreds = (32*1024);

  //
  // IsAncestor
  //
  // As described in the paper, determine whether a node 'w' is a
  // "true" ancestor for node 'v'
  //
  // Dominance can be tested quickly using a pre-order trick
  // for depth-first spanning trees. This is why DFS is the first
  // thing we run below.
  //
  bool IsAncestor(int w, int v, const IntVector &last) {
    return ((w <= v) && (v <= last[w]));
  }

  //
  // DFS
  //
  // DESCRIPTION:
  // Simple depth first traversal along out edges with node numbering
  //
  void DFS(BasicBlock      *a,
           NodeVector      *nodes,
           BasicBlockMap   *number,
           IntVector       *last) {
    MAO_ASSERT(a);

    (*nodes)[current_].Init(a, current_);
    (*number)[a] = current_;
    current_++;

    for (BasicBlock::ConstEdgeIterator outedges = a->BeginOutEdges();
         outedges != a->EndOutEdges(); ++outedges) {
      BasicBlock *target = (*outedges)->dest();

      if ((*number)[target] == kUnvisited)
        DFS(target, nodes, number, last);
    }
    (*last)[(*number)[a]] = current_-1;
  }

  //
  // member vars
  //
  const CFG          *CFG_;      // current control flow graph
  int                 current_;  // node id/number
  LoopStructureGraph *lsg_;      // loop forest
};  // HavlakLoopFinder


// Constant instantiations
//   (needed for some compilers)
//
const int HavlakLoopFinder::kUnvisited;
const int HavlakLoopFinder::kMaxNonBackPreds;


// --------------------------------------------------------------------
// Options
// --------------------------------------------------------------------
MAO_DEFINE_OPTIONS(LFIND, "Finds all Havlak loops", 1) {
  OPTION_BOOL("lsg", false, "Dump LSG in text format"),
};

class LoopFinderPass : public MaoFunctionPass {
 public:
  LoopFinderPass(MaoUnit *mao,
                 Function *function,
                 LoopStructureGraph *LSG,
                 bool conservative)
      : MaoFunctionPass("LFIND", GetStaticOptionPass("LFIND"), mao, function),
        LSG_(LSG), conservative_(conservative) {
    dump_lsg_ = GetOptionBool("lsg");
  }

  bool Go() {
    CFG *cfg = CFG::GetCFG(unit_, function_, conservative_);
    MAO_ASSERT(cfg != NULL);
    HavlakLoopFinder Havlak(cfg, LSG_);
    Havlak.FindLoops();

    if (dump_lsg_)
      LSG_->Dump();
    return true;
  }

 private:
  LoopStructureGraph *LSG_;
  bool                dump_lsg_;
  bool                conservative_;
};

}  // namespace

LoopStructureGraph *LoopStructureGraph::GetLSG(MaoUnit *mao,
                                               Function *function,
                                               bool conservative) {
  MAO_ASSERT(function != NULL);
  if (function->lsg() == NULL) {
    LoopStructureGraph *LSG = new LoopStructureGraph;
    LoopFinderPass finder(mao, function, LSG, conservative);
    finder.Go();
    function->set_lsg(LSG);
  }
  MAO_ASSERT(function->lsg());
  return function->lsg();
}

void InitLoops() {
  RegisterStaticOptionPass("LFIND", new MaoOptionMap);
}
