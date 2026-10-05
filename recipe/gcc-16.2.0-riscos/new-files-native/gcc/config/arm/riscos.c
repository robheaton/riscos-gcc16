/* Host support for GCC running natively on RISC OS (EABI): linked into the compiler proper (cc1, cc1plus, ...) through EXTRA_OBJS.
   Written for the GCCSDK GCC 16 forward port (the throwback code of the GCC 10 riscos.c is not ported yet).

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef CROSS_DIRECTORY_STRUCTURE

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"

/* The maximum size of the heap dynamic area of the compiler proper (cc1, cc1plus ...): 512 MB of reserved logical address space, in bytes.  Everything else that is linked
   with data/riscos-da.o gets UnixLib's default of 32 MB (see there for why the programs that only start others must not reserve more).  The memory is mapped as the heap
   grows.  <program>$HeapMax (an integer, in MB) overrides it.  */
int __dynamic_da_max_size = 512 * 1024 * 1024;

/* The size of the main stack of the compiler proper, in bytes: 64 MB (libunixlib 16.2.0-6 and later; an older libunixlib ignores the variable and the stack stays 1 MB, which is too
   little for deep template instantiation and constexpr evaluation: about 4 KB of stack per level of recursion, so 1 MB ends at a depth of about 250 and the compiler's own default
   limits are 900 (-ftemplate-depth) and 512 (-fconstexpr-depth); GCC itself asks for 64 MB of stack where it can set it).  ARMEABISupport maps the stack when it is touched, so the
   size costs address space only; all EABI stacks of the machine share one range of 256 MB, and when it is short UnixLib tries half the size, and so on.  */
int __stack_size = 64 * 1024 * 1024;

#endif
