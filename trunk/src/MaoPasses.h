#ifndef MAP_PASSES_H_INCLUDED_
#define MAP_PASSES_H_INCLUDED_
//
// Copyright 2008 Google Inc.
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
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <list>

#include "MaoOptions.h"
#include "mao.h"

class MaoUnit;
class Function;
class CFG;

// MaoAction
//
// Abstract base class for MAO passes and MAO analyses.  It offers
// generic functionality such as tracing, various dumping methods,
// etc.
//
// tracing level
//     0  - no dumping (don't use directly, this is the default)
//     1  - only high level info
//     2  - medium level granularity
//     3  - give me all you got.
//
//
class MaoAction {
 public:
  MaoAction(const char *name, MaoOptions* mao_options, MaoOption *options);
  virtual ~MaoAction();

  // regular Trace, adds newline to output
  virtual void Trace (unsigned int level, const char *fmt, ...) const;

  // Trace with Continuation, doesn't add newline
  virtual void TraceC(unsigned int level, const char *fmt, ...) const;

  // Options
  MaoOption   *FindOptionEntry(const char *name);
  bool         GetOptionBool(const char *name);
  const char  *GetOptionString(const char *name);
  int          GetOptionInt(const char *name);

  // Timers
  void         TimerStart();
  void         TimerStop();

  // Setters/Getters
  const char  *name() const { return name_; }
  unsigned int tracing_level() const { return tracing_level_; }
  bool         tracing() { return tracing_level_ > 0; }

  void set_tracing_level(unsigned int value) { tracing_level_ = value; }
  void set_db(const char *str);
  void set_da(const char *str);

 private:
  const char   *name_;
  MaoOption    *options_;
  unsigned int  tracing_level_;
  FILE         *trace_file_;
  MaoOptions   *mao_options_;

 protected:
  bool          db_vcg_;
  bool          db_cfg_;
  bool          da_vcg_;
  bool          da_cfg_;
};


// MaoPass
//
// A MaoAction that (potentially) changes the IR.  A MAO pass cannot
// return any values.  It can emit data (either via tracing or some
// other means) to the user, or it can change the IR.
//
//
class MaoPass : public MaoAction {
 public:
  MaoPass(const char *name, MaoOptions* mao_options, MaoOption *options,
          MaoUnit *unit);
  virtual ~MaoPass();

  // Pass implementation
  virtual bool Go() = 0;

  //Main invocation
  virtual bool Run();

 protected:
  MaoUnit *const unit_;
};


// MaoFunctionPass
//
// A MaoPass that operates function by function.
//
//
class MaoFunctionPass : public MaoPass {
 public:
  MaoFunctionPass(const char *name, MaoOptions* mao_options, MaoOption *options,
                  MaoUnit *unit, Function *function);
  virtual ~MaoFunctionPass();

  virtual bool Go() = 0;
  virtual bool Run();

 protected:
  Function *const function_;
};


// MaoPassManager
//
// The Mao Pass Manager.
//
// Allows linking of MaoPass objects into a list and executing this
// list. Very preliminary now, but extensible.
//
class MaoPassManager {
 public:
  typedef MaoPass *(*PassCreator)(MaoUnit *unit);

  MaoPassManager(MaoUnit *unit) : unit_(unit) { }
  ~MaoPassManager() {
    for (std::list<MaoPass *>::iterator pass_iter = pass_list_.begin();
         pass_iter != pass_list_.end(); ++pass_iter) {
      MaoPass *pass = (*pass_iter);
      delete pass;
    }
  }

  // Because the destructor deallocates the member passes, *all*
  // passes must be dynamically allocated with new.
  void LinkPass(MaoPass *pass) {
    pass_list_.push_back(pass);
  }

  template <class Pass>
  static MaoPass *GenericPassCreator(MaoUnit *unit) {
    return new Pass(unit);
  }

