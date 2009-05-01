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
// along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef MAOBRANCH_SEPARATE_H_
#define MAOBRANCH_SEPARATE_H_

#include <stdio.h>
#include <list>
#include <vector>
#include <map>

#include "MaoDebug.h"
#include "MaoLoops.h"
#include "MaoUnit.h"
#include "MaoPasses.h"
#include "MaoRelax.h"

// External entry point
void DoBranchSeparate(MaoUnit *mao_unit, Function *function);

#endif  // MAOBRANCH_SEPARATE_H_
