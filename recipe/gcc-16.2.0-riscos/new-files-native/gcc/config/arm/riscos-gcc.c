/* Driver-side hooks for GCC running natively on RISC OS (EABI): linked into the gcc driver through EXTRA_GCC_OBJS.
   Based on the GCC 10 version by Nick Burrett and John Tytgat; the memory check of the old APCS-32 days (Wimp_SlotSize) is gone:
   an EABI program does not live in the Wimp slot, its stack and heap are dynamic areas.

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
#include "obstack.h"

#include <unixlib/local.h>

/* Where the shared libraries live on a machine that can run EABI programs (PackMan's SharedLibs-C-armeabihf): ld needs them for the symbols that libunixlib.so
   gets from libgcc_s.so.1 when a program is linked with -static-libgcc.  */
static const char shared_libs_dir[] = "/SharedLibs:lib.armeabihf";

/* Called from the gcc driver (GCC_DRIVER_HOST_INITIALIZATION) before anything else.  */
void
riscos_host_initialisation (void)
{
}

/* Called from the gcc driver to convert a RISC OS filename into a Unix format name.  When DO_EXE is non-zero the name is not converted: the executable's
   output filename is passed on to collect2's command line as it is.  */
const char *
riscos_convert_filename (void *obstack, const char *name, int do_exe, int do_obj ATTRIBUTE_UNUSED)
{
  char tmp[1024];

  if (do_exe)
    return name;

  if (!__unixify_std (name, tmp, sizeof (tmp), 0))
    return name;

  return (const char *) obstack_copy0 ((struct obstack *) obstack, tmp, strlen (tmp));
}

/* Called by the spec parser in gcc.cc for %:riscos_multilib_dir () in SUBTARGET_EXTRA_LINK_SPEC (riscos-gnueabihf.h): extra linker options that let ld find
   the shared libraries.  */
const char *
riscos_multilib_dir (int argc ATTRIBUTE_UNUSED, const char **argv ATTRIBUTE_UNUSED)
{
  return concat ("-L", shared_libs_dir, " -rpath-link ", shared_libs_dir, NULL);
}

#endif
