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

#ifndef MAOOPTIONS_H_
#define MAOOPTIONS_H_

// Symbol table
class MaoOptions {
 public:
  MaoOptions();
  ~MaoOptions();

  const bool write_assembly() const {return write_assembly_;}
  const bool write_ir() const {return write_ir_;}
  const char *assembly_output_file_name();
  const char *ir_output_file_name();
  void set_assembly_output_file_name(const char *file_name);
  void set_ir_output_file_name(const char *file_name);
 private:
  bool write_assembly_;
  const char *assembly_output_file_name_;  // The default (NULL) means stdout.
  bool write_ir_;
  const char *ir_output_file_name_;  // The default (NULL) means stdout.
};

#endif  // MAOOPTIONS_H_
