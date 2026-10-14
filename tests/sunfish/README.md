# Sunfish: a 225 KB module built with the module kit

[Sunfish](https://github.com/ajw498/NFS) is Alex Waugh's NFS client filing system for RISC OS (MIT licence): a module of about 40 C compilations (34 distinct `.c` files, 22 of them written by the repository's `ProcessSpec`) and four assembler files that registers a filing system, talks to NFS servers over UDP or TCP through the Internet module, and keeps its own cache. It was suggested as a test of module support because a build with GCCSDK 4.7.4 had been reported to lock the machine up at once. It is a good test: it is much bigger than the usual example, it uses sockets (through `ioctl` and `close` on a socket, as code written for Acorn's TCPIPLibs does), `iconv`, `<sys/param.h>`, `<net/if.h>`, the filing system entry points of FileSwitch and the Free module.

`build-sunfish.sh` builds it from the **unmodified** source of the repository (revision `afa0a8c`, 30 December 2020, the one that was tested), with no TCPIPLibs and no libiconv: the kit has sockets, `netdb` and `iconv`.

```
git clone https://github.com/ajw498/NFS
OSLIB=<folder that has oslib/os.h> ASASM=<GCCSDK's asasm> build-sunfish.sh NFS /tmp/sunfish <tool chain>
```

It needs Perl (the RPC code is written by the repository's `ProcessSpec`), GCCSDK's `asasm` (the four `.s` files are in ObjAsm), OSLib's headers (`install-oslib.py` puts them in the tool chain; only the headers are used, no `libOSLib32.a`), and the tool chain with the kit. The result is `Sunfish,ffa` (224,264 bytes with `-O0 -mpoke-function-name`, the Makefile's debug build; `OPT=-O1` gives 144,808; with the kit of 16.2.0-22 and earlier, which kept every function of the library that a module's object files named, they were 298,516 and 189,784), plus two small UnixLib programs for the tests below. `asasm` prints a warning about `PLD` in `memcpy.s` (it assumes an old architecture); the instruction runs on ARMv6 and later.

## What it showed

On the Raspberry Pi (RISC OS 5.30, 13 October 2026), with the module built by this script (the runs with the 298,516 byte module of the kit before the dropping of unused functions, and again with the 224,264 byte one of 16.2.0-23, with the same results and the same CRC):

* `RMLoad` and `RMKill` work, with no lock-up (also with `-O1`). `sfcheck` asks FileSwitch about `Sunfish::NoSuchDisc.$.x` and gets "Disc not found" from Sunfish itself, then calls `Sunfish_ListMounts` and `Sunfish_Free`.
* `sfnfs SERVER EXPORT` mounts an NFS export with `Sunfish_Mount` (NFS3 over UDP), lists the top directory with `OS_GBPB 10`, opens the first regular file it finds and reads its first 64 KB with `OS_Find` and `OS_GBPB 3`, prints their CRC-32 and dismounts. **It writes nothing.** On a NAS (27 entries in the top directory; a file of 733 MB) the CRC was the one that an independent NFS client gave: `nfs3peek.py SERVER EXPORT PATH --crc` is a small read-only NFS3 client in Python (the calls MNT, LOOKUP, GETATTR, READDIR and READ) that you can run on the host to get the number to compare with.
* The first try failed at the mount with "Unable to connect or bind socket (Bad file descriptor)": Sunfish makes its socket non-blocking with `ioctl` and closes it with `close`, which were calls on files in the kit. In 16.2.0-23 a socket is a file descriptor ([MODULES.md](../../docs/MODULES.md#sockets)) and the same source mounts, lists and reads.

It also needed `<sys/errno.h>`, `<sys/param.h>` and `<net/if.h>` (new in 16.2.0-23) and the corrected `SIOC...` request numbers of `<sys/ioctl.h>` (the RISC OS Internet module's numbers for the "get" requests are not 4.4BSD's). The sources are compiled with `-std=c99`, in which the kit's headers declare only what ISO C has; `-D_DEFAULT_SOURCE` (the script's) brings `strdup` and `strcasecmp` back.

**Not tested:** any write to the server (Sunfish's write path), TCP, NFS2 and NFS4, the `Encoding:` key of a mount (the kit's `iconv` has UTF-8, ASCII, Latin 1, UTF-16 and UTF-32 only), the Free module path (`*Free`), and the Wimp front end (C++ with DeskLib, not built here). Do not point `sfnfs` at data you cannot afford to risk even though it only reads: a filing system module that fails can take the machine with it.

## Running it on the Pi

Copy `Sunfish,ffa`, `sfcheck,e1f` and `sfnfs,e1f` to the machine (a Task window):

```
RMLoad <path>.Sunfish
Run <path>.sfcheck
Run <path>.sfnfs <server> </path/of/the/export> NAS
RMKill Sunfish
```

Each step of `sfnfs` prints a line before it starts, so if the machine stops answering, the last line on the screen says where. If a Sunfish is already loaded, `RMLoad` replaces it until the next reset.
