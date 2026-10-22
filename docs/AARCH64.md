# The aarch64-riscos tool chain: programs, C++ and modules for RISC OS 64

**Status: experimental (17 October 2026).** Everything here is built from the sources in [`riscos64/target/`](../riscos64/target) and tested on RISC OS Pyromaniac, through the hosted
[RISC OS Build service](https://build.riscos.online/); see [RISCOS64.md](RISCOS64.md) for the results and [UNIXLIB64.md](UNIXLIB64.md) for the library plan.

`aarch64-riscos` is a real target of binutils 2.45.1 and GCC 16.2.0 (`aarch64-riscos-gcc`, `-g++`, `-as`, `-ld` ...). It runs on a PC (Linux) and makes programs and modules for 64-bit RISC OS.

## Use the release (no build needed)

Download `aarch64-riscos-gcc-16.2.0-0.11-linux-x86_64.tar.xz` (or a newer one) from the [releases page](https://github.com/robheaton/riscos-gcc16/releases) (the names start `riscos64-toolchain-`); you need Linux x86-64 with glibc 2.38 or newer.

```
tar -xf aarch64-riscos-gcc-16.2.0-0.11-linux-x86_64.tar.xz
export PATH=$PWD/aarch64-riscos-gcc-16.2.0-0.11-linux-x86_64/bin:$PATH
./aarch64-riscos-gcc-16.2.0-0.11-linux-x86_64/selftest.sh                 # builds three small programs and checks them
aarch64-riscos-gcc -O2 -o hello,ff8 hello.c                              # the SDK C library (picolibc): small programs
aarch64-riscos-gcc -munixlib -O2 -o prog,ff8 prog.c                      # UnixLib 64: Unix file names, vfork/exec, signals, threads, sockets
aarch64-riscos-g++ -munixlib -pthread -O2 -o prog,ff8 prog.cc           # C++20 with the standard library and std::thread
aarch64-riscos-gcc -mmodule -O2 mod.c mod_hdr.o -o Mod,ffa               # a 64-bit module (see MODULES64.md)
```

The programs have file type &FF8 (`,ff8`) and modules &FFA (`,ffa`); copy them to a RISC OS 64 machine or run them on the [RISC OS Pyromaniac](https://github.com/gerph/pyromaniac) emulator. The libraries zlib, expat, PCRE2, libpng and libjpeg-turbo are a separate download, `riscos64-libs-1`: `tar -xf riscos64-libs-1.tar.xz -C PREFIX/aarch64-riscos --strip-components=1` (PREFIX is the directory you unpacked the tool chain into), then link with `-lz`, `-lpng16`, `-ljpeg`, `-lexpat` or `-lpcre2-8`. `bin/riscos64-crashsym` names the addresses of a crash report from a link map (`-Wl,-Map=prog.map`). The shell and the GNU tools are the `!GNU` and `!CoreUtils` applications: [USERLAND64.md](USERLAND64.md). Programs that need `fork` (`bash`, servers) do not work: see its limits table.

## Build it

```
riscos64/tools/build-picolibc.sh                           # picolibc, the C library of programs (needs meson and ninja; installed in /var/tmp/a64/picolibc, or PICOLIBC=)
HOSTLIBS=<prefix of the host GMP, MPFR, MPC> \
riscos64/target/build-target.sh BINUTILS-SRC GCC-SRC PREFIX WORKDIR
```

`build-target.sh` patches copies of the binutils 2.45.1 and GCC 16.2.0 sources, builds binutils, GCC (C and C++), libgcc and libstdc++, compiles the SDK's start-up files and system calls and the module kit with the
compiler it has just built, and installs everything in `PREFIX`: the commands `aarch64-riscos-gcc`, `cmunge64`, `mkoslib`, the C library and the SDK files in `PREFIX/aarch64-riscos`. About 12 minutes on 22 cores.
Put `PREFIX/bin` on `PATH`.

## A program

```
aarch64-riscos-gcc -O2 hello.c -o hello,ff8
```

The result is an **AIF64** absolute (file type `&FF8`: name the file `hello,ff8`, which is how RISC OS file types are written on other systems). It is linked at `&8000` with the AIF64 header, picolibc and the SDK's
system calls (`OS_Write0`, `OS_File`, `OS_Find`, `OS_GBPB`, `OS_Args`, `OS_DynamicArea` ...). `__riscos__` and `__riscos64__` are defined; `x18` is reserved and strict alignment is the default.
The heap is a dynamic area (it grows to 1 GB). `printf` handles `long double`, `long long`, `%n` and positional arguments.

```
aarch64-riscos-g++ -O2 hello.cc -o hello,ff8
```

C++ with libstdc++: containers, strings, iostream, exceptions, static constructors and destructors, `atexit`. No threads, no `std::filesystem` (its calls fail with `ENOSYS`), `std::locale` accepts only "C" (the C library has locales: see [UNIXLIB64.md](UNIXLIB64.md)).

## Unix file names: `-munixlib`

Without options, a program uses RISC OS file names (`$.work.hello`). With **`-munixlib`** (UnixLib 64, [`riscos64/unixlib/`](../riscos64/unixlib)) the file calls take Unix names: `src/main.c` is the file `main` in the
folder `c` in the folder `src`; `/tmp` is `<Wimp$ScrapDir>`; `foo.txt` is the file `foo/txt`; `opendir`/`readdir` list folders (the suffix folders as `main.c`), `chdir`, `getcwd`, `rmdir`, `statvfs`, `argv[0]` is a Unix
path, and new files get a file type (`,xyz` in the name, or Text). The translation is that of UnixLib (`__riscosify`, `__unixify`), including `UnixEnv$...` variables and `UnixFS$/name` special folders. GMP, MPFR and MPC
are built this way ([`riscos64/ports`](../riscos64/ports)) and their tests, which use Unix file names, run.

Always available (with or without the option): the environment (`getenv` sees `setenv` and the system variables), user and group ids, `uname`, `gethostname`, `umask`, `usleep`, `nanosleep`, `getrusage`,
`getrlimit`, anonymous `mmap`, `fcntl`, `ioctl`, `fsync`, `ftruncate`, `truncate`, `chmod`, `utime`, `realpath`, `signal`/`raise`/`sigaction` over a table of handlers, `system`, `popen` (commands of the operating
system), `posix_spawn`, `waitpid`, `glob` (with `-munixlib`, which reads directories), `confstr`, `getentropy` (not secure yet), `pselect`, `<pthread.h>` for a single thread (locks, condition variables, `pthread_once` and keys work; `pthread_create` fails with `EAGAIN`) and `<dlfcn.h>` (`dlopen` always fails: programs are linked statically). Wide characters work in `printf` (`%ls`, `%lc`), `setlocale` accepts UTF-8 and the ISO-8859 and Windows code page locales, and `iconv_open` converts between UTF-8 and ISO-8859-x and CP125x (the UTF-16 and UTF-32 names are not recognised yet). Wide characters on streams (`fputwc`, `fputws`, `fgetwc`, `fgetws`, `ungetwc`) are converted to and from the multibyte encoding of the locale, as POSIX has it.

**Running other programs.** RISC OS loads a program started by `OS_CLI` over the one that started it. With `*Set UnixEnv$swap 1` the C library keeps the parent: it copies its image and stack to a dynamic area, sets an exit handler that lives in that area, and when the child ends the handler copies everything back and the call returns. `system`, `popen` and `posix_spawn` of a program then return the child's status, `vfork` + `exec*` / `_exit` and `wait`/`waitpid` work (a `vfork` child may only call `exec*`, `posix_spawn` or `_exit`, as usual), and the calls nest. This is tried on RISC OS Pyromaniac (`riscos64/unixlib/tests/t_run.c`, `t_vfork.c`); it is off unless the variable is set, because other kernels have not been checked. Without it, a started program replaces its parent. Commands of the operating system (`*Set`, `*Echo`...) are always fine. `fork` fails with `ENOSYS`. Also not yet: threads, sockets, signals that arrive from outside (Escape, a timer), dynamic loading. `termios`, `poll` and `select` exist for the console (`tcgetattr`/`tcsetattr`, keys returned as they come without `ICANON`).

## A module

```
aarch64-riscos-gcc -mmodule -O2 -c hello.c -o hello.o
cmunge64 -p -d helloh.h -o hello_hdr.o hello.cmhg         # (the module header and the C header from a CMHG file)
aarch64-riscos-gcc -mmodule -O2 hello.o hello_hdr.o -o Hello,ffa
```

`-mmodule` (or `-mmodule-fp`, which allows floating point and vector registers in the module) compiles without a library or a GOT, links the kit's runtime and C library with the module linker script and turns the
result into the module image (file type `&FFA`). `module64.mk` (in `PREFIX/share/riscos64`) has the make rules. See [MODULES64.md](MODULES64.md) for the C interface, the CMHG subset, the kit's C library and what is
not supported.

## What is tested

- programs: `riscos64/sdk/tests` (`sdktest`, `cxxtest`, zlib), the library test `riscos64/libtest`, UnixLib 64's tests (`riscos64/unixlib/tests`, with UnixLib's own `testriscosify` and `testunixify`), GMP, MPFR and MPC (their test programs; 168 of 168, 155 of 157 and 63 of 64 passed in the run with a file-name shim, the run with `-munixlib` is in progress);
- modules: the examples in `riscos64/kit/examples` (hello64, svc64, swi64, dec64, cxx64, fpmod64, libmod64) and the library test as a module.

## Limits of this stage

The linker writes the AIF64 image of a program itself (`ld --aif64`, which the driver passes: the loadable contents of the linked ELF file, the same bytes as `objcopy -O binary`); for a module the driver runs `modreloc64` after the link. There is no debug
information yet that a RISC OS debugger reads. The compiler runs on a PC.

**Tools that run on RISC OS 64 (experimental).** [`riscos64/ports/build-native-binutils.sh`](../riscos64/ports/build-native-binutils.sh) builds binutils 2.45.1 as programs for RISC OS 64, and [`build-native-gcc.sh`](../riscos64/ports/build-native-gcc.sh) does the same for GCC. On RISC OS Pyromaniac the GNU assembler, linker, `objcopy`, `objdump`, `readelf` and `ar` run: the assembler turns a `.s` file made by the cross compiler into an object, the linker links it with the SDK's start-up files and libraries and writes the AIF64 image (`--aif64`; the first native tests used `objcopy -O binary` for that step), and the program runs and prints. The GCC driver (`xgcc`) runs too: with `*Set UnixEnv$swap 1` it starts the native assembler, `collect2` and the linker as child programs and `objcopy` to make the image, and the program it links runs. The compiler proper (`cc1`, 34 MB) builds but has not been run yet: the build service gives a program only about 12 MB of application space, so `cc1` needs a machine with a larger one.
