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

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "Mao.h"

void LoadPlugin(const char *path, bool verbose) {
  if (verbose)
    fprintf(stderr, "  Loading plugin: %s\n", path);

  char *error;
  void *lib_handle = dlopen(path, RTLD_NOW);

  MAO_ASSERT_MSG(lib_handle, "%s",  dlerror());

  // Clear any pending dlerror messages
  dlerror();

  // Load the version symbol from the plugin
  PluginVersion *version;
  version = static_cast<PluginVersion *>(dlsym(lib_handle, "mao_plugin_version"));
  if ((error = dlerror()) != NULL)
    MAO_ASSERT_MSG(false, "%s", error);

  MAO_ASSERT_MSG(version->major == MAO_MAJOR_VERSION,
                 "Plugin version %d.%d does not match MAO version %d.%d",
                 version->major, version->minor,
                 MAO_MAJOR_VERSION, MAO_MINOR_VERSION);

  // Load the init function from the plugin
  void (*init)();

  // See the dlopen man page for an explanation of the strange casting
  init = (void (*)())dlsym(lib_handle, "MaoInit");
  if ((error = dlerror()) != NULL)
    MAO_ASSERT_MSG(false, "%s", error);

  init();
}

// Allow names like Mao*.so
//
int NameFilter(const struct dirent *d) {
  const char *name = d->d_name;
  int len = strlen(name);
  if (len < 6)
    return 0;
  if (name[0] == 'M' &&
      name[1] == 'a' &&
      name[2] == 'o' &&
      name[len-3] == '.' &&
      name[len-2] == 's' &&
      name[len-1] == 'o')
    return 1;
  return 0;
}

static int ScanDir(const char *dir, bool verbose) {
  struct dirent **namelist;
  int n;

  n = scandir(dir, &namelist, NameFilter, alphasort);
  if (n <= 0)
    return 0;
  for (int i = 0; i < n; ++i) {
    char *fullpath = (char *)  malloc(strlen(dir)
                                      + strlen(namelist[i]->d_name) + 1);
    MAO_ASSERT(fullpath != NULL);

    strcpy(fullpath, dir);
    strcat(fullpath, namelist[i]->d_name);

    LoadPlugin(fullpath, verbose);

    free(namelist[i]);
    free(fullpath);
  }
  free(namelist);
  return n;
}

void ScanAndLoadPlugins(const char *argv0, bool verbose) {
  char *path = realpath(argv0, NULL);
  char *rslash = strrchr(path, '/');
  if (!rslash)
    path = (char *) "./";
  else
    *(rslash+1) = '\0';

  // get plugins from same directory where mao-x86... lives
  //
  if (verbose)
    fprintf(stderr, "Scanning plugins from: %s\n", path);
  ScanDir(path, verbose);

  // next go to ../lib
  //
  const char *libdir = "../lib";
  char *fullpath = (char *) malloc(strlen(path)
                                   + strlen(libdir) + 1);
  MAO_ASSERT(fullpath != NULL);

  strcpy(fullpath, path);
  strcat(fullpath, libdir);

  if (verbose)
    fprintf(stderr, "Scanning plugins from: %s\n", fullpath);
  ScanDir(fullpath, verbose);
  free(fullpath);
}
