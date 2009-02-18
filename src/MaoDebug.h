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

#ifndef MAODEBUG_H_
#define MAODEBUG_H_

#include <stdio.h>

// Default level on generated trace messages
#define MAO_DEFAULT_TRACE_LEVEL 0

// Main assert/trace macros
#define MAO_ASSERT(condition)                         MAO_ASSERT_DBG(condition,"%s", "")
#define MAO_ASSERT_MSG(condition, format_string,...)  MAO_ASSERT_DBG(condition, format_string, ## __VA_ARGS__)
#define MAO_RASSERT(condition)                        MAO_ASSERT_REL(condition, "%s", "")
#define MAO_RASSERT_MSG(condition, format_string,...) MAO_ASSERT_REL(condition, format_string, ## __VA_ARGS__)


// Helper macros for implementation
#ifdef NDEBUG
  #define MAO_ASSERT_DBG(condition,...)
#else
  #define MAO_ASSERT_DBG(condition,...) MaoDebug::Assert((__FILE__), __LINE__, (condition), __STRING(condition), ##  __VA_ARGS__)
#endif
#define MAO_ASSERT_REL(condition,...) MaoDebug::Assert((__FILE__), __LINE__, (condition), __STRING(condition),  ##  __VA_ARGS__)

// Static functions used for asserts
class MaoDebug {
 public:
  // Main assert function
  static void Assert(const char *file_name, int line_number,
                     bool condition, const char *expr_string,
                     const char *format_string, ...)
    __attribute__ ((format (printf, 5, 6)));

  // Change output file for assert
  static void SetAssertOutPut(FILE *file);
  static FILE *assert_file() { return assert_file_; }

 private:
  // Assert file
  static FILE *assert_file_;
}; // MaoDebug


// MaoDebugAction
class MaoDebugAction {
public:
  MaoDebugAction();
  virtual ~MaoDebugAction();
  MaoDebugAction *next() { return next_; }

  void set_next(MaoDebugAction *ptr) { next_ = ptr; }

  // main invocation routine. Must be defined.
  virtual void Invoke(FILE *output) = 0;

private:
  MaoDebugAction *next_;
};

#endif  // MAODEBUG_H_
