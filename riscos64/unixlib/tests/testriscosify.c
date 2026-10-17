/* Test riscosify
 * Written by Alex Waugh
 * Copyright (c) 2003-2006 UnixLib Developers
 */

/* (UnixLib 64: this is the test of UnixLib itself, test/filename/testriscosify.c, revised BSD licence.  Changes: it no longer stops at the first failure, counts, and ends with a SUMMARY line.)

Regression tests for riscosify

Before running:
*unset UnixFS$/tmp
*set UnixEnv$testriscosify$sfix c:o:h:cc:s:cpp
*set UnixFS$/xxx xxx

*/


#include <stdio.h>
#include <string.h>
#include <unixlib/local.h>

char tests[][2][256] = {
{"/tmp","<Wimp$ScrapDir>"},
{"/tmp/file","<Wimp$ScrapDir>.file"},
{"/tmp/file.c","<Wimp$ScrapDir>.c.file"},
{"/usr/xxx/fred.c","xxx.c.fred"},
{"/xxx/fred.php","xxx.fred/php"},
{"$.work.!unixlib","$.work.!unixlib"},
{"$/work/!unixlib","$.work.!unixlib"},
{"%/cc1,ffb","%.cc1"},
{"%/cc1.o","%.o.cc1"},
{"%/cc1.o,ffb","%.o.cc1"},
{"%.cc1","%.cc1"},
{"^.fred/c","^.fred/c"},
{"<GCC$dir>.cc.smart","<GCC$dir>.cc.smart"},
{"<GCC$Dir>/cc.smart","<GCC$Dir>.cc/smart"},
{"<GCC$Dir>/smart.cc","<GCC$Dir>.cc.smart"},
{"<GCC$Dir>.cc.smart","<GCC$Dir>.cc.smart"},
{"<GCC$Dir>.config","<GCC$Dir>.config"},
{"./././c.cool","@.c/cool"},
{"../../../c.cool","^.^.^.c/cool"},
{"../cool.c","^.c.cool"},
{"././/./c.cool","@.c/cool"},
{"../../../c.cool","^.^.^.c/cool"},
{"../cool.c","^.c.cool"},
{"../cool.php","^.cool/php"},
{".plan","/plan"},
{"..cunningplan","//cunningplan"},
{"/idefs::4/$/fred/preset/s","idefs::4.$.fred.preset.s"},
{"/idefs::4/$/fred/preset.s","idefs::4.$.fred.s.preset"},
{"/Sunfish#192.0.2.50::/home/joty.$/projects/gccsdk/index.html","Sunfish#192.0.2.50::/home/joty.$.projects.gccsdk.index/html"},
{"/HostFS:","HostFS:"},
{"/HostFS:$","HostFS:$"},
{"/HostFS:$.jo.html","HostFS:$.jo/html"},
{"/HostFS:$/jo.html","HostFS:$.jo/html"},
{"/rname.c","$.c.rname"},
{"/arm/rname.c","$.arm.c.rname"},
{"/arm//rname.c","$.arm.c.rname"},
{"//arm/rname.c","$.arm.c.rname"},
{"/arm/./rname.c","$.arm.c.rname"},
{"/<GCC$Dir>/config","<GCC$Dir>.config"},
{"/dev/tty","tty:"},
{"idefs::4.$.fred","idefs::4.$.fred"},
{"idefs::4/$/fred","idefs::4.$.fred"},
{"fred.c","c.fred"},
{"foo/fred.c","foo.c.fred"},
{"foo/fred.php","foo.fred/php"},
{"c/fred","c.fred"},
{"c.fred","c.fred"},
{"/gcc:/libgcc.o","gcc:o.libgcc"},
{"/gcc:libgcc.o","gcc:o.libgcc"},
{"gcc:/getopt.c","gcc:c.getopt"},
{"gcc:/o.libgcc","gcc:o/libgcc"},
{"gcc:c.getopt","gcc:c.getopt"},
{"gcc:c/getopt","gcc:c.getopt"},
{"gcc:getopt.c","gcc:c.getopt"},
{"gcc:foo/getopt.c","gcc:foo.c.getopt"},
{"gcc:foo.getopt/php","gcc:foo.getopt/php"},
{"gcc:getopt.php","gcc:getopt.php"},
{"gcc:getopt/php","gcc:getopt/php"},
{"gcc:foo/getopt","gcc:foo/getopt"},
{"foo.bar.o.getopt","foo.bar.o.getopt"},
{"foo/bar/o/getopt","foo.bar.o.getopt"},
{"/gccpkg:lib/gcc-lib/arm-riscos-aof/2_95_4/include/time.h","gccpkg:lib.gcc-lib.arm-riscos-aof.2_95_4.include.h.time"},
{"/gccpkg:include/unixlib/stddef.h","gccpkg:include.unixlib.h.stddef"},
{"idefs::4.$.fred.s.preset","idefs::4.$.fred.s.preset"},
{"idefs::4.$.fred.preset.s","idefs::4.$.fred.preset.s"},
{"idefs::4/$/fred/preset.s","idefs::4.$.fred.s.preset"},
{"Devices#baud9600:serial","Devices#baud9600:serial"},
{"LanMan98#notypes::mint.$.index/php","LanMan98#notypes::mint.$.index/php"},
{"LanMan98#no.types::mint/$/index.php","LanMan98#no.types::mint.$.index/php"},
{"/LanMan98#no/types::mint/$/index.php","LanMan98#no/types::mint.$.index/php"},
{"c/jimmyhill","c.jimmyhill"},
{"fred/preset.s","fred.s.preset"},
{"getopt.c","c.getopt"},
{"cc.smart","cc.smart"},
{"php.smart","php/smart"},
{"smart.cc","cc.smart"},
{"gcc-2.7.2.2.tar.gz","gcc-2/7/2/2/tar/gz"},
{"foo/fred.c.text-base","foo.fred/c/text-base"},
{"foo.o.bar","foo.o.bar"},
{":4.$.something",":4.$.something"},
{".","@"},
{"..","^"},
{"gcc:/./o.libgcc","gcc:o/libgcc"},
{"gcc:/../o.libgcc","gcc:^.o/libgcc"},
{"gcc:/../libgcc.o","gcc:^.o.libgcc"},
{"gcc:^.o.libgcc","gcc:^.o.libgcc"},
{"a","a"},
{"abba","abba"},
{"Makefile,fe1","Makefile"},
{"./.","@"},
{"../.","^"},
{"./..","@.^"},
{"./foo/..","@.foo.^"},
{"foo/..","@"},
{"foo/.","foo"},
{"./foo/.","@.foo"},
{"./foo.c","@.c.foo"},
{"foo/","foo"},
{"foo//","foo"},
{"dir/file.h/","dir.file/h"},
/* This is not yet ported to GCCSDK 4.  */
#ifndef __ELF__
{"/home/riscos/env/foo","foo:"},
{"/home/riscos/env/","$.home.riscos.env"},
{"/home/riscos/env/foo/bar/baz/","foo:bar.baz"},
{"/../home/./fred/../riscos////env/./Choices/Boot","Choices:Boot"},
#endif
{"/usr/../var/../tmp/./file.c","<Wimp$ScrapDir>.c.file"},
{"~/foo",""},
{"/a/long/path/name/that/is/going/to/end/in/a.","$.a.long.path.name.that.is.going.to.end.in.a/"},
{"/a/long/path/name/that/is/not/going/to/end/in/a/.","$.a.long.path.name.that.is.not.going.to.end.in.a"},
{"/a/long/path/name/that/has/aaaa./in/the/middle","$.a.long.path.name.that.has.aaaa/.in.the.middle"},
{"/","$"},
{"/.","$"},
{"/..","$"},
{"/../","$"},
{"/.a","$./a"},
{"/..a","$.//a"},
{"/.a.","$./a/"},
{"/.a..","$./a//"},
{"/a/b/c/d/e..","$.a.b.c.d.e//"},
{"/a/b/c/d/e.","$.a.b.c.d.e/"},
{"/ADFS::4.$/foo/h/info.php","ADFS::4.$.foo.h.info/php"},
{"","@"}};

