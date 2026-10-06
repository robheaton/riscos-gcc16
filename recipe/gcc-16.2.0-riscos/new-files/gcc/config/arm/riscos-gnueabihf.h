/* Configuration file for ARM RISCOS EABI target.
   Copyright (C) 2004-2018 Free Software Foundation, Inc.
   Contributed by CodeSourcery, LLC   

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

/* This defaults us to little-endian.  */
#ifndef TARGET_ENDIAN_DEFAULT
#define TARGET_ENDIAN_DEFAULT 0
#endif

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT (TARGET_ENDIAN_DEFAULT)

#undef  TARGET_DEFAULT_FLOAT_ABI
#define TARGET_DEFAULT_FLOAT_ABI ARM_FLOAT_ABI_HARD

/* Use the AAPCS ABI by default.  */
#undef ARM_DEFAULT_ABI
#define ARM_DEFAULT_ABI ARM_ABI_AAPCS

#undef ARM_UNWIND_INFO
#define ARM_UNWIND_INFO 1

#undef INIT_SECTION_ASM_OP
#undef FINI_SECTION_ASM_OP
#define INIT_ARRAY_SECTION_ASM_OP ARM_EABI_CTORS_SECTION_OP
#define FINI_ARRAY_SECTION_ASM_OP ARM_EABI_DTORS_SECTION_OP

/* -pg: the AAPCS profiling call of bpabi.h.  The legacy one of arm.h (mov ip, lr ; bl mcount) assumes an APCS frame and hands the caller's return address over in ip,
   which the PLT stub of a call into libunixlib.so overwrites.  __gnu_mcount_nc (libunixlib) is entered with the caller's return address on the stack and no counter word.  */
#undef  NO_PROFILE_COUNTERS
#define NO_PROFILE_COUNTERS 1
#undef  ARM_FUNCTION_PROFILER
#define ARM_FUNCTION_PROFILER(STREAM, LABELNO)	\
{						\
  fprintf (STREAM, "\tpush\t{lr}\n");		\
  fprintf (STREAM, "\tbl\t__gnu_mcount_nc\n");	\
}

#undef  LIB_SPEC
#define LIB_SPEC \
  "%{!nostdlib:-lunixlib }"
  
#define DYNAMIC_LINKER "ld-riscos/so/2"
#define RISCOS_ABI "armeabihf"

#ifdef CROSS_DIRECTORY_STRUCTURE
#define SUBTARGET_EXTRA_LINK_SPEC " -m armelf_riscos_eabi -p \
     %{!static: \
      %{!fpic:-fPIC} %{fpic:-fpic}}"
#else
extern const char * riscos_multilib_dir (int argc, const char **argv);
#undef EXTRA_SPEC_FUNCTIONS
#define EXTRA_SPEC_FUNCTIONS			\
      MCPU_MTUNE_NATIVE_FUNCTIONS		\
      ASM_CPU_SPEC_FUNCTIONS			\
      CANON_ARCH_SPEC_FUNCTION			\
      CANON_ARCH_MULTILIB_SPEC_FUNCTION		\
      TARGET_MODE_SPEC_FUNCTIONS		\
      BE8_SPEC_FUNCTION				\
      { "riscos_multilib_dir", riscos_multilib_dir },

/* When building the native RISC OS compiler, we add an extra library path
   GCCSOLib:  */
#define SUBTARGET_EXTRA_LINK_SPEC \
   "-m armelf_riscos_eabi -p \
   %{!static: \
     %{!fpic:-fPIC} %{fpic:-fpic} \
     %:riscos_multilib_dir()}"

#endif

/* -mmodule: a RISC OS relocatable module (modkit, no C library; the GCCSDK 4.7.4 name of the option).  The code is for ARMv6 and later, soft float, ARM state, freestanding, not
   position independent (the image is linked at address 0 and relocates itself: modkit/README.md), with no stack protector and no unwind tables; the compiler's own
   headers come after the few of modkit (<prefix>/lib/gcc/TARGET/VERSION/include-modkit), the UnixLib headers are not searched, and no start files and libraries are linked
   except libmodkit.a (ENDFILE_SPEC).  The configured default architecture (armv7-a) is already a switch when the self specs run, so it is replaced: -march=armv6
   (or -march=armv6k ... if given), soft float, whatever -mcpu / -mfpu / -mfloat-abi say.  The image has no relocation type for movw / movt (modkit/bin/modreloc.py).  */
#define RISCOS_MODULE_SELF_SPEC						   \
  " %{mmodule:%{!march=armv6*:%<march=* -march=armv6} %<mcpu=* %<mfpu=* %<mfloat-abi=* -mfloat-abi=soft -marm -ffreestanding -fno-pic -fno-pie -fvisibility=hidden" \
  " -fno-stack-clash-protection -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -fno-builtin" \
  " -fno-tree-loop-distribute-patterns -nostdinc -nodefaultlibs}"

#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC " %{mmodule:-iwithprefixbefore include-modkit -iwithprefix include}"

/* Same as arm.h's DRIVER_SELF_SPECS (including the entries added since GCC 10,
   ARCH_CPU_CLEANUP_SPECS and MULTILIB_ARCH_CANONICAL_SPECS), plus the RISC OS
   defaults: NEON implies hard-float on a Cortex-A8, and no unaligned access.  */
