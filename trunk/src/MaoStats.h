//
// Copyright 2009 Google Inc.
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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#ifndef MAOSTATS_H_
#define MAOSTATS_H_

class Stat {
 public:
  virtual ~Stat() {}
  virtual void Print(FILE *out) = 0;
  void Print() {Print(stdout);}
 private:
};

// Print all stats to the same file.
class Stats {
 public:
  Stats() {
    stats_.clear();
  }
  ~Stats() {
    for (std::map<const char *, Stat *, ltstr>::iterator iter = stats_.begin();
        iter != stats_.end(); ++iter) {
      delete iter->second;
    }
  }
  void Add(const char *name, Stat *stat) {
    MAO_ASSERT(!HasStat(name));
    stats_[name] = stat;
  }

  bool HasStat(const char *name) const {
    return stats_.find(name) != stats_.end();
  }

  Stat *GetStat(const char *name)  {
    MAO_ASSERT(HasStat(name));
    return stats_[name];
  }

  void Print(FILE *out) {
    for (std::map<const char *, Stat *, ltstr>::iterator iter = stats_.begin();
        iter != stats_.end(); ++iter) {
      iter->second->Print(out);
    }
  }
  void Print() {Print(stdout);}
 private:
  std::map<const char *, Stat *, ltstr> stats_;
};

#endif  // MAOSTATS_H_
