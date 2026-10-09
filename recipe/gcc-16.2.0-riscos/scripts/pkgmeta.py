"""Metadata of the PackMan (RiscPkg) packages: RiscPkg/Control and RiscPkg/Copyright.

Shared by the package generators (make-c16-package.py, make-cxx-package.py, make-fortran-package.py, make-native-package.py)
and by retag-package.py, which puts this metadata into an existing, tested package without touching any other byte of it.
"""

REPO = "https://github.com/robheaton/riscos-gcc16"
MAINTAINER = "Rob Heaton <rob@robheaton.co.uk>"
REPORT = "Please report problems at %s, not to the GCCSDK mailing list: this is an experimental port that GCCSDK does not maintain." % REPO

DESCRIPTION = {
    "SharedLibs-C-armeabihf": "C runtime for EABI hard-float programs: UnixLib 5.0 rebuilt with GCC 16.2.0 and fixed, with the loader and libgcc_s of GCCSDK 10.2.0 (experimental)",
    "SharedLibs-C++-armeabihf": "libstdc++ 6.0.36 (GCC 16.2.0) for dynamically linked C++ programs (experimental)",
    "SharedLibs-Fortran-armeabihf": "libgfortran 5 (GCC 16.2.0) for dynamically linked Fortran programs (experimental)",
}


def control(name, version, licence, description, depends=None, components=None):
    """The text of RiscPkg/Control (field order as in GCCSDK's own packages)."""
    t = "Package: %s\nVersion: %s\nSection: Development\nPriority: Optional\nEnvironment: arm\nLicence: %s\nMaintainer: %s\nStandards-Version: 0.4.0\n" % (
        name, version, licence, MAINTAINER)
    if components:
        t += "Components: %s\n" % components
    if depends:
        t += "Depends: %s\n" % depends
    return t + "Description: %s\n" % description


def copyright_c(version, note):
    """RiscPkg/Copyright of SharedLibs-C-armeabihf.  NOTE = the per-release change log text of make-c16-package.py."""
    return """SharedLibs-C-armeabihf %s - the C runtime for EABI (arm-riscos-gnueabihf) programs

What this is
  An experimental rebuild of GCCSDK's SharedLibs-C-armeabihf package.  libunixlib.so.5.0.0 and libm.so.1.0.0 are UnixLib 5.0 (GCCSDK trunk r7800) rebuilt with
  GCC 16.2.0 and binutils 2.45.1, with the fixes listed below.  The loader (ld-riscos-eabihf.so, ld-riscos.so.2), libgcc_s.so.1 and libdl are the unchanged files of the
  GCCSDK autobuilder package SharedLibs-C-armeabihf 10.2.0-1 (built from GCC 10.2.0 and GCCSDK sources: http://www.riscos.info/index.php/GCCSDK#GCCSDK_Autobuilder).

Source, patches and build instructions
  %s  (patches-unixlib/, docs/RUNTIME.md)
  UnixLib sources: the GCCSDK svn, svn://svn.riscos.info/gccsdk/trunk (r7800), gcc4/recipe/files/gcc/libunixlib
  %s

Licences
  UnixLib: the revised BSD licence for most files; some files are under the GNU Library General Public Licence or carry other BSD-style notices (see doc/UnixLib/COPYING
  in the UnixLib sources).  libgcc_s: GNU General Public License version 3 or later with the GCC Runtime Library Exception.  The patches of this port are under the licence
  of the files they change.

Changes against GCCSDK's 10.2.0-1: %s
""" % (version, REPO, REPORT, note)


def copyright_runtime(name, version, what, licence_line, source_line, note):
    """RiscPkg/Copyright of SharedLibs-C++-armeabihf and SharedLibs-Fortran-armeabihf."""
    t = ("%s %s - %s\n\n"
         "Licence: %s\n"
         "Source: %s\n"
         "Port: built with the experimental GCC 16.2.0 forward-port of the GCCSDK EABI tool chain and linked with binutils 2.45.1; recipe and patches in %s\n"
         "%s\n") % (name, version, what, licence_line, source_line, REPO, REPORT)
    if note:
        t += "\n" + note.strip("\n") + "\n"
    return t


def copyright_gcc16(version):
    """RiscPkg/Copyright of Gcc16."""
    apache = ""
    if int(version.split("-")[-1]) >= 16:        # modkit/include/swisnums.h came with 16.2.0-16
        apache = ("\nThe module kit's header swis.h includes swisnums.h, the names and numbers of the SWIs of the OS (macros only), made from the assembler headers of the RISC OS Open sources, which are under the\n"
                  "Apache License 2.0 (Copyright Castle Technology Ltd, RISC OS Open Ltd and others): the licence text is in docs/Apache-2.0 of this package.\n")
    return """The native GCC 16.2.0 tool chain for RISC OS (Gcc16 %s): GCC 16.2.0, binutils 2.45.1 and GNU make 4.4.1 (GNU General Public License, version 3 or later), with the GCCSDK port
changes for arm-riscos-gnueabihf forward-ported to these versions.  UnixLib is not part of this package (see SharedLibs-C-armeabihf); the headers and libraries in
arm-riscos-gnueabihf/ come from UnixLib 5.0 (GCCSDK, revised BSD licence for most files) and the libstdc++ and libgcc of GCC 16.2.0 (with the GCC Runtime Library Exception).
The icon sprites are those of GCCSDK's own gcc package.

Sources: GCC 16.2.0, binutils 2.45.1 and make 4.4.1 from the GNU project (https://ftp.gnu.org/gnu/), with the patches, new files and build scripts of %s
(recipe/gcc-16.2.0-riscos, recipe/binutils-2.45.1-riscos, recipe/make-4.4.1-riscos); docs/ReadMe has the instructions for use.
%s%s
""" % (version, REPO, apache, REPORT)
