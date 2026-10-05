/* Direct interface calls to RISC OS.
   Copyright (c) 2000-2019 UnixLib Developers.  */

#ifndef __INTERNAL_OS_H
#define __INTERNAL_OS_H

#ifndef __UNIXLIB_FEATURES_H
# include <features.h>
#endif

#ifndef __KERNEL_H
# include <kernel.h>
#endif

#ifndef __SWIS_H
# include <swis.h>
#endif

#ifndef __UNIXLIB_TYPES_H
# include <unixlib/types.h>
#endif

__BEGIN_DECLS

/* Setup console I/O.  */
extern int __os_console (void) __THROW;

/* Setup RS423 I/O.  */
extern int __os_423 (void) __THROW;

extern int __os_vdu (int) __THROW;
extern int __os_get (void) __THROW;

/* Returns -1 on timeout.  */
extern int __os_inkey (int) __THROW;

/* Flush keyboard buffer.  */
extern int __os_keyflush (void) __THROW;

/* 423 times are in centiseconds */
extern int __os_423vdu (int) __THROW;
extern int __os_423get (void) __THROW;

/* Returns -1 on timeout.  */
extern int __os_423inkey (int) __THROW;

/* Flush RS423 buffer.  */
extern int __os_423flush (void) __THROW;
extern int __os_423break (int) __THROW;

extern _kernel_oserror *__os_swi (int swinum, int *regs /* 10 reg */ )
     __THROW __nonnull ((2));

extern _kernel_oserror *__os_word (int, void *)
     __THROW __nonnull ((2)) __wur;

extern _kernel_oserror *__os_prhex (int); 	/* %8x format hex output */
extern _kernel_oserror *__os_prdec (int);

extern _kernel_oserror *__os_print (const char *) __nonnull ((1));
extern _kernel_oserror *__os_write (const char *, int) __nonnull ((1));

extern _kernel_oserror *__os_nl (void);		/* newline */

