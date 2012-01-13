//
// Copyright 2009 Google Inc.
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

#ifndef MAOPLUGIN_H_
#define MAOPLUGIN_H_

#include "MaoUnit.h"

struct PluginVersion {
  unsigned int major;
  unsigned int minor;
};

#define PLUGIN_VERSION                                                  \
  extern "C" {                                                          \
    PluginVersion mao_plugin_version = {MAO_MAJOR_VERSION, MAO_MINOR_VERSION}; \
  }

// Load single fully specified plugin.so file.
void LoadPlugin(const char *path, bool verbose);

// Given MAO's binary path, find and scan all possible
// plugins, following this algorithm:
//
//  Extract realpath from invocation of mao-x86_64-linux, e.g.:
//     /home/rhundt/mao/bin/
//  Look for Mao*.so in  /home/rhundt/mao/bin/Mao*.so
//  Look for Mao*.so in  /home/rhundt/mao/lib/Mao*.so
//
void ScanAndLoadPlugins(const char *argv0, bool verbose);

#endif  // MAOPLUGIN_H_