#undef DRIVER_SELF_SPECS
#define DRIVER_SELF_SPECS						   \
  ARCH_CPU_CLEANUP_SPECS,						   \
  RISCOS_MODULE_SELF_SPEC,						   \
  " %{mfpu=neon:%{!mfloat-abi=*:-mfloat-abi=hard} %{!mcpu=*:-mcpu=cortex-a8}}"   \
  " %{!munaligned-access:-mno-unaligned-access}" \
  MCPU_MTUNE_NATIVE_SPECS,			\
  TARGET_MODE_SPECS,				\
  MULTILIB_ARCH_CANONICAL_SPECS,		\
  ARCH_CANONICAL_SPECS


#undef STARTFILE_SPEC
#define STARTFILE_SPEC	" %{mmodule:;:crti.o%s" \
			" %{!shared:%{pg:gcrt0.o%s;:crt0.o%s}}" \
			" %{shared:crtbeginS.o%s;:crtbegin.o%s}}"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC	" %{mmodule:libmodkit.a%s;:%{shared:crtendS.o%s;:crtend.o%s}" \
			" crtn.o%s}"

/* -mthrowback: the assembler and the linker also send their errors and warnings to the text editor (binutils patch 05-throwback).  */
#undef  SUBTARGET_EXTRA_ASM_SPEC
#define SUBTARGET_EXTRA_ASM_SPEC " %{mthrowback:--throwback}"

#undef  LINK_SPEC
#define LINK_SPEC "%{mmodule:-m armelf_riscos_eabi -T module.ld%s -static -q -X --no-warn-rwx-segments %{h*} %{version:-v} %{b} %{Wl,*:%*} %{mthrowback:--throwback};: \
   %{h*} %{version:-v} \
   %{b} %{Wl,*:%*} %{mthrowback:--throwback} \
   %{static:-Bstatic} \
   %{shared:-shared} \
   %{symbolic:-Bsymbolic} \
   %{!static: \
     %{rdynamic:-export-dynamic} \
     %{!shared:-dynamic-linker " DYNAMIC_LINKER "} \
     %{!riscos-abi:-riscos-abi " RISCOS_ABI "}} \
   -X \
   %{mbig-endian:-EB}" \
   SUBTARGET_EXTRA_LINK_SPEC "}"

/* -mmodule: after the link the output file is a module ELF file; modreloc (modkit: <prefix>/arm-riscos-gnueabihf/bin/modreloc, next to as and ld, in the cross compiler and in the native one)
   replaces it by the flat module image, as the linker of GCCSDK 4.7.4 wrote one: gcc -mmodule -o Module,ffa main.o header.o makes the module.  An output named *.elf is left alone (for a
   debugger or a simulation), and so is a partial link (-r).  */
#undef  POST_LINK_SPEC
#define POST_LINK_SPEC "%{mmodule:%{!r:%{!shared:modreloc -q --driver %{o*:%*;:a.out}}}}"

#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
      builtin_define ("__riscos");		\
      builtin_define ("__riscos__");		\
      if (TARGET_MODULE)				\
	{						\
	  builtin_define ("__TARGET_MODULE__");	\
	  builtin_define ("__TARGET_SCL__");		\
	}						\
      else						\
	builtin_define ("__TARGET_UNIXLIB__");	\
      /* The GNU C++ standard library requires this.  */	\
      if (c_dialect_cxx ())					\
	builtin_define ("_GNU_SOURCE");				\
    }						\
  while (0)

/* Use --as-needed -lgcc_s for eh support.  */
#ifdef HAVE_LD_AS_NEEDED
#define USE_LD_AS_NEEDED 1
#endif

#define TARGET_RISCOSELF

/* Override configure checks for mmap() compatibility.  Our C library
   partly provides these features, but they do not work in a way
   that the garbage collector expects.  */
#undef HAVE_MMAP_ANON
#undef HAVE_MMAP_DEV_ZERO

/* Clear the instruction cache from `beg' to `end'.  This is
   implemented in lib1funcs.S, so ensure an error if this definition
   is used.  */
#undef  CLEAR_INSN_CACHE
#define CLEAR_INSN_CACHE(BEG, END) not_used

/* These symbol names are inspired by the vxworks target as they
   serve a similar purpose.  */
#define RISCOS_GOTT_BASE "__GOTT_BASE__"
#define RISCOS_GOTT_INDEX "__GOTT_INDEX__"

#ifndef CROSS_DIRECTORY_STRUCTURE
/* This section defines all the specific features for GCC when running
   natively on RISC OS.  */

extern void riscos_host_initialisation (void);
extern const char *riscos_convert_filename (void *obstack,
  const char *name, int do_exe, int do_obj);

#define GCC_DRIVER_HOST_INITIALIZATION \
  riscos_host_initialisation ()
#define TARGET_CONVERT_FILENAME(a,b,c,d) \
  return riscos_convert_filename (a,b,c,d)

/* Character constant used in separating components in paths.  */
#undef PATH_SEPARATOR
#define PATH_SEPARATOR ','

/* Directory name separator.  */
#undef DIR_SEPARATOR
#define DIR_SEPARATOR '/'

#endif /* ! CROSS_DIRECTORY_STRUCTURE */
