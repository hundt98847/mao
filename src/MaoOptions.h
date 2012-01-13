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
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA.

#ifndef MAOOPTIONS_H_
#define MAOOPTIONS_H_

#include <map>
#include <string>

#include <unistd.h>
#include <sys/times.h>
#include <strings.h>

#include "MaoDebug.h"

class MaoOption;
class MaoPassManager;
class MaoUnit;

enum MaoOptionType {
  OPT_INT, OPT_STRING, OPT_BOOL
};

union MaoOptionValue {
  int         ival_;
  const char *cval_;
  bool        bval_;
};

struct MaoOption {
  MaoOptionType   type() const { return type_; }
  const char     *name() const { return name_; }
  const char     *description() const { return description_; }
  MaoOptionType   type_;
  const char     *name_;
  const char     *description_;
  MaoOptionValue value;
};

typedef std::map<std::string, MaoOptionValue> MaoOptionMap;

// Time for pass executions. There is one timer for each pass, if
// a pass runs multiple times, the times are accumulated.
//
class MaoTimer {
 public:
 MaoTimer() : total_(0), triggered_(false) { }

  void Start() {
    struct tms t;
    triggered_ = true;
    start_ = times(&t);;
  }

  void Stop() {
    struct tms t;
    total_ += (times(&t) - start_);
  }

  void Print(FILE *f) {
    static long ticks = 0;
    if (!ticks)
      ticks = sysconf(_SC_CLK_TCK);
    fprintf(f, "%5.1lf [sec]", 1.0 * total_ / ticks);
  }

  double GetSecs() {
    static long ticks = 0;
    if (!ticks)
      ticks = sysconf(_SC_CLK_TCK);
    return 1.0 * total_ / ticks;
  }

  bool Triggered() const { return triggered_; }

 private:
  clock_t total_;
  clock_t start_;
  bool    triggered_;
};

// This is how to define options, build up an array consisting of
// entries of this type:
#define OPTION_INT(name, val, desc)  { OPT_INT, name, desc, { ival_: val } }
#define OPTION_BOOL(name, val, desc) { OPT_BOOL, name, desc, { bval_: val } }
#define OPTION_STR(name, val, desc)  { OPT_STRING, name, desc, { cval_: val } }

// Define an array of options with help of this macro.
// Usage (please note the final trainling comma):
//    MAO_DEFINE_OPTIONS(pass, description, N) {
//       OPTION_INT(...),
//       OPTION_STR(...),
//       ...,
//    };
//
#define MAO_DEFINE_OPTIONS(pass, description, N) \
            extern MaoOption pass##_opts[]; \
            static MaoTimer  pass##_timer; \
            static MaoOptionRegister pass##_opts_reg(#pass, description, \
                                                     pass##_opts, N, \
                                                     &pass##_timer); \
            MaoOption pass##_opts[N] =

// TODO(eraman): Remove this after the next release so that existing customers
// can switch to the new macro without breaking their code till they switch
#define MAO_OPTIONS_DEFINE(pass, N) \
            extern MaoOption pass##_opts[]; \
            static MaoTimer  pass##_timer; \
            static MaoOptionRegister pass##_opts_reg(#pass, \
                                                     "[None given]", \
                                                     pass##_opts, N, \
                                                     &pass##_timer); \
            MaoOption pass##_opts[N] =

class MaoOptionRegister {
 public:
  MaoOptionRegister(const char *name, const char *description,
                    MaoOption *array, int N, MaoTimer *timer);
};

// Maintain mapping between option array and pass name.
//
class MaoOptionArray {
 public:
  MaoOptionArray(const char *name, const char *description, MaoOption *array,
                 int num_entries, MaoTimer *timer)
    : name_(name), description_(description), array_(array),
    num_entries_(num_entries), timer_(timer){
  }

  MaoOption  *FindOption(const char *option_name) {
    for (int i = 0; i < num_entries_; i++)
      if (!strcasecmp(option_name, array_[i].name()))
        return &array_[i];
    MAO_ASSERT_MSG(false, "Option %s not found", option_name);
    return NULL;
  }

  const char *name() const { return name_; }
  const char *description() const { return description_; }
  MaoOption  *array() { return array_; }
  int         num_entries() { return num_entries_; }
  MaoTimer   *timer() { return timer_; }
 private:
  const char *name_;
  const char *description_;
  MaoOption  *array_;
  int         num_entries_;
  MaoTimer   *timer_;
};

// MaoOptions - The option manager
//
class MaoOptions {
 public:
  MaoOptions() : help_(false), verbose_(false),
                 timer_print_(false),
                 mao_options_(NULL) {
  }

  ~MaoOptions() {}

  // Parse the option string
  //
  //   argv0 is the pointer to argv[0] from main().
  //         We need this path to find plugin .so's.
  //
  //   arg   is the current pointer into the Mao option string
  //         e.g., as it is passed via --mao=...
  //
  void        Parse(const char *argv0, const char *arg, bool collect = true,
                    MaoUnit *unit = NULL, MaoPassManager *pass_man = NULL);
  void        Reparse(MaoUnit *unit = NULL, MaoPassManager *pass_man = NULL);
  void        ProvideHelp(bool exit_after, bool always = false);
  static void SetOption(const char *pass_name,
                        const char *option_name,
                        int         value);
  static void SetOption(const char *pass_name,
                        const char *option_name,
                        bool        value);
  static void SetOption(const char *pass_name,
                        const char *option_name,
                        const char *value);

  void        TimerStart(const char *pass_name);
  void        TimerStop(const char *pass_name);
  static void TimerPrint();

  const bool help() const { return help_; }
  const bool timer_print() const { return timer_print_; }
  const bool verbose() const { return verbose_; }

  void set_verbose() { verbose_ = true; }
  void set_help(bool value) { help_ = value; }
  void set_timer_print() { timer_print_ = true; }

 private:
  void InitializeOptionMap(MaoOptionMap *options, MaoOptionArray *pass_opts);

  bool help_;
  bool verbose_;
  bool timer_print_;
  char *mao_options_;
};

#endif  // MAOOPTIONS_H_
