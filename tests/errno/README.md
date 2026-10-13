# errtest: the error numbers that 16.2.0-22 added

`errtest.cc` checks, on RISC OS, the seven POSIX error numbers that UnixLib lacked and that the runtime `SharedLibs-C-armeabihf` 16.2.0-14 (fix level 16) has: `EPROTO` 92, `ENODATA` 93, `ENOLINK` 94, `EMULTIHOP` 95, `ECANCELED` 96, `ENOTRECOVERABLE` 97 and `EOWNERDEAD` 98.

* **UnixLib:** the numbers, the `strerror` text of each, `sys_nerr` (99) and the answer for an unknown number (`Unknown Error`, `errno` 22).
* **libstdc++:** `std::system_category ().default_error_condition ()` puts six of the seven in the generic category since the C++ runtime 16.2.0-6 (an older runtime does not know any of them). `EMULTIHOP` stays in the system category on purpose: C++'s `std::errc` has no multihop error, so libstdc++ does not list it. `message ()` gives the `strerror` text for all seven.

Build it with the cross compiler (it links the C++ and C runtimes dynamically, so the runtimes on the machine are the ones tested) and run it:

```bash
arm-riscos-gnueabihf-g++ -O2 -Wall -o errtest,e1f errtest.cc
```

It ends with `errtest: ALL CHECKS PASSED` (exit status 0), or with `SOME CHECKS FAILED`; a `libstdc++: FAIL` with all seven in the `system` category means that `SharedLibs-C++-armeabihf` is older than 16.2.0-6, and a `UnixLib: FAIL` that the runtime is older than 16.2.0-14. The first run on the Raspberry Pi (13 October 2026) passed the UnixLib part and showed six codes in the generic category and `EMULTIHOP` in the system category; the first version of the test wrongly expected `EMULTIHOP` in the generic category, and this is the corrected one, which passed on the same machine (`errtest: ALL CHECKS PASSED`).
