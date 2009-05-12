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
#include <list>
#include <vector>

#include <stdio.h>
#include <ctype.h>
#include "MaoDebug.h"
#include "MaoOptions.h"
#include "MaoPasses.h"

// Maintain mapping between option array and pass name.
//
class MaoOptionArray {
 public:
  MaoOptionArray(const char *name, MaoOption *array, int num_entries,
                 MaoTimer *timer)
    : name_(name), array_(array), num_entries_(num_entries),
    timer_(timer){
  }

  MaoOption  *FindOption(const char *option_name) {
    for (int i = 0; i < num_entries_; i++)
      if (!strcasecmp(option_name, array_[i].name()))
        return &array_[i];
    MAO_ASSERT_MSG(false, "Option %s not found", option_name);
    return NULL;
  }

  const char *name() const { return name_; }
  MaoOption  *array() { return array_; }
  int         num_entries() { return num_entries_; }
  MaoTimer   *timer() { return timer_; }
 private:
  const char *name_;
  MaoOption  *array_;
  int         num_entries_;
  MaoTimer   *timer_;
};

// Maintain static list of all option arrays.
typedef std::vector<MaoOptionArray *> OptionVector;
static OptionVector *option_array_list;

// Unprocessed flags are passed on to as_main (which is the GNU Assembler
// main function). Everything else is handed to the MAO Option processor
//
void MaoOptions::ProvideHelp(bool always) {
  if (!help() && !always) return;

  fprintf(stderr,
          "Mao %s\n",
          MAO_VERSION);
  fprintf(stderr,
          "Usage: mao [-mao:mao-options]* "
          "[regular-assembler-options]* input-file \n"
          "\n'mao-options' are seperated by commas, and are:\n\n"
          "-h          display this help text\n"
          "-v          verbose (set trace level to 3)\n"
          "-T          output timing information for passes\n"
          "\n"
          "PASS=[phase-option][,phase-option]*\n"
          "\nwith PASS and 'phase-option' being:\n\n"
          "Pass: ALL\n"
          "  trace     : (int)    Set trace level to 'val' (0..3)\n"
          "  db[parm]  : (bool)   Dump before a pass\n"
          "  da[parm]  : (bool)   Dump after  a pass\n"
          "     with parm being one of:\n"
          "        cfg : dump CFG, if available\n"
          "        vcg : dump VCG file, if CFG is available\n"
          );

  for (OptionVector::iterator it = option_array_list->begin();
       it != option_array_list->end(); ++it) {
    fprintf(stderr, "Pass: %s\n", (*it)->name());
    MaoOption *arr = (*it)->array();
    for (int i = 0; i < (*it)->num_entries(); i++) {
      fprintf(stderr, "  %-10s: %7s %s\n",
              arr[i].name(),
              arr[i].type() == OPT_INT ? "(int)   " :
              arr[i].type() == OPT_BOOL ? "(bool)  " :
              arr[i].type() == OPT_STRING ? "(string)" :
              "(unk)",
              arr[i].description());
    }
  }
  exit(0);
}

void MaoOptions::TimerPrint() {
  double total_secs = 0.0;
  for (OptionVector::iterator it1 = option_array_list->begin();
       it1 != option_array_list->end(); ++it1) {
    total_secs += (*it1)->timer()->GetSecs();
  }

  fprintf(stderr, "Timing information for passes\n");
  for (OptionVector::iterator it = option_array_list->begin();
       it != option_array_list->end(); ++it) {
    if ((*it)->timer()->Triggered()) {
      fprintf(stderr, "  Pass: %-12s %5.1lf [sec] %5.1lf%%\n",
	      (*it)->name(), (*it)->timer()->GetSecs(),
	      100.0 * (*it)->timer()->GetSecs() / total_secs);
    }
  }
  fprintf(stderr, "Total accounted for: %5.1lf [sec]\n", total_secs );
}


// Simple (static) constructor to allow registration of
// option arrays.
//
MaoOptionRegister::MaoOptionRegister(const char *name,
                                     MaoOption  *array,
                                     int N,
                                     MaoTimer   *timer) {
  MaoOptionArray *tmp = new MaoOptionArray(name, array, N, timer);
  if (!option_array_list) {
    option_array_list = new OptionVector;
  }
  option_array_list->push_back(tmp);
}