  void Run() {
    for (std::list<MaoPass *>::iterator pass_iter = pass_list_.begin();
         pass_iter != pass_list_.end(); ++pass_iter) {
      MaoPass *pass = (*pass_iter);
      pass->TimerStart();
      MAO_ASSERT(pass->Run());
      pass->TimerStop();
    }
  }

 private:
  MaoUnit *unit_;
  std::list<MaoPass *> pass_list_;
};


// MaoFunctionPassManager
//
// The Mao Function Pass Manager.
//
// Allows linking of MaoFunctionPasses.  For each pass, a creator
// function is linked.  When the pass manager executes, it calls the
// create method for each pass on each function.  Then it executes the
// pass and deletes the pass object.  MaoFunctionPassManager is a
// MaoPass, so it can be linked to in a MaoPassManager.
//
class MaoFunctionPassManager : public MaoPass {
 public:
  typedef MaoFunctionPass *(*PassCreator)(MaoUnit *unit, Function *function);

  MaoFunctionPassManager(MaoUnit *unit);

  void LinkPass(PassCreator pass) {
    MAO_ASSERT(pass);
    pass_list_.push_back(pass);
  }

  template <class Pass>
  static MaoFunctionPass *GenericPassCreator(MaoUnit *unit, Function *function) {
    return new Pass(unit, function);
  }

  bool Go();

 private:
  std::list<PassCreator> pass_list_;
};


// Code to maintain the set of available passes
void RegisterUnitPass(const char *name,
                      MaoPassManager::PassCreator creator);
void RegisterFunctionPass(const char *name,
                          MaoFunctionPassManager::PassCreator creator);
MaoPassManager::PassCreator         GetUnitPass(const char *name);
MaoFunctionPassManager::PassCreator GetFunctionPass(const char *name);


//
// Standard Passes
//

// ReadInputPass
//
// Read/parse the input asm file and generate the IR
//
class ReadInputPass : public MaoPass {
 public:
  ReadInputPass(int argc, const char *argv[], MaoUnit *mao_unit);
  bool Go();

 private:
  const int argc_;
  const char **argv_;
};

// AssemblyPass
//
// Pass to dump out the IR in assembly format
//
class AssemblyPass : public MaoPass {
 public:
  AssemblyPass(MaoUnit *mao_unit);
  void PrintAsmSymbolHeader(FILE *out);
  bool Go();
};


// DumpIrPass

//
// Pass to to dump out the IR in IR format
//
class DumpIrPass : public MaoPass {
 public:
  explicit DumpIrPass(MaoUnit *mao_unit);
  bool Go();
};

// DumpSymbolTablePass

//
// Pass to to dump out the symboltable
//
class DumpSymbolTablePass : public MaoPass {
 public:
  explicit DumpSymbolTablePass(MaoUnit *mao_unit);
  bool Go();
};

// TestCFGPass

//
// A pass that runs the CFG, even though no pass needs it.
// Useful for debugging
//
class TestCFGPass : public MaoFunctionPass {
 public:
  explicit TestCFGPass(MaoUnit *mao_unit, Function *function);
  bool Go();
};


// TestRelaxPass

//
// A pass that runs the relaxer, even though no pass needs it.
// Useful for debugging
//
class TestRelaxPass : public MaoFunctionPass {
 public:
  explicit TestRelaxPass(MaoUnit *mao_unit, Function *function);
  bool Go();
};


//
// External Entry Points
//

// This function registers all the statically defined passes.  This
// way the passes are accessible by name.
//
void InitPasses(MaoOptions *opts);


// Find a pass for a given option set
MaoAction *FindPass(MaoOption *arr);


// Pass Initialization Entry Points.
void InitProfileAnnotation();

void InitDCE();
void InitNopKiller();
void InitNopinizer();
void InitZEE();
void InitRedundantTestElimination();
void InitRedundantMemMoveElimination();
void InitMissDispElimination();
void InitLongInstructionSplit();
void InitLoopAlign();
void InitBranchSeparate();
void InitAddAddElimination();
void InitAlignTinyLoops16();

#endif   // MAP_PASSES_H_INCLUDED_
