# Contributing

This is an experimental port made by one person on one machine, so **reports of what happens on other machines are the most useful contribution.**

## Reporting a problem

Open an [issue](https://github.com/robheaton/riscos-gcc16/issues) with:

* what you ran (the exact commands), what you expected and what happened;
* your machine and RISC OS version;
* for the native compiler: `Echo <GCC16$Version>` and the output of `fixlevel` ([tests/fixlevel](tests/fixlevel)); for the cross compiler: `arm-riscos-gnueabihf-gcc -v`;
* for a compiler crash: the source file that triggers it, and the options;
* for a module (`gcc -mmodule`, `cmunge`): the release of the tool chain (the kit is part of it), the CMHG file, the Makefile or the exact `cmunge`/`gcc` command lines, and, if the module is a port from GCCSDK 4.7.4, how it was built there ([docs/PORTING-FROM-4.7.4.md](docs/PORTING-FROM-4.7.4.md)).

Please do not report problems with this port to the GCCSDK mailing list: GCCSDK does not maintain it.

## Sending a change

* Changes to GCC, libstdc++, binutils, make or UnixLib are **patches** in `recipe/*/patches*`. Add a patch (`patch -p1` from the top of the upstream tree) rather than editing a copy, and keep each patch to one change.
* A change to UnixLib needs a check in `tools/check-libunixlib.sh` and, if it changes behaviour, a fix level (see [docs/BUILDING.md](docs/BUILDING.md)).
* A change to the module kit (`modkit/`: the C library in `modkit/lib`, `cmunge`, `modreloc`, `mkoslib`, the headers) needs a test in `modkit/tests`: the library test against glibc (`modkit/tests/libtest`) for a library function, `modkit/tests/test-ctools.py` for the tools, a `modkit/tests/sim-*.py` simulation for module code. [docs/TESTING.md](docs/TESTING.md) says which test covers what and how to run them. A function that you add should also be added to the header that declares it and, if it is part of POSIX or C, behave as glibc does (the library test compares the two).
* Say how you tested it. Anything that can be tested on the Linux host should come with a host test; anything about RISC OS behaviour should say which machine and RISC OS version it was tried on.
* Documentation fixes are very welcome, especially for steps that did not work for you as written.

## Licence

By contributing you agree that your change is released under the licence of the file it changes (see [LICENSES.md](LICENSES.md)).
