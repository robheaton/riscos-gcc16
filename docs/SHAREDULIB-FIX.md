# The fixed SharedUnixLibrary (`SharedULibFix`, optional)

> **Optional, and it replaces a system module.** `SharedULibFix_1.16-vforkfix3_arm.zip` puts a fixed copy of the RISC OS module **SharedUnixLibrary 1.16** in place of the one that RISC OS ships. You do not need it to use the compilers: ordinary compiles and `make` runs work with the stock module. Read this page first, save your work before you install it, and keep the way back in mind (`Restore`, below). It was tested on one machine (a Raspberry Pi Compute Module 4, RISC OS 5.30).

## What is wrong with the stock module

SharedUnixLibrary 1.16 (3 Apr 2020) is the module that every UnixLib program shares. It has three bugs that are met when a program starts a child with `vfork ()` and the child ends **without** calling `exec`. That is the usual reaction to an `exec` that failed (`if (vfork () == 0) { execv (...); _exit (127); }`, as a shell, GNU make or any program that runs commands does):

1. the child frees the main stack of its **parent**, and the parent dies (`Internal error: abort on data transfer`);
2. the child makes its parent's Wimp slot as big as it can be (96 MB became 512 MB in a test);
3. when the parent was itself started by `exec` (everything under `make` or a shell), the child deregisters the parent's Shared Object Manager client, which **froze the machine** in a loop test.

Ordinary compiles and `make` runs do not meet them (the regression suites, including chains of `make` -> `gcc` -> `cc1` -> `as`, pass with the stock module). A program that forks children that fail to `exec` can.

The reports are drafts 01 and 08 in [`docs/upstream/`](upstream/00-INDEX.txt), with the patches, reproducers and simulations.

## What the package is

`SharedULibFix_1.16-vforkfix3_arm.zip` (about 24 KB) is a PackMan package that installs the folder `!SULFix` in `Apps.Utilities`. **Installing the package changes nothing in the system**: the module is replaced only when you run `Install` in a Task window. Removing the package removes only that folder; the module that is installed stays (run `Restore` first if you want the stock module back).

| In `!SULFix` | What it is |
|---|---|
| `SharedULib` | the fixed module: SharedUnixLibrary **1.16-vforkfix3** (4 Oct 2026), 3228 bytes, file type &FFA |
| `Install`, `Restore`, `Check` | Obey files (read them: they are short) |
| `sulfile` | says which SharedUnixLibrary a file is (by size and a hash of the whole file) and where the file that `System:Modules.SharedULib` really is |
| `modver` | the title and version of the module that is **loaded** |
| `fixlevel` | the fix level of the runtime |
| `vforkbare`, `vforkfail` | the tests of `Check` |

It needs the runtime `SharedLibs-C-armeabihf` 16.2.0-13 (PackMan installs it first). **The runtime matters:** the fixed module lets a `vfork` child that ends without `exec` run to the end, and with a runtime older than UnixLib fix level 10 (16.2.0-8) that child frees memory that its parent still uses, which can corrupt the RMA and freeze the machine. `Install` and `Check` refuse an older runtime. Programs that carry their own copy of UnixLib (linked with `-static`, or built by an older tool chain) do not use the runtime package: they have the old behaviour whatever module is installed.

## Install it

1. Install `SharedULibFix_1.16-vforkfix3_arm.zip` with PackMan and open the folder that contains `!SULFix` (`Apps.Utilities`) once.
2. Open a Task window (Ctrl-F12) and type: `Obey <SULFix$Dir>.Install` (about 10 seconds).
3. **Reboot.** A module that is loaded stays as it is until the next boot, so the stock module is still the one that runs.
4. As the **first** thing after the boot, open the folder that contains `!SULFix` once (the Filer sets `<SULFix$Dir>` again at every boot, and that starts no UnixLib program; or type the full name that `Install` printed at its end), then in a Task window: `Obey <SULFix$Dir>.Check`. It checks that the installed file is the fixed module, that the module that is **loaded** is the fixed one, and that a `vfork` child that ends without `exec` leaves its parent alone (with the stock module the parent died).

`Install` stops, and changes nothing, at the first thing that is not as expected:

