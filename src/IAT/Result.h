//
// Copyright 2009 and later Google Inc.
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
// along with this program; if not, write to the
//   Free Software Foundation Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// Author: Casey Burkhardt (caseyburkhardt)
// Description: File serves as the primary declaration source for the Result
//              class, which represents the abstracted version of a single
//              result generated from the runner script.
//

#ifndef IAT_RESULT_H_
#define IAT_RESULT_H_

#include <string>

using namespace std;

// Result class represents an abstracted version of a single result generated
// by the runner script.  It appears to be most logical to create a result
// object with a constructor with no arguments and add the information as it is
// determined in the Analyzer.
class Result {
 public:
  // Constructor
  Result();

  // Destructor
  virtual ~Result();

  // Accessors and Mutators
  void set_operation_name(string data);
  string operation_name();
  void set_addressing_mode(string data);
  string addressing_mode();
  void set_events_per_instruction(int value);
  int events_per_instruction();
  void set_raw_event_count(long int value);
  long int raw_event_count();

  // Debug
  string OutputString();

 private:
  string operation_name_;
  string addressing_mode_;

  int events_per_instruction_;
  long int raw_instruction_count_;
};

#endif /* IAT_RESULT_H_ */
