# Upstream tests of the ports, on RISC OS Pyromaniac (18 October 2026)

Scripts for the shell of this port (`sh`) that run the tests that come with the ports: `xzt` (xz's `tests/files`), `rt_lz4`, `rt_zstd`, `rt_xz` (round trips at several levels), `pct` (PCRE2's `testinput`/`testoutput` pairs; `pct 1 2 4 5 6 7 8 9`, with the files of PCRE2's `testdata` in the current folder) and `jpgt` (libjpeg-turbo's md5 tests of `cjpeg`, `djpeg` and `jpegtran`, taken from its `CMakeLists.txt`). The results are in docs/PORTS-TESTS.md.