static MaoOptionArray *FindOptionArray(const char *pass_name) {
  for (OptionVector::iterator it = option_array_list->begin();
       it != option_array_list->end(); ++it) {
    if (!strcasecmp((*it)->name(), pass_name))
      return (*it);
  }
  MAO_ASSERT_MSG(false, "Can't find passname: %s", pass_name);
  return NULL;
}

void MaoOptions::SetOption(const char *pass_name,
                           const char *option_name,
                           int         value) {
  MaoOptionArray *entry = FindOptionArray(pass_name);
  MaoOption      *option = entry->FindOption(option_name);
  MAO_ASSERT(option->type() == OPT_INT);

  option->ival_ = value;
}

void MaoOptions::SetOption(const char *pass_name,
                           const char *option_name,
                           bool        value) {
  MaoOptionArray *entry = FindOptionArray(pass_name);
  MaoOption      *option = entry->FindOption(option_name);
  MAO_ASSERT(option->type() == OPT_BOOL);

  option->bval_ = value;
}

void MaoOptions::SetOption(const char *pass_name,
                           const char *option_name,
                           const char *value) {
  MaoOptionArray *entry = FindOptionArray(pass_name);
  MaoOption      *option = entry->FindOption(option_name);
  MAO_ASSERT(option->type_ == OPT_STRING);

  option->cval_ = value;
}

void MaoOptions::TimerStart(const char *pass_name) {
  MaoOptionArray *entry = FindOptionArray(pass_name);
  entry->timer()->Start();
}

void MaoOptions::TimerStop(const char *pass_name) {
  MaoOptionArray *entry = FindOptionArray(pass_name);
  entry->timer()->Stop();
}

static char *NextToken(const char *arg, const char **next, char *token_buff) {
  int i = 0;
  const char *p = arg;
  if (*p == ',' || *p == ':' || *p == '=' || *p == '+')
    ++p;
  while (isalnum(*p) || (*p == '_') ||
         (*p == '/') ||
         (*p == '.') || (*p == '-'))
    token_buff[i++] = *p++;

  token_buff[i]='\0';
  *next = p;
  return token_buff;
}

static const char *GobbleGarbage(const char *arg, const char **next) {
  const char *p = arg;
  if (*p == ',' || *p == ':' || *p == '=')
    ++p;
  *next = p;
  return p;
}

// At the current parameter location, check if we have
// a parameter, e.g.:
//    option:val
//    option(val)
//    option[val]
//
static bool GetParam(const char *arg, const char **next, const char **param,
                     char *token_buff) {
  if (arg[0] == '(' || arg[0] == '[') {
    char  delim = arg[0];

    ++arg;
    *param = NextToken(arg, &arg, token_buff);

    if (delim != ':') {
      MAO_ASSERT_MSG(arg[0] == ')' || arg[0] == ']',
                     "Ill formatted parameter to option: %s", arg);
      ++arg;  // skip closing bracket
    }
    *next = arg;
    return true;
  } else {
    *next = arg;
    *param = NULL;
    return false;
  }
}

// See whether an option is a "pass-specific" options, an option
// that applies to passes, but is not specified in the pass' option
// array.
//
bool SetPassSpecificOptions(const char *option, const char *arg,
                            const char **next, char *token_buff,
                            MaoOptionArray *current_opts) {
  if (!strcasecmp(option, "trace")) {
    const char *param;
    int   level = 1;
    if (GetParam(arg, next, &param, token_buff))
      level = atoi(param);
    MaoAction *mao_pass = FindPass(current_opts->array());
    if (mao_pass)
      mao_pass->set_tracing_level(level);
    return true;
  }
  if (!strcasecmp(option, "db")) {
    const char *param;
    MaoAction *mao_pass = FindPass(current_opts->array());
    GetParam(arg, next, &param, token_buff);
    if (mao_pass && param)
      mao_pass->set_db(param);
    return true;
  }
  if (!strcasecmp(option, "da")) {
    const char *param;
    MaoAction *mao_pass = FindPass(current_opts->array());
    GetParam(arg, next, &param, token_buff);
    if (mao_pass && param)
      mao_pass->set_da(param);
    return true;
  }
  return false;
}

// Reparse the accumulated option strings. The reason for reparsing is
// that dynamically created passes are not visible at standard option
// parsing time. We therefore reparse on pass creation.
//
void MaoOptions::Reparse(MaoUnit *unit, MaoPassManager *pass_man) {
  Parse(mao_options_, false, unit, pass_man);
}

