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

#include <sys/times.h>

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
  MaoTimer() : total_(0) { }

  void Start() {
    struct tms t;

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

 private:
  clock_t total_;
  clock_t start_;
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
  MaoOptions() : write_assembly_(true),
                 assembly_output_file_name_("<stdout>"),
                 output_is_stdout_(true),
                 output_is_stderr_(false),
                 write_ir_(false),
                 help_(false), verbose_(false),
                 timer_print_(false),
                 ir_output_file_name_(0),
                 mao_options_(NULL) {
  }

  ~MaoOptions() {}

  void        Parse(const char *arg, bool collect = true);
  void        Reparse();
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
  const bool output_is_stdout() const { return output_is_stdout_; }
  const bool output_is_stderr() const { return output_is_stderr_; }

  const bool write_assembly() const {return write_assembly_;}
  const bool write_ir() const {return write_ir_;}
  const char *assembly_output_file_name();
  const char *ir_output_file_name();
  void set_assembly_output_file_name(const char *file_name);
  void set_output_is_stderr() { output_is_stderr_ = true; }
  void set_verbose() { verbose_ = true; }
  void set_help() { help_ = true; }
  void set_timer_print() { timer_print_ = true; }

 private:
  bool write_assembly_;
  const char *assembly_output_file_name_;  // The default (NULL) means stdout.
  bool output_is_stdout_;
  bool output_is_stderr_;
  bool write_ir_;
  bool help_;
  bool verbose_;
  bool timer_print_;
  const char *ir_output_file_name_;  // The default (NULL) means stdout.
  char *mao_options_;
};

#endif  // MAOOPTIONS_H_
