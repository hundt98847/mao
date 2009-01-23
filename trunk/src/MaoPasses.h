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

// MaoPass
//
// Base class for every pass in MAO. All other pieces of functionality
// should be derived from this base class, as it offers generic
// functionality, such as tracing, enabling/disabling, various
// dumping methods, and so on.
//
// tracing level
//     0  - no dumping (don't use directly, this is the default)
//     1  - only high level info
//     2  - medium level granularity
//     3  - give me all you got.
//
class MaoPass {
  public:
  MaoPass(const char *name) : name_(name), enabled_(true), tracing_level_(3),
    trace_file_(stderr) {
  }

  virtual void Trace(unsigned int level, const char *fmt, ...);

  // Main invocation (depends on enabled())
  virtual bool Go() = 0;

  // Setters/Getters
  const char  *name() { return name_; }
  bool         enabled() { return enabled_; };
  unsigned int tracing_level() { return tracing_level_; }
  bool         tracing() { return tracing_level_ > 0; }

  void set_enabled(bool value) { enabled_ = value; }
  void set_tracing_level(unsigned int value) { tracing_level_ = value; }

  private:
  const char   *name_;
  bool          enabled_;
  unsigned int  tracing_level_;
  FILE         *trace_file_;
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
  MaoPassManager() {
  }

  void AddNewPass(MaoPass *pass) {
    pass_list_.push_back(pass);
  }

  MaoPass *FindPass(const char *name) {
    for (std::list<MaoPass *>::iterator piter = pass_list_.begin();
         piter != pass_list_.end(); ++piter) {
      MaoPass *pass = (*piter);
      if (!strcmp(pass->name(), name))
        return pass;
    }
    return NULL;
  }

  void LinkPass(MaoPass *pass) {
    MAO_ASSERT(pass_list_.size());
    LinkPass(FindPass("END"), pass);
  }

  void LinkPass(MaoPass *here, MaoPass *new_pass, bool before = true) {
    for (std::list<MaoPass *>::iterator piter = pass_list_.begin();
         piter != pass_list_.end(); ++piter) {
      MaoPass *pass = (*piter);
      if (pass == here) {
        if (before)
          pass_list_.insert(piter, new_pass);
        else
          pass_list_.insert(++piter, new_pass);
        return;
      }
    }
  }

  void Run() {
    for (std::list<MaoPass *>::iterator piter = pass_list_.begin();
         piter != pass_list_.end(); ++piter) {
      MaoPass *pass = (*piter);
      fflush(stderr);
      if (pass->enabled()) {
        MAO_ASSERT(pass->Go());
      }
    }
  }

  private:
  std::list<MaoPass *> pass_list_;
};


//
// External Entry Points
//

// This function links together a begin pass (BEG) and end pass (END)
// and return a pointer to the static pass manager object.
//
MaoPassManager *InitPasses();
