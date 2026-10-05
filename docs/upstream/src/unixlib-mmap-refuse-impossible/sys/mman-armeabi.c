/* Map or unmap memory.
   Copyright (c) 1996-2021 UnixLib Developers.  */

/* Definitions for BSD-style memory management.  */
/* sys/mman.c: Written by Peter Burwood, 1 November 1996, June 1997  */
/* sys/mman-armeabi.c: Adapted from the above by Lee Noar, March 2021  */

#include <sys/mman.h>
#include <sys/types.h>
#include <swis.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <internal/os.h>
#include <internal/unix.h>
#include <internal/swiparams.h>
#include <internal/fd.h>
#include <internal/local.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>

#define page_align(gbl, len) \
  (((len) + (gbl)->pagesize - 1) & ~((gbl)->pagesize - 1))

extern void __mmap_page_copy (void * dst, void * src, int len);

static char coredump_dir[256];

void
__init_coredump (const char *dir)
{
  size_t dirlen = strlen (dir);

  /* Allow at least 16 bytes for the coredump filenames inside user supplied
     directory.  */
  if (dirlen < sizeof (coredump_dir)-16)
    strcpy (coredump_dir, dir);
}

const _kernel_oserror *
__unixlib_write_coredump (const char *dir)
{
  if (dir != NULL)
    __init_coredump (dir);

  size_t dirlen = strlen (coredump_dir);
  if (dirlen == 0)
    return NULL;

  /* Remove ending dot if there is one.  */
  if (coredump_dir[dirlen - 1] == '.')
    coredump_dir[--dirlen] = '\0';

  /* Create the coredump directory, with default number of entries.  */
  const _kernel_oserror *roerr;
  if ((roerr = SWI_OS_File_CreateDirectory (coredump_dir)) != NULL)
    return roerr;

  /* Write application space data.  */
  strcpy (coredump_dir + dirlen, ".app");
  const struct ul_memory *mem = &__ul_memory;
  roerr = SWI_OS_File_SaveBlockLoadExec (coredump_dir, 0x8000 /* load */,
					 mem->rwlomem /* exec */, 0x8000 /* start */,
					 mem->appspace_limit /* end */);

  /* Write main DA block (if there is one).  */
  if (__ul_global.dynamic_num != -1)
    {
      strcpy (coredump_dir + dirlen, ".da_main");
      if (roerr == NULL)
	roerr = SWI_OS_File_SaveBlockLoadExec (coredump_dir, mem->dalomem /* load */,
					       mem->dabreak /* exec */, mem->dalomem /* start */,
					       mem->dalimit /* end */);
    }

  /* Not really practical to dump all mmap allocations, there could be hundreds.  */
  coredump_dir[dirlen] = '\0';
  return roerr;
}


/* Free all mmapped memory.  This is called from __dynamic_area_exit.
   At that point, the thread system is not running.  */
void
__munmap_all (void)
{
  /* NOOP. This is handled by ARMEABISupport.  */
}

/* A request that can never be served is refused here, before ARMEABISupport makes a dynamic area for it.  ARMEABISupport makes an "mmap#N" area
   first and only then finds out that it cannot serve the request: the answer is ENOMEM, but the area stays, with the pages that were claimed for it, until the next reboot.
   malloc (2GB - 1), which is what new char[SIZE_MAX / 2] asks for, makes two such requests (the direct mmap, then the "mmap as morecore" retry) and so leaves two 100MB areas
   behind; a request a little over the OS clamp on the size of one dynamic area (OS_DynamicArea 8: 128MB on a machine that has it set) pins all the memory it claimed.
   So: half of the 32-bit address space or more can never be served, and nor can more than the clamp (the one for areas that give their maximum size, as the mmap areas do).
   Such a request failed anyway, with the same errno; now it leaves nothing behind.  */
#define MMAP_ABSURD_SIZE	0x80000000u		/* 2GB */
#define MMAP_CLAMP_CHECK_SIZE	(16u * 1024 * 1024)	/* smaller requests are not worth a SWI */

