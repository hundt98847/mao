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

#include <iostream>

#include <stdio.h>
#include "MaoDebug.h"
#include "MaoOptions.h"

// Class MaoOptions

MaoOptions::MaoOptions()
    : write_assembly_(false),
      assembly_output_file_name_(0),
      write_ir_(false),
      ir_output_file_name_(0) {
}

MaoOptions::~MaoOptions() {}

const char *MaoOptions::assembly_output_file_name() {
  return assembly_output_file_name_;
}

const char *MaoOptions::ir_output_file_name() {
  return ir_output_file_name_;
}

void MaoOptions::set_assembly_output_file_name(const char *file_name) {
  MAO_ASSERT(file_name);
  write_assembly_ = true;
  assembly_output_file_name_ = file_name;
}

void MaoOptions::set_ir_output_file_name(const char *file_name) {
  MAO_ASSERT(file_name);
  write_ir_ = true;
  ir_output_file_name_ = file_name;
}