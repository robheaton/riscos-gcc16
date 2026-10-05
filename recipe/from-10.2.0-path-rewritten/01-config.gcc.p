--- gcc/config.gcc.orig	2020-07-23 07:35:17.316384243 +0100
+++ gcc/config.gcc	2020-07-24 21:11:45.319035882 +0100
@@ -776,7 +776,7 @@
 *-*-fuchsia*)
   native_system_header_dir=/include
   ;;
-*-*-linux* | frv-*-*linux* | *-*-kfreebsd*-gnu | *-*-gnu* | *-*-kopensolaris*-gnu | *-*-uclinuxfdpiceabi)
+*-*-linux* | frv-*-*linux* | *-*-kfreebsd*-gnu | *-*-kopensolaris*-gnu | *-*-uclinuxfdpiceabi)
   extra_options="$extra_options gnu-user.opt"
   gas=yes
   gnu_ld=yes
@@ -1332,6 +1332,32 @@
 	esac
 	tm_file="${tm_file} arm/aout.h arm/arm.h"
 	;;
+arm*-riscos-gnueabihf)
+	gas=yes
+	gnu_ld=yes
+	tm_file="dbxelf.h elfos.h glibc-stdint.h arm/elf.h arm/aout.h arm/arm.h arm/riscos-gnueabihf.h"
+	# FIXME: when --enable-shared option is given, we shouldn't have t-slibgcc
+	# specified.  It's only there to let SHLIB and ENABLE_SHARED_LIBGCC
+	# being defined so REAL_LIBGCC_SPEC is correctly defined for the shared vs
+	# static cases.
+	tmake_file="arm/t-arm arm/t-riscos-gnueabihf t-slibgcc"
+	xm_file="arm/xm-riscos.h"
+	extra_options="${extra_options} arm/riscos.opt"
+	extra_objs="${extra_objs} riscos.o"
+	extra_gcc_objs="riscos-gcc.o"
+	;;
+arm*-*-riscos)
+	tm_file="dbxelf.h elfos.h arm/elf.h arm/aout.h arm/riscos-elf.h arm/arm.h"
+	# FIXME: when --enable-shared option is given, we shouldn't have t-slibgcc
+	# specified.  It's only there to let SHLIB and ENABLE_SHARED_LIBGCC
+	# being defined so REAL_LIBGCC_SPEC is correctly defined for the shared vs
+	# static cases.
+	tmake_file="arm/t-arm arm/t-arm-riscos-elf t-slibgcc"
+	xm_file="arm/xm-riscos.h"
+	extra_options="${extra_options} arm/riscos.opt"
+	extra_objs="${extra_objs} riscos.o"
+	extra_gcc_objs="riscos-gcc.o"
+	;;
 avr-*-*)
 	tm_file="elfos.h avr/elf.h avr/avr-arch.h avr/avr.h avr/specs.h dbxelf.h avr/avr-stdint.h"
 	if test x${with_avrlibc} != xno; then
