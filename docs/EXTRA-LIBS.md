# libffi and OpenSSL for the 32-bit tool chain

Static libraries and headers for `arm-riscos-gnueabihf` (the 32-bit, hard-float EABI target of this tool chain), built from the upstream sources with the cross compiler:

| Library | Version | Licence |
|---|---|---|
| libffi | 3.8.0 | MIT ([text](../licenses/libffi-MIT.txt)) |
| OpenSSL | 1.1.1w | the OpenSSL licence and the SSLeay licence ([text](../licenses/OpenSSL-1.1.1-LICENSE.txt)) |

They are in the pre-release [`libs-armeabihf-1`](https://github.com/robheaton/riscos-gcc16/releases/tag/libs-armeabihf-1) (`riscos-gcc16-extra-libs-ffi3.8.0-ssl1.1.1w.tar.xz`, 1.2 MB), with the upstream source tarballs and a `SHA256SUMS` file. The recipe is [`recipe/extra-libs/build-extra-libs.sh`](../recipe/extra-libs/build-extra-libs.sh).

**Why rebuild them?** Libraries built for GCCSDK 4.7.4 (or 3.4) are for the old soft-float ABI; a program built with this tool chain is hard-float EABI and cannot link them. They have to be built again, from source, with this compiler. Most autotools projects need only `--host=arm-riscos-gnueabihf --disable-shared`; these two have a few details that are worth having written down.

**Why OpenSSL 1.1.1?** It is the series that Python 3.8 and many older programs expect (they do not build against OpenSSL 3). Upstream stopped supporting 1.1.1 in September 2023: use it to port existing programs, not for new work that can use a current version. [`build-netsurf.py`](../recipe/gcc-16.2.0-riscos/scripts/build-netsurf.py) builds OpenSSL 4.0.3 for NetSurf ([NETSURF.md](NETSURF.md)).

## Install

Unpack over the cross compiler's directory, so that the files land next to `libc.a`:

```
tar -C /path/to/riscos-gcc16-cross-16.2.0-24-x86_64-linux -xf riscos-gcc16-extra-libs-ffi3.8.0-ssl1.1.1w.tar.xz
```

After that `-lffi` and `-lssl -lcrypto` work with no `-I` or `-L`. `pkg-config` finds `libffi`, `libssl`, `libcrypto` and `openssl` when `PKG_CONFIG_LIBDIR` (or `PKG_CONFIG_PATH`) contains `.../arm-riscos-gnueabihf/lib/pkgconfig`; the `.pc` files find their own folder, so the tool chain can be moved. For static libraries ask for `pkg-config --static --libs libssl`, otherwise `-lcrypto` is left out.

To keep them apart from the tool chain, unpack into another directory and add `-I<dir>/arm-riscos-gnueabihf/include -L<dir>/arm-riscos-gnueabihf/lib`. Link order matters for static libraries: `-lssl -lcrypto`.

The files are for tool chain 16.2.0-24 or later (they were built with it). The package is for the cross compiler on Linux; it is not packaged for the native compiler on RISC OS.

## Using them in a port

* Autotools projects: `./configure --host=arm-riscos-gnueabihf --disable-shared ...` and, when the project asks where libffi or OpenSSL are, the directory you unpacked into. CMake projects: a toolchain file with `CMAKE_SYSTEM_NAME Generic` ([CROSS-COMPILER.md](CROSS-COMPILER.md)).
* Python 3.8 as an example: `--with-system-ffi` makes `_ctypes` use libffi, and `_ssl` and `_hashlib` are enabled in `Modules/Setup` with `SSL=` set to the unpack directory. **Python itself has not been built against these libraries here**; those are CPython's own documented options, not something that was tried.

## What is different on RISC OS

* **Static only.** Everything is linked into the program; there are no `.so` files for these.
* **No `-pthread`.** Threads are in UnixLib and the compiler does not know the option ([KNOWN-ISSUES.md](KNOWN-ISSUES.md)). OpenSSL's `BSD-generic32` target adds `-pthread` and `-lpthread`, so the recipe defines a small target of its own, `riscos-gnueabihf`, that inherits from it without them. Other projects that insist on `-pthread` need the same treatment.
* **OpenSSL is built thread safe** (`OPENSSL_THREADS`), with `no-shared no-pic no-asm no-dso no-async no-tests no-ui-console no-devcryptoeng`: plain C (no ARM assembler), no engines loaded at run time.
* **Random numbers:** `RAND_bytes` reads `/dev/urandom`, which UnixLib serves from the **CryptRandom** module (part of RISC OS 5). The module must be loaded.
* **Certificates:** the compiled-in certificate directory is `/OpenSSL`, which does not exist. Give a program its CA bundle explicitly: the `SSL_CERT_FILE` or `SSL_CERT_DIR` variables, `SSL_CTX_load_verify_locations`, or the `cafile=` argument of Python's `ssl.create_default_context`. A bundle such as the curl project's `cacert.pem` is the usual choice.
* **libffi** is built for the ARM VFP calling convention (`FFI_VFP`). Closures are plain `malloc`ed blocks (RISC OS has no non-executable memory and no `mmap`); free them with `ffi_closure_free` as usual.

## Rebuilding

`recipe/extra-libs/build-extra-libs.sh TOOLCHAIN_DIR [WORKDIR]` downloads the two source tarballs, checks their SHA-256 and builds and installs everything into `TOOLCHAIN_DIR/arm-riscos-gnueabihf`. `STAGE_ONLY=1` collects the files in `WORKDIR/stage` instead (that is how the release package was made). It needs `curl`, `make`, `perl` and `tar` besides the tool chain.

| Source | SHA-256 |
|---|---|
| `libffi-3.8.0.tar.gz` (https://github.com/libffi/libffi/releases/download/v3.8.0/) | `7da3e2d9a171eb0a038f592ecad3ff2bb2550f3496d87b3b29ad0cf4430c0db4` |
| `openssl-1.1.1w.tar.gz` (https://www.openssl.org/source/old/1.1.1/) | `cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8` |

## How it was checked

Three small programs ([`tests/extra-libs`](../tests/extra-libs), built with the tool chain from the unpacked package) were run on a Raspberry Pi Compute Module 4 with RISC OS 5.30:

* `fficheck`: `ffi_prep_cif` and `ffi_call` with mixed integer, floating-point, 64-bit and structure arguments and results, and a closure called as an ordinary C function (9 checks, all passed).
* `sslcheck`: SHA-1 and SHA-256 test vectors, `RAND_bytes`, an AES-128-CBC round trip, EC P-256 key generation, a self-signed certificate, and a TLS 1.3 handshake plus data in both directions between a client and a server that talk through memory BIOs (all passed).
* `threadcheck`: four threads at once, each doing digests, random numbers, a key generation and six TLS handshakes, with the two `SSL_CTX` objects shared by all the threads (all passed, five runs).

`sslcheck` and `threadcheck` are 2 MB programs and do not start in the default program slot (they stop with "Wimpslot not big enough to run ELF program"): they were run from a Task window with `WimpSlot -min 64M -max 64M` set first. These are checks that the libraries work on RISC OS; they are not a test suite of OpenSSL or libffi, and OpenSSL's own tests were not run.
