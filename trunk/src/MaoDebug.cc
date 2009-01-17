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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <stdlib.h>
#include <stdarg.h>
#include "MaoDebug.h"

// Default value of threshold messages to print
#define MAO_DEFAULT_TRACE_THRESHOLD 0

// Default output for asserts and traces
#define MAO_DEFAULT_ASSERT_OUTPUT stderr
#define MAO_DEFAULT_TRACE_OUTPUT stdout


FILE *MaoDebug::assert_file_ = MAO_DEFAULT_ASSERT_OUTPUT;
FILE *MaoDebug::trace_file_ = MAO_DEFAULT_TRACE_OUTPUT;
unsigned char MaoDebug::trace_treshold_ = MAO_DEFAULT_TRACE_THRESHOLD;

void MaoDebug::Assert(const char *file_name, int line_number,
                      bool condition, const char *expr_string,
                      const char *format_string, ...) {
  if (! condition) {
    fprintf(assert_file_, "*** ASSERT! (%s) failed.", expr_string);
    fprintf(assert_file_, "[%s:%d]\n",
            file_name?file_name:"<unknown>",
            line_number);
    va_list argList;
    va_start(argList, format_string);
    vfprintf(assert_file_, format_string, argList);
    va_end(argList);
    exit(1);
  }
}

void MaoDebug::SetAssertOutPut(FILE *file) {
  assert_file_ = file;
}

void MaoDebug::SetTraceOutPut(FILE *file) {
  trace_file_ = file;
}

void MaoDebug::Trace(const char *file_name, int line_number,
                     unsigned int level, const char *format_string, ...) {
  if( level <= trace_treshold_) {
    fprintf(trace_file_, "*** TRACE! (%d): ", level);
    va_list argList;
    va_start(argList, format_string);
    vfprintf(trace_file_, format_string, argList);
    va_end(argList);
    fprintf(trace_file_, " [%s:%d]\n",  file_name?file_name:"<unknown>",
            line_number);
  }
}