static int
mmap_too_big (size_t len)
{
  unsigned int clamp;

  if (len >= MMAP_ABSURD_SIZE)
    return 1;
  if (len < MMAP_CLAMP_CHECK_SIZE)
    return 0;

  /* OS_DynamicArea 8 with R1 = R2 = 0 only reads the clamps; R2 is the one for areas
     that give a maximum size (0 or -1: no clamp).  An OS without it has no clamp.  */
  if (_swix (OS_DynamicArea, _INR(0,2)|_OUT(2), 8, 0, 0, &clamp) != NULL)
    return 0;
  return clamp != 0 && clamp != (unsigned int) -1 && len > clamp;
}

/* Map addresses starting near ADDR and extending for LEN bytes.  from
   OFFSET into the file FD describes according to PROT and FLAGS.  If ADDR
   is nonzero, it is the desired mapping address.  If the MAP_FIXED bit is
   set in FLAGS, the mapping will be at ADDR exactly (which must be
   page-aligned); otherwise the system chooses a convenient nearby address.
   The return value is the actual mapping address chosen or (void *) -1
   for errors (in which case `errno' is set).  A successful `mmap' call
   deallocates any previous mapping for the affected region.  */
void *
mmap (void * addr, size_t len, int prot, int flags, int fd, off_t offset)
{
  struct ul_global *gbl = &__ul_global;

  PTHREAD_UNSAFE

  /* We only support MAP_FIXED for an existing block.  */

  /* We don't support MAP_COPY.  */
  if (flags & MAP_COPY)
    return (void *) __set_errno (ENOSYS);

  if (mmap_too_big (len))
    return (void *) __set_errno (ENOMEM);

  /* We don't support MAP_PRIVATE. However, we will just use MAP_SHARED instead
     rather than changing all the user code calling us.  */

  void *result;
  _kernel_oserror *err;
  err = _swix (ARMEABISupport_MMapOp, _INR(0,6)|_OUT(0), ARMEABISUPPORT_MMAPOP_MAP,
							 addr,
							 len,
							 prot,
							 flags,
							 fd,
							 offset,
							 &result);
  if (err || !result)
    return (void *) __set_errno (ENOMEM);

  size_t mapped_size = page_align (gbl, len);

  if (flags & MAP_ANON)
    memset (result, 0, mapped_size);

  if (fd == -1 || IS_SHM_FD(fd))
    return result;

  /* Read in the relevant section of the file */
  /* FIXME: Take into account file/memory protections.  */
  int count = 0;

  off_t oldpos = lseek (fd, 0, SEEK_CUR);

  lseek (fd, offset, SEEK_SET);

  while (count < mapped_size)
    {
      ssize_t size = read (fd, result + count, mapped_size - count);

      if (size < 0)
	{
	  int save_errno = errno;
	  munmap (result, mapped_size);
	  lseek (fd, oldpos, SEEK_SET);
	  return (void *) __set_errno (save_errno);
	}
      else if (size == 0)
	break;

      count += size;
    }
  lseek (fd, oldpos, SEEK_SET);

  /* Ref: http://linux.die.net/man/2/mmap
   * "For a file that is not a multiple of the page size, the remaining memory
   * is zeroed when mapped, and writes to that region are not written out to
   * the file."
   */
  if (mapped_size - len > 0)
    memset (result + len, 0, mapped_size - len);

  return result;
}

/* Deallocate any mapping for the region starting at ADDR and extending LEN
   bytes.  Returns 0 if successful, -1 for errors (and sets errno).  */