int main (void)
{
	char buffer[256];
	char buffer2[256];
	int i;
	int j;
	int fail = 0;
	int filetype = 0;
	int failed = 0, passed = 0, untested = 0;

	i = -1;
	do {
		i++;
		printf("Testing %s\n",tests[i][0]);
		__riscosify(tests[i][0], 0 ,0 | __RISCOSIFY_FILETYPE_EXT | __RISCOSIFY_DONT_CHECK_DIR, buffer, 256, &filetype);

		if (tests[i][1][0])
		  fail = strcmp(buffer,tests[i][1]);
		else
		  fail = 0;
		printf("riscosified: %s\n",buffer);
		if (!tests[i][1][0]) untested++; else if (fail) failed++; else passed++;
		for (j=0;j<60;j++) printf(" ");
		printf("%s\n", tests[i][1][0] ? (fail ? "riscosify FAIL" : "riscosify pass") : "riscosify no test");
		__unixify(buffer,0 | __RISCOSIFY_FILETYPE_EXT | __RISCOSIFY_DONT_CHECK_DIR, buffer2, 256, filetype);

		printf("unixified back again %s : %s\n\n\n",buffer2,strcmp(tests[i][0],buffer2) ? "DIFFERS" : "same");
	} while (tests[i][0][0]);

	printf("SUMMARY riscosify: %d passed, %d failed, %d without an expected value\n", passed, failed, untested);
	return failed ? 2 : 0;
}