/* The inline SWI wrappers.

   GCC guarantees a local register variable (register T x __asm ("r1")) to be
   in its register only where it is an operand of an asm statement; anywhere
   else the variable IS the hard register, and a call between the asm and the
   use of the variable has overwritten it.  So the registers of a SWI are
   passed in as operands, and the results are copied out INSIDE the asm
   (MOV %[out], rN at the end of the template, early clobber) into ordinary
   variables.  A register that the SWI changes and that is not an operand is
   a clobber; an input whose register the SWI changes is an in-out operand.
   A result that is read in C after the asm is not safe: GCC 16 at -O2 made
   __unixinit use the result of strlen () for the size of the DDEUtils
   command line.  */

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Byte (int __a, int __xin, int __yin, int *__xoutp, int *__youtp)
{
  register int a __asm ("r0") = __a;
  register int xin __asm ("r1") = __xin;
  register int yin __asm ("r2") = __yin;
  const _kernel_oserror * err;
  int xout;
  int yout;
  __asm__ volatile ("SWI\t%[SWI_XOS_Byte]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[xout], r1\n\t"
		    "MOV\t%[yout], r2\n\t"
		    : [err] "=&r" (err),
		      [xout] "=&r" (xout),
		      [yout] "=&r" (yout),
		      "+r" (a),
		      "+r" (xin),
		      "+r" (yin)
		    : [SWI_XOS_Byte] "i" (OS_Byte | (1<<17))
		    : "r14", "cc");
  if (__xoutp && !err)
    *__xoutp = xout;
  if (__youtp && !err)
    *__youtp = yout;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_CLI (const char *__cmd)
{
  register const char *cmd __asm ("r0") = __cmd;
  const _kernel_oserror * err;
  __asm__ volatile ("SWI\t%[SWI_XOS_CLI]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err),
		      "+r" (cmd)
		    : [SWI_XOS_CLI] "i" (OS_CLI | (1<<17))
		    : "r14", "cc");

  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Find_Open (unsigned __reason, const char *__filename, unsigned *__fhandle)
{
  register unsigned reason __asm ("r0") = __reason;
  register const char *filename __asm ("r1") = __filename;
  const _kernel_oserror * err;
  unsigned fhandle;
  __asm__ volatile ("SWI\t%[SWI_XOS_Find]\n\t"
		    "MOV\tr1, r0\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[fhandle], r1\n\t"
		    : [err] "=&r" (err),
		      [fhandle] "=&r" (fhandle),
		      "+r" (reason),
		      "+r" (filename)
		    : [SWI_XOS_Find] "i" (OS_Find | (1<<17))
		    : "r14", "cc");
  *__fhandle = !err ? fhandle : 0;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Find_Close (unsigned __fhandle)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #0\n\t"
		    "SWI\t%[SWI_XOS_Find]\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (fhandle),
		      [SWI_XOS_Find] "i" (OS_Find | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_SaveBlockLoadExec (const char *__filename, unsigned __loadaddr,
			       unsigned __execaddr, unsigned __start,
			       unsigned __end)
{
  register const char *filename __asm ("r1") = __filename;
  register unsigned loadaddr __asm ("r2") = __loadaddr;
  register unsigned execaddr __asm ("r3") = __execaddr;
  register unsigned start __asm ("r4") = __start;
  register unsigned end __asm ("r5") = __end;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #0\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (filename),
		      "r" (loadaddr),
		      "r" (execaddr),
		      "r" (start),
		      "r" (end),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r14", "cc");

  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_WriteCatLoadExecAttr (const char *__filename, unsigned __loadaddr,
				  unsigned __execaddr, unsigned __attr)
{
  register const char *filename __asm ("r1") = __filename;
  register unsigned loadaddr __asm ("r2") = __loadaddr;
  register unsigned execaddr __asm ("r3") = __execaddr;
  register unsigned attr __asm ("r5") = __attr;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #1\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (filename),
		      "r" (loadaddr),
		      "r" (execaddr),
		      "r" (attr),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_WriteCatInfoAttr (const char *__filename, unsigned __attr)
{
  register const char *filename __asm ("r1") = __filename;
  register unsigned attr __asm ("r5") = __attr;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #4\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (filename),
		      "r" (attr),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_DeleteObject (const char *__objectname)
{
  register const char *objectname __asm ("r1") = __objectname;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #6\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (objectname),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r2", "r3", "r4", "r5", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_CreateDirectory (const char *__dirname)
{
  register const char *dirname __asm ("r1") = __dirname;
  register unsigned numentries __asm ("r4") = 0;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #8\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (dirname),
		      "r" (numentries),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_CreateFileWithFileType (const char *__filename,
				    unsigned __filetype, unsigned __length)
{
  register const char *filename __asm ("r1") = __filename;
  register unsigned filetype __asm ("r2") = __filetype;
  register unsigned start __asm ("r4") = 0;
  register unsigned end __asm ("r5") = __length;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #11\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (filename),
		      "r" (filetype),
		      "r" (start),
		      "r" (end),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_ReadCatInfo (const char *__filename, unsigned *__objtype,
			 unsigned *__loadaddr, unsigned *__execaddr,
			 unsigned *__objlen, unsigned *__attr)
{
  register const char *filename __asm ("r1") = __filename;
  const _kernel_oserror * err;
  unsigned objtype;
  unsigned loadaddr;
  unsigned execaddr;
  unsigned objlen;
  unsigned attr;
  __asm__ volatile ("MOV\tr0, #17\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOV\tr1, r0\n\t"
                    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[objtype], r1\n\t"
		    "MOV\t%[loadaddr], r2\n\t"
		    "MOV\t%[execaddr], r3\n\t"
		    "MOV\t%[objlen], r4\n\t"
		    "MOV\t%[attr], r5\n\t"
		    : [err] "=&r" (err),
		      [objtype] "=&r" (objtype),
		      [loadaddr] "=&r" (loadaddr),
		      [execaddr] "=&r" (execaddr),
		      [objlen] "=&r" (objlen),
		      [attr] "=&r" (attr),
		      "+r" (filename)
		    : [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r5", "r4", "r3", "r2", "r0", "r14", "cc");
  if (!err)
    {
      if (__objtype)
#ifdef __TARGET_SCL__
	*__objtype = objtype;
#else
	*__objtype = objtype == 3 ? (__get_feature_imagefs_is_file () ? 1 : 2) : objtype;
#endif
      if (__loadaddr)
	*__loadaddr = loadaddr;
      if (__execaddr)
	*__execaddr = execaddr;
      if (__objlen)
	*__objlen = objlen;
      if (__attr)
	*__attr = attr;
    }
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_File_WriteCatInfoFileType (const char *__filename, unsigned __filetype)
{
  register const char *filename __asm ("r1") = __filename;
  register unsigned filetype __asm ("r2") = __filetype;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #18\n\t"
		    "SWI\t%[SWI_XOS_File]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (filename),
		      "r" (filetype),
		      [SWI_XOS_File] "i" (OS_File | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_GBPB_ReadBytes (unsigned __fhandle, void *__buf, unsigned __to_read,
		       unsigned *__not_read)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  register void *buf __asm ("r2") = __buf;
  register unsigned to_read __asm ("r3") = __to_read;
  const _kernel_oserror * err;
  unsigned not_read;
  __asm__ volatile ("MOV\tr0, #4\n\t"
		    "SWI\t%[SWI_XOS_GBPB]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[not_read], r3\n\t"
		    : [err] "=&r" (err),
		      [not_read] "=&r" (not_read),
		      "+r" (buf),
		      "+r" (to_read)
		    : "r" (fhandle),
		      [SWI_XOS_GBPB] "i" (OS_GBPB | (1<<17))
		    : "r0", "r4", "r14", "cc", "memory");
  if (__not_read && !err)
    *__not_read = not_read;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_GBPB_WriteBytes (unsigned __fhandle, const void *__buf, unsigned __to_write,
			unsigned *__not_written)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  register const void *buf __asm ("r2") = __buf;
  register unsigned to_write __asm ("r3") = __to_write;
  const _kernel_oserror * err;
  unsigned not_written;
  __asm__ volatile ("MOV\tr0, #2\n\t"
		    "SWI\t%[SWI_XOS_GBPB]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[not_written], r3\n\t"
		    : [err] "=&r" (err),
		      [not_written] "=&r" (not_written),
		      "+r" (buf),
		      "+r" (to_write)
		    : "r" (fhandle),
		      [SWI_XOS_GBPB] "i" (OS_GBPB | (1<<17))
		    : "r0", "r4", "r14", "cc");
  if (__not_written && !err)
    *__not_written = not_written;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_ChangeRedirection (int __fh_in, int __fh_out,
			  int *__prev_fh_in, int *__prev_fh_out)
{
  register int fh_in __asm ("r0") = __fh_in;
  register int fh_out __asm ("r1") = __fh_out;
  int prev_fh_in;
  int prev_fh_out;
  register const _kernel_oserror *err;
  __asm__ volatile ("SWI\t%[SWI_XOS_ChangeRedirection]\n\t"
		    "MOVVS\t%[err], r0\n\t"
		    "MOVVC\t%[err], #0\n\t"
		    "MOV\t%[prev_fh_in], r0\n\t"
		    "MOV\t%[prev_fh_out], r1\n\t"
		    : [prev_fh_in] "=&r" (prev_fh_in),
		      [prev_fh_out] "=&r" (prev_fh_out),
		      [err] "=&r" (err),
		      "+r" (fh_in),
		      "+r" (fh_out)
		    : [SWI_XOS_ChangeRedirection] "i" (OS_ChangeRedirection | (1<<17))
		    : "r14", "cc");
  if (__prev_fh_in && !err)
    *__prev_fh_in = prev_fh_in;
  if (__prev_fh_out && !err)
    *__prev_fh_out = prev_fh_out;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_DDEUtils_SetCLSize (size_t __len)
{
  register size_t len __asm ("r0") = __len;
  const _kernel_oserror * err;
  __asm__ volatile ("SWI\t%[SWI_XDDEUtils_SetCLSize]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err),
		      "+r" (len)
		    : [SWI_XDDEUtils_SetCLSize] "i" (DDEUtils_SetCLSize | (1<<17))
		    : "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_DDEUtils_SetCL (const char *__cli)
{
  register const char *cli __asm ("r0") = __cli;
  const _kernel_oserror * err;
  __asm__ volatile ("SWI\t%[SWI_XDDEUtils_SetCL]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err),
		      "+r" (cli)
		    : [SWI_XDDEUtils_SetCL] "i" (DDEUtils_SetCL | (1<<17))
		    : "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_DDEUtils_GetCLSize (size_t *__len)
{
  size_t len;
  register const _kernel_oserror *err;
  __asm__ volatile ("SWI\t%[SWI_XDDEUtils_GetCLSize]\n\t"
		    "MOVVS\t%[err], r0\n\t"
		    "MOVVC\t%[err], #0\n\t"
		    "MOV\t%[len], r0\n\t"
		    : [len] "=&r" (len),
		      [err] "=&r" (err)
		    : [SWI_XDDEUtils_GetCLSize] "i" (DDEUtils_GetCLSize | (1<<17))
		    : "r0", "r14", "cc");
  if (__len && !err)
    *__len = len;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_DDEUtils_GetCL (char *__cli)
{
  register char *cli __asm ("r0") = __cli;
  const _kernel_oserror * err;
  __asm__ volatile ("SWI\t%[SWI_XDDEUtils_GetCL]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err),
		      "+r" (cli)
		    : [SWI_XDDEUtils_GetCL] "i" (DDEUtils_GetCl | (1<<17))
		    : "r14", "cc", "memory");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_DDEUtils_Prefix (const char *__dir)
{
  register const char *dir __asm ("r0") = __dir;
  const _kernel_oserror * err;
  __asm__ volatile ("SWI\t%[SWI_XDDEUtils_Prefix]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err),
		      "+r" (dir)
		    : [SWI_XDDEUtils_Prefix] "i" (DDEUtils_Prefix | (1<<17))
		    : "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_DDEUtils_ReadPrefix (int __ao, const char **__prefix)
{
  register int ao __asm ("r0") = __ao;
  const char * prefix;
  register const _kernel_oserror *err;
  __asm__ volatile ("SWI\t%[SWI_XDDEUtils_ReadPrefix]\n\t"
		    "MOVVS\t%[err], r0\n\t"
		    "MOVVC\t%[err], #0\n\t"
		    "MOV\t%[prefix], r0\n\t"
		    : [prefix] "=&r" (prefix),
		      [err] "=&r" (err),
		      "+r" (ao)
		    : [SWI_XDDEUtils_ReadPrefix] "i" (DDEUtils_ReadPrefix | (1<<17))
		    : "r14", "cc");
  if (__prefix && !err)
    *__prefix = prefix;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_FSControl_Rename (const char *__oldfn, const char *__newfn)
{
  register const char *oldfn __asm ("r1") = __oldfn;
  register const char *newfn __asm ("r2") = __newfn;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #25\n\t"
		    "SWI\t%[SWI_XOS_FSControl]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (oldfn),
		      "r" (newfn),
		      [SWI_XOS_FSControl] "i" (OS_FSControl | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_FSControl_SetCurDir (const char *__dirname)
{
  register const char *dirname __asm ("r1") = __dirname;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #0\n\t"
		    "SWI\t%[SWI_XOS_FSControl]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (dirname),
		      [SWI_XOS_FSControl] "i" (OS_FSControl | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

/* Canonicalise pathname.
   PATHNAME will be canonicalised and returned in BUF (being BUFSIZE bytes
   deep).  When BUFSIZE is too small, REQ_XTRABUFSIZE will be non-zero,
   otherwise it will be zero.
   When PATHVAR is non-NULL, it will be used for canonicalisation.  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_FSControl_Canonicalise (const char *__pathname, const char *__pathvar,
			       char *__buf, size_t __bufsize,
			       size_t *__req_xtrabufsize)
{
  register const char *pathname __asm ("r1") = __pathname;
  register char *buf __asm ("r2") = __buf;
  register const char *pathvar __asm ("r3") = __pathvar;
  register size_t bufsize __asm ("r5") = __bufsize;
  const _kernel_oserror * err;
  int xtrabufsize;
  __asm__ volatile ("MOV\tr0, #37\n\t"
		    "MOV\tr4, #0\n\t"
		    "SWI\t%[SWI_XOS_FSControl]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[xtrabufsize], r5\n\t"
		    : [err] "=&r" (err),
		      [xtrabufsize] "=&r" (xtrabufsize),
		      "+r" (bufsize)
		    : "r" (pathname),
		      "r" (buf),
		      "r" (pathvar),
		      [SWI_XOS_FSControl] "i" (OS_FSControl | (1<<17))
		    : "r0", "r4", "r14", "cc", "memory");
  if (__req_xtrabufsize && !err)
    *__req_xtrabufsize = xtrabufsize < 1 ? 1 - xtrabufsize : 0;
  return err;
}

/* Read filehandle's sequential file pointer.  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_GetFilePtr (unsigned __fhandle, __off_t *__fileptr)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  const _kernel_oserror * err;
  int fileptr;
  __asm__ volatile ("MOV\tr0, #0\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[fileptr], r2\n\t"
		    : [err] "=&r" (err),
		      [fileptr] "=&r" (fileptr)
		    : "r" (fhandle),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r2", "r0", "r14", "cc");
  if (__fileptr && !err)
    *__fileptr = fileptr;
  return err;
}

/* Write filehandle's sequential file pointer.  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_SetFilePtr (unsigned __fhandle, __off_t __fileptr)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  register int fileptr __asm ("r2") = __fileptr;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #1\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (fhandle),
		      "r" (fileptr),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}  

/* Read filehandle's extent.  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_GetExtent (unsigned __fhandle, __off_t *__extent)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  const _kernel_oserror * err;
  int extent;
  __asm__ volatile ("MOV\tr0, #2\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[extent], r2\n\t"
		    : [err] "=&r" (err),
		      [extent] "=&r" (extent)
		    : "r" (fhandle),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r2", "r0", "r14", "cc");
  if (__extent && !err)
    *__extent = extent;
  return err;
}

/* Write filehandle's extent.  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_SetExtent (unsigned __fhandle, __off_t __extent)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  register int extent __asm ("r2") = __extent;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #3\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (fhandle),
		      "r" (extent),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}  

/* Canonicalise file handle.  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_Canonicalise (unsigned __fhandle, char *__buf, size_t __bufsize,
			  size_t *__req_xtrabufsize)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  register char *buf __asm ("r2") = __buf;
  register size_t bufsize __asm ("r5") = __bufsize;
  const _kernel_oserror * err;
  int xtrabufsize;
  __asm__ volatile ("MOV\tr0, #7\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[xtrabufsize], r5\n\t"
		    : [err] "=&r" (err),
		      [xtrabufsize] "=&r" (xtrabufsize),
		      "+r" (bufsize)
		    : "r" (fhandle),
		      "r" (buf),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r0", "r14", "cc", "memory");
  if (__req_xtrabufsize && !err)
    *__req_xtrabufsize = xtrabufsize < 1 ? 1 - xtrabufsize : 0;
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_GetFileHandleStatus (unsigned __fhandle, int *__status)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  const _kernel_oserror * err;
  int status;
  __asm__ volatile ("MOV\tr0, #254\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOV\tr2, r0\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    "MOV\t%[status], r2\n\t"
		    : [err] "=&r" (err),
		      [status] "=&r" (status)
		    : "r" (fhandle),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r2", "r0", "r14", "cc", "memory");
  if (__status && !err)
    *__status = status;
  return err;
}

/* Flush filehandle(s).  */
static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_OS_Args_Flush (unsigned __fhandle)
{
  register unsigned fhandle __asm ("r1") = __fhandle;
  const _kernel_oserror * err;
  __asm__ volatile ("MOV\tr0, #255\n\t"
		    "SWI\t%[SWI_XOS_Args]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err)
		    : "r" (fhandle),
		      [SWI_XOS_Args] "i" (OS_Args | (1<<17))
		    : "r0", "r14", "cc");
  return err;
}

static __inline__ const _kernel_oserror * __attribute__ ((always_inline))
SWI_Territory_ConvertTimeToOrdinals (int __territory,
				     const unsigned int __ro_time[],
				     unsigned int __ordinals[])
{
  register int territory __asm ("r0") = __territory;
  register const unsigned int *ro_time __asm ("r1") = __ro_time;
  register unsigned int *ordinals __asm ("r2") = __ordinals;
  _kernel_oserror * err;
  __asm__ volatile ("SWI\t%[SWI_Territory_ConvertTimeToOrdinals]\n\t"
		    "MOVVC\tr0, #0\n\t"
		    "MOV\t%[err], r0\n\t"
		    : [err] "=&r" (err),
		      "+r" (territory)
		    : "r" (ro_time),
		      "r" (ordinals),
		      [SWI_Territory_ConvertTimeToOrdinals] "i" (Territory_ConvertTimeToOrdinals | (1<<17))
		    : "r14", "cc", "memory");
  return err;
}

/* Technically not part of the OS, but...  */
#ifdef PIC
static __inline__ void som_deregister_client (void)
{
  __asm__ volatile ("SWI\t%[SWI_SOM_DeregisterClient]\n"
		    : /* No output */
		    : [SWI_SOM_DeregisterClient] "i" (SOM_DeregisterClient | (1<<17)));
}
#endif

#ifdef __ARM_EABI__
static __inline__ void armeabisupport_cleanup (void)
{
  __asm__ volatile ("SWI\t%[SWI_ARMEABISupport_Cleanup]\n"
		    : /* No output */
		    : [SWI_ARMEABISupport_Cleanup] "i" (ARMEABISupport_Cleanup | (1<<17)));
}
#endif

/* Flag RISC OS error to UNIX and take a copy of the RISC OS error block.
   If 'err' is NULL, no action is taken.
   If 'new_errno' is non-zero, then UnixLib's errno is also set to 'new_errno'.
   This function returns -1 if there is an error to allow to function
   to be used as a return value in error situations */
extern int __ul_seterr (const _kernel_oserror *__err, int __new_errno)
     __THROW;

__END_DECLS

#endif