int
munmap (void * addr, size_t len)
{
  PTHREAD_UNSAFE

  _kernel_oserror *err;
  void *mmap_addr;
  size_t size;
  off_t offset;
  int prot, flags, fd;

  err = _swix(ARMEABISupport_MMapOp, _INR(0,1)|_OUTR(0,5),
				     ARMEABISUPPORT_MMAPOP_GET_INFO,
				     addr,
				     &mmap_addr,
				     &size,
				     &prot,
				     &flags,
				     &fd,
				     &offset);
  /* We get an error if a specific block wasn't found, however, for a memory
   * mapping (rather than a file mapping), that isn't an issue.  */
  if (!err)
    {
      /* The start of a previously allocated block was passed in, is it a file
       * mapping?  */
      if ((fd != -1 && !IS_SHM_FD(fd)) &&
	    !BADF(fd) && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE))
        {
	  /* Write back file contents if necessary
	     All the docs I've seen suggest that we only need to write
	     stuff back on msync(), but some software (e.g. mkimage)
	     seems to rely on the writeback occuring un munmap().  */
          /* Assume that the user only wants 'len' bytes writing.  */
	  off_t oldpos = lseek (fd, 0, SEEK_CUR);
	  lseek (fd, offset, SEEK_SET);
	  write (fd, addr, len);
	  lseek (fd, oldpos, SEEK_SET);
	}
    }

  err = _swix(ARMEABISupport_MMapOp, _INR(0,2), ARMEABISUPPORT_MMAPOP_UNMAP,
						addr,
						len);

  return err ? __ul_seterr (err, __errno_from_armeabisupport(err->errnum)) : 0;
}

/* Change the memory protection of the region starting at ADDR and
   extending LEN bytes to PROT.  Returns 0 if successful, -1 for errors
   (and sets errno).  */
int
mprotect (void * addr __attribute__ ((__unused__)),
	  size_t len __attribute__ ((__unused__)),
	  int prot __attribute__ ((__unused__)))
{
  struct ul_global *gbl = &__ul_global;

  if (!addr || (size_t)addr & (gbl->pagesize - 1))
    return EINVAL;

  _kernel_oserror *err;
  err = _swix(ARMEABISupport_MMapOp, _INR(0,3), ARMEABISUPPORT_MMAPOP_PROTECT,
						addr,
						len,
						prot);

  return err ? __ul_seterr (err, __errno_from_armeabisupport(err->errnum)) : 0;
}

/* this is supposed to be able to work on non-mmaped memory too. */

/* Synchronize the region starting at ADDR and extending LEN bytes with the
   file it maps.  Filesystem operations on a file being mapped are
   unpredictable before this is done.  */
int
msync (void * addr, size_t len, int syncflags __attribute__ ((__unused__)))
{
  const struct ul_global *gbl = &__ul_global;

  PTHREAD_UNSAFE

  _kernel_oserror *err;
  void *mmap_addr;
  size_t size;
  off_t offset;
  int prot, flags, fd;

  if ((size_t)addr & (gbl->pagesize - 1))
    return __set_errno (EINVAL);

  err = _swix(ARMEABISupport_MMapOp, _INR(0,1)|_OUTR(0,5),
				     ARMEABISUPPORT_MMAPOP_GET_INFO,
				     addr,
				     &mmap_addr,
				     &size,
				     &prot,
				     &flags,
				     &fd,
				     &offset);
  /* If we couldn't find the mmap mapping, then return an error.  */
  if (err)
    return __set_errno (EINVAL);

  /* Don't access areas outside the mapped range.  */
  if (addr + len > mmap_addr + size)
    return __set_errno (ENOMEM);

  /* Only valid if this is a file open for non-private writing.  */
  if ((fd == -1 || IS_SHM_FD(fd)) || BADF(fd)
      || !(prot & PROT_WRITE) || (flags & MAP_PRIVATE))
    return __set_errno (EINVAL);

  /* If no length specified, flush the whole mapping.  */
  if (!len)
    {
      len = size;
      addr = mmap_addr;
    }

  off_t oldpos = lseek (fd, 0, SEEK_CUR);
  lseek (fd, offset + addr - mmap_addr, SEEK_SET);
  int fail = write (fd, addr, len) != len;
  lseek (fd, oldpos, SEEK_SET);

  return fail ? __set_errno (EIO) : 0;
}

