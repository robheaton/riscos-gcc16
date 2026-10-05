#!/usr/bin/env python3
"""Make the patches of the SharedUnixLibrary report (module/sul.s of the GCCSDK UnixLib) against PRISTINE upstream (svn trunk r7800, the file ~/gccsdk/gcc4/recipe/files/gcc/libunixlib/module/sul.s of a clean working copy):
   sul-1-vfork-child-main-stack.patch      sul_fork clears PROC_STACK in the child; sul_exit frees the main stack only when PROC_STACK is not 0
   sul-2-vfork-child-wimp-slot.patch       restore_wimpslot does nothing when PROC_INITIALAPPSPACE is 0
   sul-3-vfork-child-execed-flag.patch     sul_fork clears IS_EXECED in the child's status word
   sul-all-vfork-child.patch               all three in one patch (the one to take if you take more than one: 1 and 3 insert after the same line)
The patches contain code and comments only (no ChangeLog hunk: a suggested entry is in the report).  usage: make-upstream-sul-patches.py OUTDIR      (also writes the patched files to OUTDIR/src/<variant>/sul.s)"""
import os, subprocess, sys
PRISTINE = os.path.expanduser("~/gccsdk/gcc4/recipe/files/gcc/libunixlib/module/sul.s")
REL = "gcc4/recipe/files/gcc/libunixlib/module/sul.s"

STACK_FORK = '''	@ The child was copied from the parent, PROC_STACK (the ARMEABISupport handle of the main stack) included.  A vfork child runs on the stack of its
	@ PARENT until it execs, so that stack is not the child's: if the child's PROC_STACK stayed, sul_exit would free the stack of the parent when the child
	@ ends without an exec, and the parent would be resumed on it.  A child that execs a UnixLib program records the handle of its own stack in its
	@ start-up code.
	STR	a1, [v2, #PROC_STACK]
'''
STACK_EXIT_OLD = '''	MOV	a1, #ARMEABISUPPORT_STACKOP_FREE
	LDR	a2, [v2, #PROC_STACK]
	SWI	XARMEABISupport_StackOp
'''
STACK_EXIT_NEW = '''	@ Free the main stack, if the process has one of its own recorded: a process that ends before its start-up code has recorded the handle, a vfork child
	@ that did not exec, and a child that exec'd a program that is not UnixLib have PROC_STACK = 0 (sul_fork clears it in a child).  StackOp FREE of 0
	@ must not be called: ARMEABISupport does not check the handle and follows it as a pointer.
	LDR	a2, [v2, #PROC_STACK]
	TEQ	a2, #0
	MOVNE	a1, #ARMEABISUPPORT_STACKOP_FREE
	SWINE	XARMEABISupport_StackOp
'''
SLOT_ANCHOR = '''restore_wimpslot:
	LDR	a1, [v2, #PROC_INITIALAPPSPACE]
'''
SLOT_NEW = SLOT_ANCHOR + '''	@ Nothing was recorded, so there is nothing to restore: this is the exit of a vfork child that did not exec (sul_fork clears the INITIAL* fields of a child, and only
	@ copy_up_parent, for a child that execs, and SharedUnixLibrary_Initialise record them).  With the 0 the code below asks Wimp_SlotSize for 0 - 0x8000 = 0xFFFF8000
	@ bytes, which the Wimp takes for a huge request and answers by growing the slot of the PARENT, the task that child shares, to its maximum (96MB to 512MB, measured on a
	@ Cortex-A72 / RISC OS 5.30; the application space and memory limits follow).
	TEQ	a1, #0
	MOVEQ	pc, lr
'''
EXECED = '''	@ The status word was copied from the parent too, and with it the flag IS_EXECED: execve sets it in the process structure of a process that execs, and the program
	@ that is started keeps that structure, so a program that was started by exec has it set for its whole life.  A vfork child of such a program is not the result of an exec:
	@ until it execs itself it is the same client of the Shared Object Manager as its parent.  With the flag set, sul_exit takes the child for a new client and calls
	@ SOM_DeregisterClient for the client that it shares with the parent: that frees the parent's client record and the tables behind its PIC register (the pointer at
	@ 0x8038 in the application space), which the parent goes on using.  The next client that registers - typically the child that the parent execs next - is handed the freed
	@ memory, and the parent's next call into UnixLib loads garbage as its PIC register: a crash, or a frozen machine when that happens inside __env_unixlib (between OS_IntOff
	@ and OS_IntOn).  A child that execs sets the flag itself (execve) before it calls sul_exec.
	LDR	a1, [v2, #PROC_STATUS]
	BIC	a1, a1, #SULPROC_STATUS_FLAG_IS_EXECED
	STR	a1, [v2, #PROC_STATUS]
'''
PARENTADDR = '''	STR	a1, [v2, #PROC_PARENTADDR]

	@ Save a reference to the parent proc struct
'''

def rep(t, old, new, count=1):
    assert t.count(old) == count, (old[:60], t.count(old))
    return t.replace(old, new)

def stack(t):
    t = rep(t, PARENTADDR, '''	STR	a1, [v2, #PROC_PARENTADDR]

''' + STACK_FORK + '''
	@ Save a reference to the parent proc struct
''')
    return rep(t, STACK_EXIT_OLD, STACK_EXIT_NEW)
def slot(t): return rep(t, SLOT_ANCHOR, SLOT_NEW)
def execed_alone(t): return rep(t, PARENTADDR, '''	STR	a1, [v2, #PROC_PARENTADDR]

''' + EXECED + '''
	@ Save a reference to the parent proc struct
''')
def execed_after_stack(t): return rep(t, STACK_FORK + '''
	@ Save a reference''', STACK_FORK + '\n' + EXECED + '''
	@ Save a reference''')

def main():
    out = sys.argv[1]; os.makedirs(os.path.join(out, "src"), exist_ok=True)
    pristine = open(PRISTINE).read()
    variants = {"pristine": pristine, "stack": stack(pristine), "slot": slot(pristine), "execed": execed_alone(pristine),
                "all": execed_after_stack(slot(stack(pristine)))}
    names = {"stack": "sul-1-vfork-child-main-stack.patch", "slot": "sul-2-vfork-child-wimp-slot.patch", "execed": "sul-3-vfork-child-execed-flag.patch", "all": "sul-all-vfork-child.patch"}
    for k, v in variants.items():
        os.makedirs(os.path.join(out, "src", k), exist_ok=True); open(os.path.join(out, "src", k, "sul.s"), "w").write(v)
    for k, n in names.items():
        d = subprocess.run(["diff", "-u", "--label", "a/" + REL, "--label", "b/" + REL, os.path.join(out, "src", "pristine", "sul.s"), os.path.join(out, "src", k, "sul.s")], capture_output=True, text=True)
        assert d.returncode == 1, d.stderr
        open(os.path.join(out, "patches", n), "w").write(d.stdout)
        print("%-40s %3d lines" % (n, d.stdout.count("\n")))
main()
