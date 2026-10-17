/* (UnixLib 64: the test of UnixLib itself, test/filename/testunixify.c, revised BSD licence; it ends with return 0 always: the 7 failures that UnixLib itself has are listed in expected-oracle.pyromaniac.txt.)
   Regression tests for unixify
   Copyright (c) 2006 UnixLib Developers.

*/


#include <stdio.h>
#include <string.h>
#include <unixlib/local.h>

struct test {
	char *in;
	char *out;
	int expectedfail; /* Set if the test is known to fail on the current implementation */
};

struct test tests[] = {
/*
   Relative paths
*/

/* Unix */
{".plan", ".plan", 1},
{"..cunningplan", "..cunningplan", 1},
{"config/arm/rname.c", "config/arm/rname.c", 0},
{"funky.cold.medina.c", "funky.cold.medina.c", 1},
{".", ".", 0},
{"..", "..", 0},
{"../cc.cool", "../cc.cool", 1},
{"./././c.cool", "./././c.cool", 1},
{"../../../c.smart", "../../../c.smart", 1},
{"./.plan", "./.plan", 1},
{"./..cunningplan", "./..cunningplan", 1},
{"./foo/fred.php","./foo/fred.php", 1},
{"./config/arm/rname.c", "./config/arm/rname.c", 0},
{"./funky.cold.medina.c", "./funky.cold.medina.c", 1},
{"../../arm-riscos-aof/libiberty/libiberty", "../../arm-riscos-aof/libiberty/libiberty", 0},

/* RISC OS */
{"c.fred", "fred.c", 0},
{"foo.c.fred", "foo/fred.c", 0},
{"foo.bar.o.getopt", "foo/bar/getopt.o", 0},
{"cc.smart", "smart.cc", 0},
{"foo.fred/c/text-base", "foo/fred.c.text-base", 0},
{"dir.file/h", "dir/file.h", 0},
{"file/h", "file.h", 1},
{"cc.c++.c.for.lots.of.prefixes.cc.cool", "cc/c++/c/for/lots/of/prefixes/cool.cc", 0},

/* Ambiguous */
/* These could be either, but I think the safest is to assume they are unix and therefore don't translate them */
{"file/php", "file.php", 1},
{"fred/c/text-base", "fred.c.text-base", 1},
{"gcc-2/7/2/2/tar/gz", "gcc-2.7.2.2.tar.gz", 1},
{"foo/fred.php","foo/fred.php", 1},
{"c/fred", "c/fred", 0},
{"php/smart", "php/smart", 0},
{"a", "a", 0},
{"abba", "abba", 0},
{"a.c", "a.c", 0},
{"c.a", "c.a", 0},
{"aa.c", "aa.c", 0},
{"c.aa", "aa.c", 0},
{"hello.world", "hello.world", 1},
{"objects/EltNode-h", "objects/EltNode-h", 0},
{"objects.EltNode-m", "objects.EltNode-m", 1},

/*
   Full paths
*/

/* Unix */
{"/plan", "/plan", 0},
{"//cunningplan", "//cunningplan", 0},
{"/arm/rname.c", "/arm/rname.c", 0},
{"/arm/c.rname", "/arm/c.rname", 0},
{"/arm/c/rname", "/arm/c/rname", 0},
{"/arm/rname/c", "/arm/rname/c", 0},
{"/idefs::hd/$/fred/preset.s", "/idefs::hd/$/fred/preset.s", 0},
{"/idefs::hd.$/fred/preset.s", "/idefs::hd.$/fred/preset.s", 0},
{"/idefs::hd.$/fred/preset/s", "/idefs::hd.$/fred/preset/s", 0},
{"/idefs::hd.$/fred/s/preset", "/idefs::hd.$/fred/s/preset", 0},
{"/idefs::hd.$/fred/s.preset", "/idefs::hd.$/fred/s.preset", 0},

/* RISC OS */
{":4.$.something", "/:4.$/something", 0},
{"$.work.!unixlib", "/work/!unixlib", 0},
{"idefs::4.$.fred.preset.s", "/idefs::4.$/fred/preset/s", 1},
{"idefs::4.$.fred.s.preset", "/idefs::4.$/fred/preset.s", 0},
{"idefs::4.$.fred", "/idefs::4.$/fred", 0},
{"$.c.rname", "/rname.c", 0},
{"$.arm.c.rname", "/arm/rname.c", 0},
{"Devices#baud9600:serial", "/Devices#baud9600:serial", 0},
{"LanMan98#notypes::mint.$.index/php", "/LanMan98#notypes::mint.$/index.php", 0},
{"LanMan98#no.types::mint.$.index/php", "/LanMan98#no.types::mint.$/index.php", 0},
{"LanMan98#no/types::mint.$.index/php", "/LanMan98#no/types::mint.$/index.php", 0},
{"$.home.riscos.env", "/home/riscos/env", 0},
{"$.a.long.path.name.that.is.going.to.end.in.a/", "/a/long/path/name/that/is/going/to/end/in/a.", 0},
{"$.a.long.path.name.that.is.not.going.to.end.in.a", "/a/long/path/name/that/is/not/going/to/end/in/a", 1},
{"$.a.long.path.name.that.has.aaaa/.in.the.middle", "/a/long/path/name/that/has/aaaa./in/the/middle", 0},
{"$", "/", 1},
{"$./a", "/.a", 0},
{"$.//a", "/..a", 0},
{"$./a/", "/.a.", 0},
{"$./a//", "/.a..", 0},
{"$.a.b.c.d.e//", "/a/b/c/d/e..", 0},
{"$.a.b.c.d.e/", "/a/b/c/d/e.", 0},
{"ADFS::4.$.foo.h.info/php", "/ADFS::4.$/foo/h/info.php", 0},
{"IDEFS::HD.$.Work.gcc.gcc-272.config.arm.c.rname", "/IDEFS::HD.$/Work/gcc/gcc-272/config/arm/rname.c", 0},
{"IDEFS::HD.$.Work.gcc.gcc-272.config.arm.for.rname", "/IDEFS::HD.$/Work/gcc/gcc-272/config/arm/for/rname", 0},

/*
   Path variables
*/

/* Unix */
{"/gcc:/../getopt.c", "/gcc:/../getopt.c", 0},
{"DeskLib:foobar.h", "/DeskLib:foobar.h", 0},
{"GCC:objc/list.h", "/GCC:objc/list.h", 0},
{"gcc:/../getopt.c", "/gcc:/../getopt.c", 1},
{"gcc:/getopt.c", "/gcc:/getopt.c", 0},
{"/gcc:o.libgcc", "/gcc:o.libgcc", 0},
{"/gcc:foo.c.getopt", "/gcc:foo/getopt.c", 1},
{"/foo:bar.baz", "/foo:bar.baz", 0},
{"/gcc:o/libgcc", "/gcc:o/libgcc", 0},
{"/gcc:foo.getopt/php", "/gcc:foo.getopt/php", 0},
{"/gcc:getopt.php", "/gcc:getopt.php", 0},
{"/gcc:getopt/php", "/gcc:getopt/php", 0},

/* RISC OS */
{"foo:", "/foo:", 0},
{"gcc:o.libgcc", "/gcc:libgcc.o", 1},
{"gcc:foo.c.getopt", "/gcc:foo/getopt.c", 0},
{"gccpkg:lib.gcc-lib.arm-riscos-aof.2_95_4.include.h.time", "/gccpkg:lib/gcc-lib/arm-riscos-aof/2_95_4/include/time.h", 0},
{"gccpkg:include.unixlib.h.stddef", "/gccpkg:include/unixlib/stddef.h", 0},
{"gcc:^.o/libgcc", "/gcc:/../o.libgcc", 0},
{"gcc:^.o.libgcc", "/gcc:/../libgcc.o", 0},
{"gcc:^.getopt.c", "/gcc:/../getopt/c", 1},

/* Ambiguous */
/* Probably safest to assume they are RISC OS format */
{"foo:bar.baz", "/foo:bar/baz", 0},
{"gcc:o/libgcc", "/gcc:o.libgcc", 1},
{"gcc:foo.getopt/php", "/gcc:foo/getopt.php", 0},
{"gcc:getopt.php", "/gcc:getopt/php", 0},
{"gcc:getopt/php", "/gcc:getopt.php", 1},

/*
   System variables
*/

/* Unix */
{"/<GCC$dir>/cc.smart", "/<GCC$dir>/cc.smart", 0},
{"/<GCC$Dir>/cc/smart", "/<GCC$Dir>/cc/smart", 0},
{"/<GCC$Dir>/smart.cc", "/<GCC$Dir>/smart.cc", 0},
{"/<GCC$Dir>/smart/cc", "/<GCC$Dir>/smart/cc", 0},
{"/<GCC$Dir>/config", "/<GCC$Dir>/config", 0},
{"/<GCC$Dir>/../config", "/<GCC$Dir>/../config", 0},
{"/<Wimp$ScrapDir>/foo/bar/c.file", "/<Wimp$ScrapDir>/foo/bar/c.file", 0},
{"/<Wimp$ScrapDir>/foo/bar/c/file", "/<Wimp$ScrapDir>/foo/bar/c/file", 0},
{"/<Wimp$ScrapDir>/foo/bar/file.c", "/<Wimp$ScrapDir>/foo/bar/file.c", 0},
{"/<Wimp$ScrapDir>/foo/bar/file/c", "/<Wimp$ScrapDir>/foo/bar/file/c", 0},

/* RISC OS */
{"<GCC$dir>.cc.smart", "/<GCC$dir>/smart.cc", 1},
{"<GCC$Dir>.cc/smart", "/<GCC$Dir>/cc.smart", 1},
{"<GCC$Dir>.smart.cc", "/<GCC$Dir>/smart/cc", 1},
{"<GCC$Dir>.smart/cc", "/<GCC$Dir>/smart.cc", 1},
{"<GCC$Dir>.config", "/<GCC$Dir>/config", 1},
{"<GCC$Dir>.^.config", "/<GCC$Dir>/../config", 1},
{"<Wimp$ScrapDir>.foo.bar.c.file", "/<Wimp$ScrapDir>/foo/bar/file.c", 1},
{"<Wimp$ScrapDir>.foo.bar.c/file", "/<Wimp$ScrapDir>/foo/bar/c.file", 1},
{"<Wimp$ScrapDir>.foo.bar.file.c", "/<Wimp$ScrapDir>/foo/bar/file/c", 1},
{"<Wimp$ScrapDir>.foo.bar.file/c", "/<Wimp$ScrapDir>/foo/bar/file.c", 1},

/*
   RISC OS special paths
*/
{"%", "/lib", 1},
{"%.cc1", "/lib/cc1", 0},
{"%.o.cc1", "/lib/cc1.o", 0},
{"%.o/cc1", "/lib/o.cc1", 0},
{"%.cc1.o", "/lib/cc1/o", 1},
{"%.cc1/o", "/lib/cc1.o", 0},
{"%.cool/php", "/lib/cool.php", 0},
{"%.something.in.the.library", "/lib/something/in/the/library", 0},

{"^", "..", 1},
{"^.cc1", "../cc1", 0},
{"^.o.cc1", "../cc1.o", 0},
{"^.o/cc1", "../o.cc1", 0},
{"^.cc1.o", "../cc1/o", 1},
{"^.cc1/o", "../cc1.o", 0},
{"^.cool/php", "../cool.php", 0},
{"^.something.in.the.library", "../something/in/the/library", 0},
{"wibble.wobble.^.o.cc1", "wibble/wobble/../cc1.o", 0},
{"wibble.wobble.^.o/cc1", "wibble/wobble/../o.cc1", 0},
{"wibble.wobble.^.cc1.o", "wibble/wobble/../cc1/o", 1},
{"wibble.wobble.^.cc1/o", "wibble/wobble/../cc1.o", 0},
{"wibble.wobble.^.something.in.the.library", "wibble/wobble/../something/in/the/library", 0},
{"wibble/wobble.^.something.in/the.library", "wibble.wobble/../something/in.the/library", 0},
{"^.^.^.c.cool", "../../../cool.c", 0},
{"^.^.^.hot.c.cool", "../../../hot/cool.c", 0},

{"@", ".", 1},
{"@.cc1", "./cc1", 0},
{"@.o.cc1", "./cc1.o", 0},
{"@.o/cc1", "./o.cc1", 0},
{"@.cc1.o", "./cc1/o", 1},
{"@.cc1/o", "./cc1.o", 0},
{"@.cool/php", "./cool.php", 0},
{"@.something.in.the.library", "./something/in/the/library", 0},

{"","", 0},
{NULL, NULL, 0}};



