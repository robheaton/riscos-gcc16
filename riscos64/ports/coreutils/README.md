# GNU coreutils 9.5 for RISC OS 64

Packaged like the 32-bit `coreutils_8.30-2_arm.zip`: the application `!CoreUtils` holds one program for each utility (`ls,ff8`, `sort,ff8` ...), and its `!Boot` sets `CoreUtils$Dir` and one `Alias$NAME` for each
program (`Set Alias$sort WimpSlot -min 600k|MRun <CoreUtils$Dir>.sort %%*0`), so the utilities work by name on the command line, in Obey files, and for programs that use `system ()`, `popen ()` or `posix_spawn`.
`install` is `ginstall`, as in the 32-bit package. The shell of this port (`ports/sh`) puts `/<CoreUtils$Dir>` in its default `PATH`, and execvp / posix_spawnp look there too.

`package-coreutils.sh` makes the application and the zip from a build; `build-gnu.sh` builds everything. The patch (`ports/patches/coreutils-9.5-riscos64.patch`) fills three places in gnulib that do not know picolibc
(freadahead, freadptr, freadseek) and a missing declaration in `pinky.c`.

Tested on RISC OS Pyromaniac (18 October 2026, job `port_cu2`): uname, date, sort (-u, -k), tr, uniq -c, wc, tee, seq, cut, head, tail, nl, mkdir -p, touch, cp, mv, ls (-l), stat, basename, dirname, rm -r, md5sum,
sha256sum and cksum (the same sums as the host), expr, env and printenv (the environment is passed on), fold, whoami, id. The zlib `./configure` runs to its end with these, the shell and GNU sed, grep and gawk.

Not possible here: `ln -s` (no links), `chown` / `chgrp` (one user), `df` / `who` / `users` / `uptime` (no list of file systems or logins), and a pipeline whose first stage never ends (`yes | head`): the commands of a pipeline run one after the other.
