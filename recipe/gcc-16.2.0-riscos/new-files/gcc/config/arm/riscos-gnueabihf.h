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

#undef  LIB_SPEC
#define LIB_SPEC \
  "%{!nostdlib:-lunixlib }"
  
#define DYNAMIC_LINKER "ld-riscos/so/2"
#define RISCOS_ABI "armeabihf"

#ifdef CROSS_DIRECTORY_STRUCTURE
#define SUBTARGET_EXTRA_LINK_SPEC " -m armelf_riscos_eabi -p \
     %{!static: \
      %{!fpic:-fPIC} %{fpic:-fpic} \
      %{mmodule:--ro-module-reloc --target2=rel}}"
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
     %:riscos_multilib_dir() \
     %{mmodule:--ro-module-reloc --target2=rel}}"

#endif

/* Same as arm.h's DRIVER_SELF_SPECS (including the entries added since GCC 10,
   ARCH_CPU_CLEANUP_SPECS and MULTILIB_ARCH_CANONICAL_SPECS), plus the RISC OS
   defaults: NEON implies hard-float on a Cortex-A8, and no unaligned access.  */
#undef DRIVER_SELF_SPECS
#define DRIVER_SELF_SPECS						   \
  ARCH_CPU_CLEANUP_SPECS,						   \
  " %{mfpu=neon:%{!mfloat-abi=*:-mfloat-abi=hard} %{!mcpu=*:-mcpu=cortex-a8}}"   \
  " %{!munaligned-access:-mno-unaligned-access}" \
  MCPU_MTUNE_NATIVE_SPECS,			\
  TARGET_MODE_SPECS,				\
  MULTILIB_ARCH_CANONICAL_SPECS,		\
  ARCH_CANONICAL_SPECS


#undef STARTFILE_SPEC
#define STARTFILE_SPEC	" crti.o%s" \
			" %{!shared:crt0.o%s}" \
			" %{shared:crtbeginS.o%s;:crtbegin.o%s}"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC	" %{shared:crtendS.o%s;:crtend.o%s}" \
			" crtn.o%s"

#undef  LINK_SPEC
#define LINK_SPEC "%{h*} %{version:-v} \
   %{b} %{Wl,*:%*} \
   %{static:-Bstatic} \
   %{shared:-shared} \
   %{symbolic:-Bsymbolic} \
   %{!static: \
     %{rdynamic:-export-dynamic} \
     %{!shared:-dynamic-linker " DYNAMIC_LINKER "} \
     %{!riscos-abi:-riscos-abi " RISCOS_ABI "}} \
   -X \
   %{mbig-endian:-EB}" \
   SUBTARGET_EXTRA_LINK_SPEC

#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
      builtin_define ("__riscos");		\
      builtin_define ("__riscos__");		\
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