void MaoOptions::Parse(const char *arg, bool collect,
                       MaoUnit *unit, MaoPassManager *pass_man) {
  MaoFunctionPassManager *func_pass_man = NULL;

  if (!arg) return;

  if (collect) {
    if (!mao_options_) {
      mao_options_ = strdup(arg);
    } else {
      // Append mao_options_ and arg
      char *buf = (char *)malloc(sizeof(char) * (strlen(mao_options_) +
                                                 strlen(arg) +
                                                 1 +
                                                 1));
      sprintf(buf, "%s:%s", mao_options_, arg);
      free(mao_options_);
      mao_options_ = buf;
    }
  }

  char *token_buff = new char[strlen(arg)+1];
  while (arg && arg[0]) {
    GobbleGarbage(arg, &arg);

    // Standard options start with a '-'.
    //
    if (arg[0] == '-') {
      ++arg;
      if (arg[0] == 'v') {
        set_verbose();
        ++arg;
      } else if (arg[0] == 'h') {
        set_help();
        ++arg;
      } else if (arg[0] == 'T') {
        set_timer_print();
        ++arg;
      } else {
        fprintf(stderr, "Invalid Option starting with: %s\n", arg);
        ++arg;
      }
      continue;
    }

    // Named passes start with a regular character,
    // have an identifier (a valid pass name), and
    // are followed by either '=' or ':'.
    //
    if (isascii(arg[0])) {
      char *pass_name = NextToken(arg, &arg, token_buff);

      if (pass_name[0] != '\0') {
        if (pass_man) {
          MaoPassManager::PassCreator unit_creator = GetUnitPass(pass_name);
          if (unit_creator) {
            pass_man->LinkPass(unit_creator(unit));
            func_pass_man = NULL;
          } else {
            MaoFunctionPassManager::PassCreator func_creator =
                GetFunctionPass(pass_name);
            if (func_creator) {
              if (!func_pass_man) {
                func_pass_man = new MaoFunctionPassManager(unit);
                pass_man->LinkPass(func_pass_man);
              }
              func_pass_man->LinkPass(func_creator);
            } else {
              MAO_ASSERT_MSG(false, "Can't find passname: %s", pass_name);
            }
          }
        }

        if (arg[0] == '=') {
          MaoOptionArray *current_opts = FindOptionArray(pass_name);
          MAO_ASSERT(current_opts);

          char *option;
          while (1) {
            const char *old_arg = arg;
            // should we start a new parse group?
            if (arg[0] == ':')
              break;

            option = NextToken(arg, &arg, token_buff);
            if (!option || option[0] == '\0')
              break;

            // if the option is a passname, the next character will be a '='
            // Then we need to exit the option parsing and process the next PASS
            //
            if (arg[0] == '=') {
              arg = old_arg;
              break;
            }
            if (SetPassSpecificOptions(option, arg, &arg, token_buff,
                                       current_opts))
              continue;

            MaoOption *opt = current_opts->FindOption(option);
            MAO_ASSERT_MSG(opt, "Could not find option: %s", option);

            const char *param;
            if (GetParam(arg, &arg, &param, token_buff)) {
              if (opt->type() == OPT_INT)
                opt->ival_ = atoi(param);
              else if (opt->type() == OPT_STRING)
                opt->cval_ = strdup(param);
              else if (opt->type() == OPT_BOOL) {
                if (param[0] == '0' ||
                    param[0] == 'n' || param[0] == 'N' ||
                    param[0] == 'f' || param[0] == 'F' ||
                    (param[0] == 'o' && param[1] == 'f'))
                  opt->bval_ = false; else
                  if (param[0] == '1' ||
                      param[0] == 'y' || param[0] == 'Y' ||
                      param[0] == 't' || param[0] == 'T' ||
                      (param[0] == 'o' && param[1] == 'n'))
                    opt->bval_ = true;
              }
            } else {
              if (opt->type() == OPT_BOOL)
                opt->bval_ = true;
              else
                MAO_ASSERT_MSG(false, "non-boolean option %s used as boolean",
                               option);
            }
            if (arg[0] == '\0') break;
            if (arg[0] == ':' || arg[0] == '|' || arg[0] == ';') {
              ++arg;
              break;
            }
            ++arg;
          }
          GobbleGarbage(arg, &arg);
        }

        continue;
      }
    }

    fprintf(stderr, "Unknown input: %s\n", arg);
    ++arg;
  }
  delete [] token_buff;
}
