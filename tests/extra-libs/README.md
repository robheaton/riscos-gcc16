# Checks of libffi and OpenSSL

Small programs that check the libraries of [docs/EXTRA-LIBS.md](../../docs/EXTRA-LIBS.md) on RISC OS (they print `ok` per check and `all passed` at the end; the return code is 0 when everything passed):

| Program | What |
|---|---|
| `fficheck.c` | libffi: calls with mixed integer, floating-point, 64-bit and structure arguments and results; a closure |
| `sslcheck.c` | OpenSSL: SHA-1, SHA-256, `RAND_bytes`, AES-128-CBC, an EC key, a self-signed certificate, a TLS handshake and data through memory BIOs |
| `threadcheck.c` | OpenSSL from four threads at once with shared `SSL_CTX` objects |

Build them with the unpacked library package in the tool chain:

```
CC=arm-riscos-gnueabihf-gcc
$CC -O2 fficheck.c -lffi -o fficheck
$CC -O2 sslcheck.c -lssl -lcrypto -o sslcheck
$CC -O2 threadcheck.c -lssl -lcrypto -o threadcheck
```

`sslcheck` and `threadcheck` are 2 MB programs: start them from a Task window after `WimpSlot -min 64M -max 64M` (in the default slot they stop with "Wimpslot not big enough to run ELF program").