/* Advise the system about particular usage patterns the program follows
   for the region starting at ADDR and extending LEN bytes.  */
int
madvise (void * addr, size_t len, int advice)
{
  const struct ul_global *gbl = &__ul_global;

  if ((size_t)addr & (gbl->pagesize - 1))
    return __set_errno (EINVAL);

  int fd;
  _kernel_oserror *err;
  err = _swix(ARMEABISupport_MMapOp, _INR(0,1)|_OUT(4),
				     ARMEABISUPPORT_MMAPOP_GET_INFO,
				     addr,
				     &fd);
  /* For simplicity don't allow remapping of mapped files.  */
  if (err || (fd != -1 && !BADF(fd)))
    return __set_errno (ENOSYS);

  err = _swix(ARMEABISupport_MMapOp, _INR(0,3), ARMEABISUPPORT_MMAPOP_ADVISE,
						addr,
						len,
						advice);
  return err ? __ul_seterr (err, __errno_from_armeabisupport(err->errnum)) : 0;
}

/* Remap addresses mapped by the range [ADDR,ADDR+OLD_LEN) to new length
   NEW_LEN.  If MAY_MOVE is MREMAP_MAYMOVE the returned address may differ
   from ADDR.  The return value is the actual mapping address chosen or
   (void *) -1 for errors (in which case `errno' is set).  */

void *
mremap (void * addr, size_t old_len, size_t new_len, int may_move)
{
  _kernel_oserror *err;
  void *result;

  if (new_len > old_len && mmap_too_big (new_len))
    return (void *) __set_errno (ENOMEM);

  err = _swix(ARMEABISupport_MMapOp, _INR(0,4)|_OUT(0), ARMEABISUPPORT_MMAPOP_MREMAP,
							addr,
							old_len,
							new_len,
							may_move,
							&result);

  return err ? (void *)__ul_seterr (err, __errno_from_armeabisupport(err->errnum)) : result;
}

int shm_open(const char *name, int oflag, mode_t mode)
{
  _kernel_oserror *err;
  int fd;
  err = _swix(ARMEABISupport_ShmOp, _INR(0,3)|_OUT(0), ARMEABISUPPORT_SHMOP_OPEN,
						       name,
						       oflag,
						       mode,
						       &fd);

  return err ? __ul_seterr (err, __errno_from_armeabisupport (err->errnum)) : fd;
}

int shm_unlink(const char *name)
{
  if (strlen(name) > PATH_MAX)
    return ENAMETOOLONG;
  _kernel_oserror *err;

  err = _swix (ARMEABISupport_ShmOp, _INR(0,1), ARMEABISUPPORT_SHMOP_UNLINK,
					        name);

  return err ? __ul_seterr (err, __errno_from_armeabisupport (err->errnum)) : 0;
}

int __errno_from_armeabisupport (int errnum)
{
  int error;
  switch (errnum)
  {
  case ARMEABISUPPORT_ERROR_EACCES:
    error = EACCES;
    break;
  case ARMEABISUPPORT_ERROR_EEXIST:
    error = EEXIST;
    break;
  case ARMEABISUPPORT_ERROR_EINVAL:
    error = EINVAL;
    break;
  case ARMEABISUPPORT_ERROR_ENAMETOOLONG:
    error = ENAMETOOLONG;
    break;
  case ARMEABISUPPORT_ERROR_ENOENT:
    error = ENOENT;
    break;
  case ARMEABISUPPORT_ERROR_ENOSPC:
    error = ENOSPC;
    break;
  case ARMEABISUPPORT_ERROR_EBADF:
    error = EBADF;
    break;
  case ARMEABISUPPORT_ERROR_ENOMEM:
    error = ENOMEM;
    break;
  default:
    error = EOPSYS;
    break;
  }

  return error;
}