* it finds the file that `System:Modules.SharedULib` is (`System$Path` can have it in any of several folders) and compares the **whole file** with the stock 1.16 (size and hash): the fixed module already installed, another version of SharedUnixLibrary, any other file: it stops;
* it checks the runtime (fix level 10 or later);
* it backs the stock module up **twice**, `SharedULib-stock` in `!SULFix` and `SharedULib-stock` next to the module, and checks both copies;
* it copies the fixed module next to the installed one (`SharedULib-new`) and checks that copy there, so a disc or network error shows up before anything is replaced;
* only then does it replace the module and check the result, **and that `System:Modules.SharedULib` now finds the fixed module**. If a check fails it puts the stock module back in the file that it replaced.

## Go back

`Obey <SULFix$Dir>.Restore`, then reboot. It uses the backup next to the module, or the one in `!SULFix`, after checking it; if neither is the stock module it stops. If the folder is gone: copy `SharedULib-stock`, which is next to the module, over `SharedULib`, and reboot.

A PackMan update of a package that ships the stock module (a RISC OS update, say) puts the stock module back: run `Install` again then.

## Where it comes from

* The module is built from `module/sul.s` of UnixLib (GCCSDK svn trunk r7800; the release attaches an unmodified snapshot, `gccsdk-unixlib-r7800.tar.xz`) with three patches, [`patches-unixlib/unixlib-sul-vfork-child-stack.patch`, `-slot.patch` and `-execed.patch`](../recipe/gcc-16.2.0-riscos/patches-unixlib), one for each bug above (the same fixes are in `docs/upstream/patches/`, with the reports). [`build-sul.sh`](../recipe/gcc-16.2.0-riscos/scripts/build-sul.sh) builds it with GCCSDK 10.2.0's tool chain, because `sul.s` uses FPA instructions that binutils 2.45.1 no longer assembles. The unpatched source built the same way is **byte for byte GCCSDK's own module** (the script checks that, so the tool chain is known to reproduce the stock file).
* The programs and the Obey files are in [`recipe/gcc-16.2.0-riscos/sulfix`](../recipe/gcc-16.2.0-riscos/sulfix); the package is made by `make-sul-package.py` ([BUILDING.md](BUILDING.md), step 6).
* Licences: SharedUnixLibrary is part of UnixLib (the revised BSD licence, some files LGPL; the patches keep the licence of the file they change); the programs and the Obey files are GPL-3.0-or-later ([LICENSES.md](../LICENSES.md)). No warranty: it replaces a system module.

## How it was tested

* The module is the one that ran on the author's machine since 4 Oct 2026: the eight stages of the `vfork` loops that froze the machine every time with the stock module (children that end without `exec`, children whose `exec` fails, six processes in a row, every way a process can end) ran without a freeze, and the whole regression suite passes with it. It stayed installed for the later releases' tests.
* The installer logic: [`tools/sim-sulfix.py`](../tools/sim-sulfix.py) runs the three Obey files on a model of the RISC OS command line and of the file layout (28 scenarios: the normal install; the module already fixed; a file it does not know; no module; a runtime that is too old; the module in another folder of `System$Path`, found first or last; a copy that is damaged on its way, in each of the four places; `System:` finding another file after the copy; `Restore` with each backup missing or damaged; `Check` with the stock module still loaded). `sulfile` in it is the real source, built for the host against a model of the RISC OS calls it makes ([`host-main.c`](../recipe/gcc-16.2.0-riscos/sulfix/host-main.c)); the pure parts of it have their own host test ([`host-test.c`](../recipe/gcc-16.2.0-riscos/sulfix/host-test.c), 41 checks). What the model had to assume about RISC OS itself is marked ASSUMED in those files.
* The package was installed with PackMan on the author's machine and its scripts were run for real on 6 Oct 2026: `Install` on the stock module (it found the file `Sys:310.Modules.SharedULib`, backed it up twice, copied the fixed module next to it, checked the copy, replaced the module, and `System:Modules.SharedULib` then found the fixed one), a reboot, `Check` (the installed file, the **loaded** module and the three `vfork` tests all passed), `Restore` (the stock module back, checked by size and hash), `Install` again, another reboot, `Check` again, and a last `Install`, which stopped with "already installed" and changed nothing. The package of the release is the one that was tested, apart from the wording of its ReadMe.
