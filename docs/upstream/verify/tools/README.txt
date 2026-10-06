verify/tools - the ARM (A32) interpreter and the scripts that run the machine code of a BUILT module / libunixlib on it (written for these reports; Python 3, no other dependency but an `nm` that reads ARM ELF
object files: set NM=/path/to/nm if the default one cannot).  They run the code itself, not a model of it: the module's .o gives the symbols, the raw .bin gives the code.
  sim-startup-loops.py   the interpreter (ARM state, the instructions these routines use, SWIs reported to a hook, memory with an overlay)
  sim-sul-exit.py        sul_exit: when is ARMEABISupport_StackOp FREE called, with which handle        sim-sul-exit.py MODULE.bin MODULE.o fixed|orig
  sim-sul-slot.py        restore_wimpslot: which SWIs for a given PROC_INITIALAPPSPACE                  sim-sul-slot.py MODULE.bin MODULE.o fixed2|old
  sim-sul-fork.py        sul_fork: the child's status word for 48 parent statuses                       sim-sul-fork.py OLD.bin OLD.o NEW.bin NEW.o
  sim-sul-trace.py, sim-sul-trace-diff.py   what sim-sul-fork.py is built on (and the checks of the traced build I used for the investigation)
  sim-exit-hooks.py      libunixlib's __pthread_prog_fini and the other exit hooks (report 02)
  mutate-himem-max.py    breaks each of the four start-up instructions that set __ul_memory.appspace_himem_max (report 08) in a copy of a BUILT libunixlib.so and runs sim-startup-loops.py on it:
                         every breakage must be caught.   (sim-startup-loops.py itself also runs those instructions for 12 combinations of limit and application space limit.)
  mutate-stack-da.py     breaks every instruction of the two start-up loops of sys/_syslib.s (stack_try, report 17; da_try, report 18) in a copy of a BUILT libunixlib.so (nop,
                         opposite condition, changed immediate; 76 breakages) and runs sim-startup-loops.py on each: every breakage must be caught.  (sim-startup-loops.py itself runs
                         25 stack and 13 + 3 heap-area scenarios; it reads the library's own symbols stack_try, stack_ok, da_try, da_last, da_done, ___program_name.)
  check-vstm-split.py    report 16: the patched _memcpymove-v7l.o differs from the original only by the split of each 64-byte vstm (needs objdump of the target)
  check-fidelity.sh      the 21 UnixLib patches of the bundle, applied in sequence to pristine trunk, equal (comments stripped) the sources of the release build of my port
  a32.py                 the A32 interpreter as a module (ELF static image, writable data overlay, SWI hook) that the scripts below import
  sim-regvar.py          report 19: runs repro/regvar/regvar1.c's f () on the interpreter, compiled by each compiler given, with the wrapper as in os.h and with -DFIXED
                         sim-regvar.py REGVAR1.C CC [CC ...]
  check-regvar-asm.sh    report 19: is the result of the asm in regvar1.c copied out of r0 before the first call?  check-regvar-asm.sh CC REGVAR1.C [-DFIXED]  (also for GCC 4.7.4)
  sim-swi-wrappers.py    report 19: every wrapper of the pristine and of the rewritten os.h compiled into a test function and run on the interpreter with a model of the kernel
                         that scrambles the registers the wrapper declares changed (32 wrappers x 300 seeds, 10 mutants); needs swi-wrappers/gen.py;
                         environment A32_CC (the GCC 16 compiler), A32_LIBROOT (a libunixlib source tree), A32_CONFIG_INC (the directory with config.h)
  sim-get-dde-prefix.py  report 20: the compiled __get_dde_prefix () of the pristine and of the patched common/prefix.c on the interpreter (11 cases, 5 mutants)
  sim-mcount.py          report 25: the compiled __gnu_mcount_nc (the MCOUNT macro of the patched incl-local/internal/machine-gmon.h) as a profiled function calls it (prologue, push {lr},
                         bl __gnu_mcount_nc, body, epilogue) on the interpreter: 27 cases of r0-r3 / ip / fp, 8 mutants (among them the legacy ip and frame-pointer conventions)
                         sim-mcount.py TREE PATCHED_INCL CONFIG_INC CC     (TREE = the libunixlib source tree, PATCHED_INCL = src/unixlib-gprof-eabi/incl-local)
How the modules were built: see verify/VERIFY-LOG.txt (the unpatched sul.s assembled with the GCCSDK 10.2.0 tool chain: xgcc -xassembler-with-cpp -D__UNIXLIB_CHUNKED_STACK=0 -g -O2 -c, then strip -O binary).
