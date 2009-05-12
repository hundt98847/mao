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

#include <unistd.h>
#include <sys/times.h>

class MaoUnit;
class MaoPassManager;

enum MaoOptionType {
  OPT_INT, OPT_STRING, OPT_BOOL
};

typedef struct MaoOption {
  MaoOptionType   type() const { return type_; }
  const char     *name() const { return name_; }
  const char     *description() const { return description_; }
  MaoOptionType   type_;
  const char     *name_;
  const char     *description_;
  union {
    int         ival_;
    const char *cval_;
    bool        bval_;
  };
};

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
//    MAO_OPTIONS_DEFINE {
//       OPTION_INT(...),
//       OPTION_STR(...),
//       ...,
//    };
//
#define MAO_OPTIONS_DEFINE(pass, N)  \
            extern MaoOption pass##_opts[]; \
            static MaoTimer  pass##_timer; \
            static MaoOptionRegister pass##_opts_reg(#pass, pass##_opts, N, \
                       &pass##_timer); \
            MaoOption pass##_opts[N] =


// Provide option array name.
#define MAO_OPTIONS(pass) pass##_opts

class MaoOptionRegister {
 public:
  MaoOptionRegister(const char *name, MaoOption *array, int N,
                    MaoTimer *timer);
};

class MaoOptions {
 public:
  MaoOptions() : help_(false), verbose_(false),
                 timer_print_(false),
                 mao_options_(NULL) {
  }

  ~MaoOptions() {}

  void        Parse(const char *arg, bool collect = true,
                    MaoUnit *unit = NULL, MaoPassManager *pass_man = NULL);
  void        Reparse(MaoUnit *unit = NULL, MaoPassManager *pass_man = NULL);
  void        ProvideHelp(bool always = false);
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
  void set_help() { help_ = true; }
  void set_timer_print() { timer_print_ = true; }

 private:
  bool help_;
  bool verbose_;
  bool timer_print_;
  char *mao_options_;
};

#endif  // MAOOPTIONS_H_
