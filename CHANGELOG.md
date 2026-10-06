# Changelog

## v16.2.0-11: 2026-10-05, the first public release

| Asset | Version |
|---|---|
| native compiler `Gcc16` | 16.2.0-11 |
| runtime `SharedLibs-C-armeabihf` | 16.2.0-11 (UnixLib fix level 13) |
| runtime `SharedLibs-C++-armeabihf` | 16.2.0-5 (unchanged) |
| runtime `SharedLibs-Fortran-armeabihf` | 16.2.0-2 (unchanged) |
| Linux cross compiler | 16.2.0-11 |

New since the last internal release (16.2.0-10):

* **`scanf` understands `long long`** (`ll`, `q`, `j`, `hh`, `z`, `t` and `%Lf`): the root cause of the native `-flto` failures. UnixLib fix level 13.
* **`-flto=N`, `-flto=auto` and a make job server work in the native compiler** (the optimisation jobs run one after the other).
* The package metadata names the real maintainer and points at this repository.
* Checked by the full regression run: 50 summary lines identical to the previous release, none failing.
* The build instructions ([docs/BUILDING.md](docs/BUILDING.md)) were checked by running every command again in a fresh copy of the repository; the Linux tarball of this release is that rebuild.

## The internal releases before it (not published)

| Native compiler | What it added |
|---|---|
| 16.2.0-1 to -5 | the first native compilers, `make` 4.4.1, the heap in dynamic areas, sizes of heaps built in |
| 16.2.0-6 | `gfortran` |
| 16.2.0-7 | 64 MB stacks for `cc1`, `cc1plus` and `f951`, 8 MB for `make` |
| 16.2.0-8 | throwback (`-mthrowback`) |
| 16.2.0-9, -10 | native `-flto` (test packages; `-10` carried a workaround for the `scanf` bug) |

The runtime's fix levels 5 to 13 are described in [docs/RUNTIME.md](docs/RUNTIME.md).
