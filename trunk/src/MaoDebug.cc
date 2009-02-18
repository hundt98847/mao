///
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <stdlib.h>
#include <stdarg.h>
#include "MaoDebug.h"

// Default value of threshold messages to print
#define MAO_DEFAULT_TRACE_THRESHOLD 0

// Default output for asserts and traces
#define MAO_DEFAULT_ASSERT_OUTPUT stderr
#define ASSERT_PREFIX      "*** ASSERT "


FILE *MaoDebug::assert_file_ = MAO_DEFAULT_ASSERT_OUTPUT;

// Debug Action Management.
MaoDebugAction *debugAction = NULL;

MaoDebugAction::MaoDebugAction() {
  next_ = debugAction;
  debugAction = this;
}

MaoDebugAction::~MaoDebugAction() {
  if (this == debugAction) {
    debugAction = next_;
  } else {
    MaoDebugAction *action = debugAction;
    while (action->next()) {
      if (action->next() == this) {
        action->set_next(action->next()->next());
        break;
      }
      action = action->next();
    }
  }
}



static void ProcessDebugActions(FILE *assert_file) {
  MaoDebugAction *action = debugAction;
  while (action) {
    action->Invoke(assert_file);
    action = action->next();
  }
}

// Main Assert routine.
void MaoDebug::Assert(const char *file_name, int line_number,
                      bool condition, const char *expr_string,
                      const char *format_string, ...) {
  if (! condition) {
    fprintf(assert_file_, ASSERT_PREFIX);
    fprintf(assert_file_, "%s:%d ",
            file_name?file_name:"<unknown>",
            line_number);
    fprintf(assert_file_, " (%s) failed. ", expr_string);

    va_list argList;
    va_start(argList, format_string);
    vfprintf(assert_file_, format_string, argList);
    va_end(argList);
    fprintf(assert_file_, "\n");

    ProcessDebugActions(assert_file_);
    abort();
  }
}

void MaoDebug::SetAssertOutPut(FILE *file) {
  assert_file_ = file;
}
