repro/regvar - report 19: the inline SWI wrappers of UnixLib's os.h read
register variables after the asm; GCC 16 then cuts the DDEUtils command line

regvar1.c               The reproducer: the shape of the wrapper
                        SWI_DDEUtils_GetCLSize () and of its use in __unixinit
                        (), with the SWI replaced by "mov r0, #7".  f ("hello")
                        must return 12.
                          gcc -O2 -S -o - regvar1.c
                            is the result of the asm copied out of r0 before
                            "bl strlen"?  (GCC 4.7.4 and 10.2.0: yes; GCC
                            16.2.0: no)
                          gcc -O2 -o regvar1 regvar1.c ; ./regvar1
                            prints the result (ARM targets only)
                          -DFIXED   the rewritten wrapper of the patch
                          -DNO_MAIN no main () (for verify/tools/sim-regvar.py)
                        verify/tools/check-regvar-asm.sh makes the assembler
                        check, verify/tools/sim-regvar.py runs f () on an A32
                        interpreter.
fix-unixlib-regvars.py  IN OUT: rewrites the inline SWI wrappers of a pristine
                        incl-local/internal/os.h; the output (with the comment
                        block that the patch adds) is the patched file.
argtest.c               The programs of the machine runs (RISC OS, arm-riscos-
rawarg.c                gnueabihf-gcc):
ddecl.c                   argtest.c  gcc -O2 argtest.c -o argtest,e1f
                            prints argc, every argument, the OS_GetEnv string
                            and the DDEUtils command line buffer; the test
                            words are w01, w02, ...: "ALL ARRIVED IN ORDER"
                          rawarg.c   no UnixLib, no C library (static ELF):
                            gcc -O2 -ffreestanding -nostdlib -nostartfiles
                            -static -Wl,-e,_start -o rawarg,e1f rawarg.c
                            prints what the loader (SOMRun) hands over: the
                            OS_GetEnv string and the DDEUtils command line
                          ddecl.c    a round trip of DDEUtils_SetCLSize /
                            SetCL / GetCl for sizes 2 - 2000
                        To see the problem on a machine: load the DDEUtils
                        module (any editor that does throwback loads it, or
                        RMLoad it), then *argtest w01 w02 ... w60 (or more
                        words): with a UnixLib built by GCC 16 and without the
                        patch only as many characters arrive as the program
                        name is long (the name as OS_GetEnv gives it).
