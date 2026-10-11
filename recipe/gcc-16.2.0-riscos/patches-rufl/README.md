# Patch for building RUfl for programs

`rufl-log-off.patch` (one hunk in `src/rufl_internal.h`) is applied by `scripts/install-rufl.py` to your checkout of `git://git.netsurf-browser.org/librufl.git` (RUfl, under the MIT licence; the patch is too). RUfl's source has its debug log hard-wired on (`#if 1 /*ndef NDEBUG*/`): every message goes to stderr, which a Wimp program shows on the screen. The patch compiles the log only when `RUFL_LOG` is defined. See [docs/RUFL.md](../../../docs/RUFL.md).
