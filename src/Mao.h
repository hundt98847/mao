//
// Copyright 2008 Google Inc.
//
// This program is free software; you can redistribute it and/or to
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
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor,
//   Boston, MA  02110-1301, USA.

// Umbrella include file. Most passes should just include this one file

#ifndef __MAO_H_INCLUDED
#define __MAO_H_INCLUDED

#include <math.h>
#include <stdio.h>

#include "MaoDebug.h"
#include "MaoOptions.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoCFG.h"
#include "MaoDefs.h"
#include "MaoLoops.h"
#include "MaoRelax.h"
#include "MaoPlugin.h"
#include "MaoLiveness.h"
#include "MaoReachingDefs.h"
#include "MaoLoops.h"

#define MAO_REVISION "$Rev: 751 $"
#define MAO_VERSION "0.2 " MAO_REVISION

// gas main entry point
extern "C" {
// gas main entry point
int as_main (int argc, char ** argv);
const char *get_default_arch ();
}

#endif // __MAO_H_INCLUDED