int main (void)
{
	char buffer[256];
	char buffer2[256];
	int i;
	int j;
	int fail = 0;
	int filetype = 0;
	int passes = 0;
	int fails = 0;
	int xfails = 0;

	i = 0;
	do {
		printf("Testing %s\n",tests[i].in);
		__unixify(tests[i].in, 0, buffer, sizeof(buffer), filetype);

		fail = strcmp(buffer,tests[i].out);
		printf("unixified: %s\n",buffer);
		for (j=0;j<60;j++) printf(" ");
		printf("unixify %s %s\n", fail ? "FAIL" : "pass", tests[i].expectedfail ? "(expected fail)" : "");
		__riscosify(buffer, 0 ,0 | __RISCOSIFY_FILETYPE_EXT, buffer2, sizeof(buffer2), &filetype);

		printf("riscosified back again %s : %s\n\n\n",buffer2,strcmp(tests[i].in,buffer2) ? "DIFFERS" : "same");
		if (fail) {
			if (tests[i].expectedfail) xfails++; else fails++;
		} else {
			passes++;
		}
		if (tests[i].expectedfail) fail = !fail;
	} while (tests[++i].in /*&& !fail*/);

	printf("Passes %d\nFails %d\nExpected fails %d\n",passes, fails, xfails);
	printf("SUMMARY unixify: %d passed, %d failed, %d expected failures\n", passes, fails, xfails);
	return 0;
}



