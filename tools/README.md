# Tools

Host tools used to check the changes without a RISC OS machine, and to regenerate the upstream patches.

| Tool | What it does |
|---|---|
| `a32.py` | a small interpreter for ARM-state (A32) code, in Python: it loads an ELF or a flat image, runs functions with a model of the kernel and the SWIs they call, and reports registers and memory. The checks below use it to run the **compiled machine code** of the changed UnixLib functions |
| `check-docs.py [repo]` | checks every relative link and `#anchor` in the Markdown documents (run it after editing the docs) |
| `check-libunixlib.sh <libunixlib.so>` | looks for the code of every fix in a built UnixLib (and runs the start-up and exit logic on the interpreter), so that a patch that silently failed to apply cannot be packaged; used by `make-c16-package.py` |
| `sim-startup-loops.py`, `sim-exit-hooks.py` (+ `sim-exit-hooks.equivalent`) | the interpreter checks behind `check-libunixlib.sh`: the stack and heap start-up loops (116 scenarios) and the exit hooks (35 scenarios, 75 mutants of the code; the two that survive are reviewed as equivalent in `sim-exit-hooks.equivalent`). The check of the rewritten SWI wrappers is in [`docs/upstream/verify/tools`](../docs/upstream/verify/tools) |
| `make-upstream-unixlib-patches.py`, `make-upstream-sul-patches.py` | regenerate the patches in `docs/upstream/patches` against the pristine GCCSDK sources (needs a clean svn checkout of trunk r7800 in `~/gccsdk`) |
| `scan-os-modules.py` | what the C modules of the RISC OS Open sources need from a C library and from CMHG, against what modkit has (`--missing` lists the functions that are missing, with the number of modules that use each; `--detail` one line per module); the figures of [docs/MODULES.md](../docs/MODULES.md#what-can-be-built-today-and-what-cannot) come from it |
