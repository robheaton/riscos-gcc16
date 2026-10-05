/* Low-level device handling.
   Copyright (c) 2002-2020 UnixLib Developers.  */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <swis.h>
#include <unistd.h>

#include <unixlib/local.h>
#include <internal/dev.h>
#include <internal/os.h>
#include <internal/local.h>
#include <internal/swiparams.h>
#include <internal/unix.h>
#include <internal/symlinks.h>

/* #define DEBUG */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

const struct dev __dev[NDEV] = {
  /* DEV_RISCOS */
  {__fsopen, __fsclose, __fsread, __fswrite,
   __fslseek, __nullioctl, __nullselect, __fsstat, __fsfstat,
   __fslstat},

  /* DEV_TTY */
  {__ttyopen, __ttyclose, __ttyread, __ttywrite,
   __nulllseek, __ttyioctl, __ttyselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_PIPE */
  {__pipeopen, __pipeclose, __piperead, __pipewrite,
   __nulllseek, __nullioctl, __pipeselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_NULL */
  {__nullopen, __nullclose, __nullread, __nullwrite,
   __nulllseek, __nullioctl, __nullselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_SOCKET */
  /* Socket select is a special case.  */
  {__sockopen, __sockclose, __sockread, __sockwrite,
   __nulllseek, __sockioctl, __sockselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_ZERO */
  {__nullopen, __nullclose, __zeroread, __nullwrite,
   __nulllseek, __nullioctl, __nullselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_RANDOM */
  {__randomopen, __nullclose, __randomread, __nullwrite,
   __nulllseek, __nullioctl, __nullselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_DSP */
  {__dspopen, __dspclose, __nullread, __dspwrite,
   __dsplseek, __dspioctl, __nullselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_CUSTOM */
  {__customopen, __customclose, __customread, __customwrite,
   __customlseek, __customioctl, __customselect, __nullstat, __nullfstat,
   __nullstat},

  /* DEV_EVENTFD */
  {__nullopen, __eventfd_close, __eventfd_read, __eventfd_write,
   __nulllseek, __nullioctl, __eventfd_select, __nullstat, __nullfstat,
   __nullstat}
};

/* Map of special device names to device types.  */
struct sfile
{
  const char *name;		/* The 'xxx' in /dev/xxx (if defined)  */
  const dev_t dev;		/* Allocated device number, see DEV_* definitions  */
};

/* Map of RISC OS FS to device types.  */
struct rofs2dev
{
  const char *rofsname;
  const dev_t dev;
};

static const struct sfile __sfile[] = {
  {"tty", DEV_TTY},
  {"console", DEV_TTY},
  {"rs423", DEV_TTY},
  {"ttyS0", DEV_TTY},
  {"null", DEV_NULL},
  {"zero", DEV_ZERO},
  {"random", DEV_RANDOM},
  {"urandom", DEV_RANDOM},
  {"dsp", DEV_DSP},
  {"custom", DEV_CUSTOM},
/*   {"pipe", DEV_PIPE}, does open on /dev/pipe make sense ?  */
/*   {"socket", DEV_SOCKET}, does open on /dev/socket make sense ?  */
  {NULL, DEV_RISCOS}		/* table terminator */
};

static const struct rofs2dev __rofs2dev[] = {
  {"null:", DEV_NULL},
/*   {"pipe:", DEV_PIPE}, makes sense ? What about filename mapping ?  */
  {NULL, DEV_RISCOS}		/* table terminator */
};

dev_t
__getdevtype (const char *filename, int ro_control)
{
  if ((ro_control & __RISCOSIFY_NO_PROCESS)
      || !(ro_control & __RISCOSIFY_STRICT_UNIX_SPECS))
    {
      const struct rofs2dev *rofs2dev;

      for (rofs2dev = __rofs2dev; rofs2dev->rofsname != NULL; ++rofs2dev)
	{
	  if (strncmp (rofs2dev->rofsname, filename,
		       strlen (rofs2dev->rofsname)) == 0)
	    return rofs2dev->dev;
	}
    }

  if (filename[0] == '/' && filename[1] == 'd' && filename[2] == 'e' &&
      filename[3] == 'v' && filename[4] == '/')
    {
      const struct sfile *special = __sfile;

      while (special->name != NULL)
	{
	  if (strcmp (special->name, filename + sizeof ("/dev/") - 1) == 0)
	    break;
	  special++;
	}

      return special->dev;
    }

  return DEV_RISCOS;
}

void *
__fsopen (struct __unixlib_fd *file_desc, const char *ux_filename, int mode)
{
  int sftype, fflag = file_desc->fflag;
  char file[MAXPATHLEN + 2];	/* Want two chars for ".^" below.  */
  char *end_of_filename = __riscosify_std (ux_filename,
					   fflag & (O_CREAT | O_WRONLY | O_RDWR),
					   file, sizeof (file) - 2, &sftype);
  if (end_of_filename == NULL)
    return (void *) __set_errno (ENAMETOOLONG);

  const char *ro_filename;
#if __UNIXLIB_SYMLINKS
  char target[MAXPATHLEN + 2];
  if (__resolve_symlinks (file, target, MAXPATHLEN, 0) != 0)
    return (void *) -1;

  ro_filename = target;
#else
  ro_filename = file;
#endif

  /* Get file vital statistics.  */
  const _kernel_oserror *err;
  int objtype, attr;
  if ((err = SWI_OS_File_ReadCatInfo (ro_filename, &objtype, NULL, NULL,
				      NULL, &attr)) != NULL)
    return (void *) __ul_seterr (err, EOPSYS);
  if (objtype)
    {
      /* Some sort of object exists.  */
      if ((fflag & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT))
	/* File exists and the user set O_EXCL.  */
	return (void *) __set_errno (EEXIST);

      /* Check for permission to access the file in the mode we
         requested.  */
      if (objtype == 2
	  || (objtype == 3 && !__get_feature_imagefs_is_file ()))
	{
	  /* Directory or image file.  Set errno if the user specified write
	     access.  */
	  if ((fflag & O_ACCMODE) != O_RDONLY)
	    return (void *) __set_errno (EISDIR);

	  file_desc->dflag |= FILE_ISDIR;

#ifdef DEBUG
	  debug_printf ("open directory (read only): file=%s\n", ro_file);
#endif
	  DIR *dir = opendir (ux_filename);
	  /* When dir is NULL, errno is already set so no need to set it again.  */
	  return (dir == NULL) ? (void *) -1 : (void *) dir;
	}

      /* Remove creation bits.  */
      fflag &= ~O_CREAT;

      __mode_t access_mode = __get_protection (attr);
      if (((fflag & O_ACCMODE) == O_RDONLY && !(access_mode & S_IRUSR))
	  || ((fflag & O_ACCMODE) == O_WRONLY && !(access_mode & S_IWUSR))
	  || ((fflag & O_ACCMODE) == O_RDWR
	      && ((access_mode & (S_IWUSR | S_IRUSR)) != (S_IWUSR | S_IRUSR))))
	return (void *) __set_errno (EACCES);
    }
  else
    {
      /* We try to do our best to get at the filing system name
	 by canonicalising the filename if possible, otherwise checking
	 for pipe: anyway.
	 We don't need the full filename, just need to check that the first
	 few characters begin with 'pipe'.
	 FIXME: This is an ugly hack (= who says the too-small buffer will
	 be filled in correctly for the first 16 bytes ?) to make opening
	 non-existent files for reading on PipeFS work.  We should handle
	 PipeFS though a proper device ioctl.  So SWI call always generates
	 an error which we ignore.  */
      char temp[16];
      SWI_OS_FSControl_Canonicalise (ro_filename, NULL, temp, sizeof (temp), NULL);
      if (!(tolower (temp[0]) == 'p'
	    && tolower (temp[1]) == 'i'
	    && tolower (temp[2]) == 'p'
	    && tolower (temp[3]) == 'e'
	    && temp[4] == ':'))
	{
	  /* If no file exists and O_CREAT was not specified,
	     return ENOENT.  */
	  if (!(fflag & O_CREAT))
	    return (void *) __set_errno (ENOENT);
	}
    }

  if (fflag & O_CREAT)
    {
      /* If the O_CREAT flag has been set, then create an empty file. Strict
	 POSIX says that there is no difference between text and binary
	 files. */
      if ((err = SWI_OS_File_CreateFileWithFileType (ro_filename,
						     (sftype == __RISCOSIFY_FILETYPE_NOTFOUND) ? 0xfff : sftype,
						     0)) != NULL)
	{
	  /* Could not create.  Cannot assume component of name does not
	     exist, as errors such as "directory full" end up here.  */
	  const char *where = ro_filename;
	  int leafonly = 1;
	  char ch;
	  while ((ch = *where++))
	    if (ch == '.' || ch == ':')
	      {
		leafonly = 0;
		break;
	      }

	  if (!leafonly)
	    {
	      /* There was a '.' or ':' present, so check for parent
	         directory.  end_of_filename points to the terminating
	         '\0' in the riscosified filename.  If that filename ends
	         ':' (eg "parallel:") append '^' else append '.^'.  Need to
	         check for leafname only, as "!Run.^" gives an error.  */
	      if (end_of_filename[-1] != ':')
		*end_of_filename++ = '.';
	      *end_of_filename = ':';

	      if (__object_exists_ro (file) != 2)
		return (void *) __set_errno (ENOENT);
	      /* No parent directory - throw Unix error.  */
	    }
	  /* Parent directory exists - implies OS problem creating file.  */
	  return (void *) __ul_seterr (err, EOPSYS);
	}
    }

  /* Open the file.  */
  int openmode;
  if (fflag & O_TRUNC)
    openmode = OSFIND_OPENOUT;
  else
    {
      switch (fflag & O_ACCMODE)
	{
	  case O_RDWR:
	  case O_WRONLY:
	    openmode = OSFIND_OPENUP;
	    break;
	  case O_RDONLY:
	    openmode = OSFIND_OPENIN;
	    break;
	  default:
	    return (void *) __set_errno (EINVAL);
	}
    }
  unsigned fd;
  if ((err = SWI_OS_Find_Open (openmode, ro_filename, &fd)) != NULL)
    return (void *) __ul_seterr (err, EOPSYS);

  /* Now set the protection. This must be done after the file is opened,
     so that the file can be created for writing but set to read only.
     RCS needs to do that.  However, ShareFS objects to having file
     attributes changed while a file is open, so it needs to be closed
     first then reopened  */
  if (fflag & O_CREAT)
    {
      mode &= ~(__ul_global.sulproc->umask & 0777);
      int attr = __set_protection (mode);

      /* Write the object attributes.  */
      if (SWI_OS_File_WriteCatInfoAttr (ro_filename, attr))
	{
	  /* Now try closing it first if that failed */
	  if ((err = SWI_OS_Find_Close (fd)) != NULL
	      || (err = SWI_OS_File_WriteCatInfoAttr (ro_filename, attr)) != NULL
	      || (err = SWI_OS_Find_Open (openmode, ro_filename, &fd)) != NULL)
	    return (void *) __ul_seterr (err, EOPSYS);
	}
    }

  /* Check the 'stream is unallocated' bit.  Probably caused
     by us trying to open a file that is already open.  */
  int status;
  if ((err = SWI_OS_Args_GetFileHandleStatus (fd, &status)) != NULL
      || (status & (1 << 11)) != 0)
    {
      SWI_OS_Find_Close (fd);
      /* Trying again *might* fix the problem.  */
      return (void *) __set_errno (EAGAIN);
    }

  return (void *) fd;
}

int
__fsclose (struct __unixlib_fd *file_desc)
{
  /* If we got the RISC OS file handle from OS, we don't close it.  */
  if (file_desc->dflag & FILE_HANDLE_FROM_OS)
    return 0;

  if (file_desc->dflag & FILE_ISDIR)
    return closedir ((DIR *) file_desc->devicehandle->handle);

  /* Check if RISC OS file handle is in use by OS_ChangeRedirection.  If so,
     detach it.  */
  const _kernel_oserror *err;
  unsigned prev_fh_in, prev_fh_out;
  if ((err = SWI_OS_ChangeRedirection (-1, -1, &prev_fh_in, &prev_fh_out)) != NULL)
    return __ul_seterr (err, EOPSYS);

  int fhandle = (int) file_desc->devicehandle->handle;
  if ((fhandle == prev_fh_in || fhandle == prev_fh_out)
      && (err = SWI_OS_ChangeRedirection (fhandle == prev_fh_in ? 0 : -1,
					  fhandle == prev_fh_out ? 0 : -1,
					  NULL, NULL)) != NULL)
    return __ul_seterr (err, EOPSYS);

  char *buffer;
  if (file_desc->fflag & O_UNLINKED)
    buffer = __canonicalise_handle (fhandle);
  else
    buffer = NULL;

#ifdef DEBUG
  debug_printf ("-- __fsclose: ro file handle 0x%x, buffer '%s'\n", fhandle,
		buffer ? buffer : "<NULL>");
#endif

  /* Close file.  */
  err = SWI_OS_Find_Close (fhandle);
  if (!err && buffer)
    {
      err = SWI_OS_File_DeleteObject (buffer);
      /* Delete the suffix swap dir if it is now empty */
      __unlinksuffix (buffer);	/* buffer is corrupted by this call */
    }

  free (buffer);
  return (!err) ? 0 : __ul_seterr (err, EOPSYS);
}

int
__fsread (struct __unixlib_fd *file_desc, void *data, int nbyte)
{
  if (file_desc->dflag & FILE_ISDIR)
    {
      struct dirent entry, *result;

#ifdef DEBUG
      debug_printf ("-- __fsread: read directory entry\n");
#endif
      if (readdir_r ((DIR *) file_desc->devicehandle->handle, &entry, &result))
	return -1;
      if (!result)
	return -1;
      memcpy (data, entry.d_name,
	      (nbyte < entry.d_namlen) ? nbyte : entry.d_namlen);
      /* FIXME: If the buffer is too small then we will lose the
         rest of the filename */
      return entry.d_namlen;
    }
  else
    {
      const _kernel_oserror *err;
      unsigned not_read;
      __touch_stack_buffer (data, nbyte);
      if ((err = SWI_OS_GBPB_ReadBytes ((int) file_desc->devicehandle->handle,
					data, nbyte, &not_read)) != NULL)
	return __ul_seterr (err, EOPSYS);

      return nbyte - not_read;
    }
}

int
__fswrite (struct __unixlib_fd *file_desc, const void *data, int nbyte)
{
#ifdef DEBUG
  debug_printf ("-- __fswrite(%p, nbyte=%d)\n", file_desc->devicehandle->handle,
		nbyte);
#endif
  const _kernel_oserror *err;
  unsigned not_written;
  if ((err = SWI_OS_GBPB_WriteBytes ((int) file_desc->devicehandle->handle,
				     data, nbyte, &not_written)) != NULL)
    return __ul_seterr (err, EOPSYS);

  return nbyte - not_written;
}

__off_t
__fslseek (struct __unixlib_fd * file_desc, __off_t lpos, int whence)
{
  if (file_desc->dflag & FILE_ISDIR)
    return 0; /* FIXME?: Should we call seekdir() ? */

  const _kernel_oserror *err;
  int handle = (int) file_desc->devicehandle->handle;
  if (whence == SEEK_SET)
    err = SWI_OS_Args_SetFilePtr (handle, lpos);
  else if (whence == SEEK_CUR)
    {
      __off_t fileptr;
      err = SWI_OS_Args_GetFilePtr (handle, &fileptr);
      if (!err)
	{
	  __off_t old_lpos = lpos;
	  lpos += fileptr;
	  if (old_lpos)
	    err = SWI_OS_Args_SetFilePtr (handle, lpos);
	}
    }
  else if (whence == SEEK_END)
    {
      __off_t extent;
      if ((err = SWI_OS_Args_GetExtent (handle, &extent)) == NULL)
	{
	  lpos += extent;
	  err = SWI_OS_Args_SetFilePtr (handle, lpos);
	}
    }
  else
    return __set_errno (EINVAL);

  return !err ? lpos : __ul_seterr (err, EOPSYS);
}

int
__fsstat (const char *ux_filename, struct stat *buf)
{
  if (buf == NULL)
    return __set_errno (EINVAL);

  /* When we have suffix swapping defined, the directory names are not
     suffix swapped so check first without suffix swapping if we can't
     find a directory.  */
  int riscosify_control, dir_suffix_check;
  if (((riscosify_control = __get_riscosify_control ())
       & __RISCOSIFY_NO_SUFFIX) == 0)
    {
      /* Suffix swapping is setup.  */
      __set_riscosify_control (riscosify_control | __RISCOSIFY_NO_SUFFIX);
      dir_suffix_check = 1;
    }
  else
    dir_suffix_check = 0;

  char filename[_POSIX_PATH_MAX];
  int objtype, load, exec, length, attr;
  int rtrn_get_attrs = __object_get_attrs (ux_filename, filename,
					   sizeof (filename), &objtype,
					   NULL, &load, &exec, &length, &attr);

  if (dir_suffix_check)
    {
      /* Restore suffix swapping status.  */
      __set_riscosify_control (riscosify_control);
    }

  if (rtrn_get_attrs)
    {
      if (dir_suffix_check == 0
	  || __object_get_attrs (ux_filename, filename, sizeof (filename),
				 &objtype, NULL, &load, &exec, &length,
				 &attr))
	return -1;
    }

  buf->st_ino = __get_file_ino (NULL, filename);

  return __stat (objtype, load, exec, length, attr, buf);
}

int
__fsfstat (int fd, struct stat *buf)
{
  const struct __unixlib_fd *file_desc = getfd (fd);

  char *buffer;
  if (file_desc->dflag & FILE_ISDIR)
    buffer = strdup (((DIR *) file_desc->devicehandle->handle)->dd_name_can);
  else
    buffer = __canonicalise_handle ((int) file_desc->devicehandle->handle);

  if (buffer == NULL)
    return __set_errno (EBADF);

  /* Get vital file statistics.  */
  const _kernel_oserror *err;
  unsigned objtype, loadaddr, execaddr, objlen, attr;
  if ((err = SWI_OS_File_ReadCatInfo (buffer, &objtype, &loadaddr,
				      &execaddr, &objlen, &attr)) != NULL)
    {
      free (buffer);
      return __ul_seterr (err, EIO);
    }
  buf->st_ino = __get_file_ino (NULL, buffer);
  free (buffer);

  if (!(file_desc->dflag & FILE_ISDIR))
    {
      /* OS_File ReadCatInfo returns the allocated size of the file,
         but we want the current extent of the file */
      if ((err = SWI_OS_Args_GetExtent ((int) file_desc->devicehandle->handle,
					(__off_t *)&objlen)) != NULL)
	return __ul_seterr (err, EIO);
    }

  return __stat (objtype, loadaddr, execaddr, objlen, attr, buf);
}

int
__fslstat (const char *ux_filename, struct stat *buf)
{
  if (buf == NULL)
    return __set_errno (EINVAL);

  /* When we have suffix swapping defined, the directory names are not
     suffix swapped so check first without suffix swapping if we can't
     find a directory.  */
  int riscosify_control, dir_suffix_check;
  if (((riscosify_control = __get_riscosify_control ())
       & __RISCOSIFY_NO_SUFFIX) == 0)
    {
      /* Suffix swapping is setup.  */
      __set_riscosify_control (riscosify_control | __RISCOSIFY_NO_SUFFIX);
      dir_suffix_check = 1;
    }
  else
    dir_suffix_check = 0;

  char filename[_POSIX_PATH_MAX];
  int objtype, load, exec, length, attr, ftype;
  int rtrn_get_attrs = __object_get_lattrs (ux_filename, filename,
					    sizeof (filename), &objtype,
					    &ftype, &load, &exec, &length, &attr);

  if (dir_suffix_check)
    {
      /* Restore suffix swapping status.  */
      __set_riscosify_control (riscosify_control);
    }

  if (rtrn_get_attrs)
    {
      if (dir_suffix_check == 0
	  || __object_get_lattrs (ux_filename, filename, sizeof (filename),
				  &objtype, &ftype, &load, &exec, &length,
				  &attr))
	return -1;
    }

  buf->st_ino = __get_file_ino (NULL, filename);

  __stat (objtype, load, exec, length, attr, buf);

#if __UNIXLIB_SYMLINKS
  if (ftype == SYMLINK_FILETYPE)
    {
      buf->st_mode &= ~S_IFMT;
      buf->st_mode |= S_IFLNK;
    }
#endif

  return 0;
}

void *
__pipeopen (struct __unixlib_fd *file_desc, const char *file, int mode)
{
  return __fsopen (file_desc, file, mode);
}

int
__pipeclose (struct __unixlib_fd *file_desc)
{
  return __fsclose (file_desc);
}

int
__piperead (struct __unixlib_fd *file_desc, void *data, int nbyte)
{
  return __fsread (file_desc, data, nbyte);
}

int
__pipewrite (struct __unixlib_fd *file_desc, const void *data, int nbyte)
{
  int handle = (int) file_desc->devicehandle->handle;
  const _kernel_oserror *err;
  /* Read current file position.  */
  __off_t fileptr;
  if ((err = SWI_OS_Args_GetFilePtr (handle, &fileptr)) == NULL)
    {
      /* Read extent && set file pointer to end of file.  */
      __off_t extent;
      if ((err = SWI_OS_Args_GetExtent (handle, &extent)) == NULL
	  && (fileptr == extent
	      || (err = SWI_OS_Args_SetFilePtr (handle, extent)) == NULL))
	{
	  /* Write some more data to the "pipe" at the end of the file.  */
	  int write_err = __fswrite (file_desc, data, nbyte);
	  /* Restore the pointer to where we found it.  */
	  if ((err = SWI_OS_Args_SetFilePtr (handle, fileptr)) == NULL)
	    return write_err;
	}
    }
  return __ul_seterr (err, EPIPE);
}

int
__pipeselect (struct __unixlib_fd *file_desc, int fd,
	      __fd_set * pread, __fd_set * pwrite, __fd_set * except)
{
  /* Return ready to write, no exceptional conditions.  Ready to read is a
     little more tricky.  Bascially, work out if there is any data ready.
     Start by assuming there is nothing to read.  */
  int to_read = 0;

  if (pread)
    {
      /* Read current file position.  */
      __off_t pos;
      if (!SWI_OS_Args_GetFilePtr ((int) file_desc->devicehandle->handle, &pos))
	{
	  /* Read extent.  */
	  __off_t extent;
	  if (!SWI_OS_Args_GetExtent ((int) file_desc->devicehandle->handle,
				      &extent)
              && pos != extent)
	    to_read = 1;
	  /* If file pointer != extent then there is still data to read.  */
	}

      /* If this is the only fd left open on the pipe then there is EOF
         to read */
      if (file_desc->devicehandle->refcount < 2)
	to_read = 1;

      if (to_read)
	FD_SET (fd, pread);
      else
	FD_CLR (fd, pread);
    }

  if (pwrite)
    FD_SET (fd, pwrite);
  if (except)
    FD_CLR (fd, except);

  /* This may not be correct, but it is consistent with Internet 5 select.  */
  return to_read + (pwrite ? 1 : 0);
}

void *
__nullopen (struct __unixlib_fd *file_desc, const char *file, int mode)
{
  return (void *) 1;
}

int
__nullclose (struct __unixlib_fd *file_desc)
{
  return 0;
}

int
__nullread (struct __unixlib_fd *file_desc, void *data, int nbyte)
{
  return 0;
}

int
__nullwrite (struct __unixlib_fd *file_desc, const void *data, int nbyte)
{
  return nbyte;
}

__off_t
__nulllseek (struct __unixlib_fd * file_desc, __off_t lpos, int whence)
{
  return __set_errno (ESPIPE);
}

int
__nullioctl (struct __unixlib_fd *file_desc, unsigned long request, void *arg)
{
  switch (request)
    {
      case FIOASYNC:
        /* Set/Clear async I/O.  Do nothing */
        return 0;
    }

  return __set_errno (EINVAL);
}

int
__nullselect (struct __unixlib_fd *file_desc, int fd,
	      __fd_set * cread, __fd_set * cwrite, __fd_set * except)
{
  /* Return ready to read, ready to write, no execptional conditions.  */

  if (cread)
    FD_SET (fd, cread);
  if (cwrite)
    FD_SET (fd, cwrite);
  if (except)
    FD_CLR (fd, except);

  /* This may not be correct, but it is consistent with Internet 5 select.  */
  return (cread ? 1 : 0) + (cwrite ? 1 : 0);
}

int
__nullstat (const char *filename, struct stat *buf)
{
  buf->st_ino = 0;

  __stat (0, 0, 0, 0, 0, buf);

  buf->st_mode |= S_IRUSR | S_IWUSR;

  return 0;
}

int
__nullfstat (int fd, struct stat *buf)
{
  buf->st_ino = 0;

  __stat (0, 0, 0, 0, 0, buf);

  const struct __unixlib_fd *file_desc = getfd (fd);
  int fflag = file_desc->fflag;

  __mode_t mode = 0;
  if (fflag & O_RDONLY)
    mode |= S_IRUSR;
  if (fflag & O_WRONLY)
    mode |= S_IWUSR;
  if (fflag & O_PIPE)
    mode |= S_IFIFO;

  buf->st_mode |= mode;

  return 0;
}

void *
__sockopen (struct __unixlib_fd *file_desc, const char *file, int mode)
{
  return (void *) __set_errno (EOPNOTSUPP);
}

int
__sockclose (struct __unixlib_fd *file_desc)
{
  return _sclose ((int) file_desc->devicehandle->handle);
}

int
__sockread (struct __unixlib_fd *file_desc, void *data, int nbyte)
{
  __touch_stack_buffer (data, nbyte);
  return _sread ((int) file_desc->devicehandle->handle, data, nbyte);
}

int
__sockwrite (struct __unixlib_fd *file_desc, const void *data, int nbyte)
{
  return _swrite ((int) file_desc->devicehandle->handle, data, nbyte);
}

int
__sockioctl (struct __unixlib_fd *file_desc, unsigned long request, void *arg)
{
  return _sioctl ((int) file_desc->devicehandle->handle, request, arg);
}

int
__sockselect (struct __unixlib_fd *file_descriptor, int fd,
	      __fd_set * sread, __fd_set * swrite, __fd_set * except)
{
  /* Return 1,1,1 so that merge routine will copy in this socket's
     results.  */

  if (sread)
    FD_SET (fd, sread);
  if (swrite)
    FD_SET (fd, swrite);
  if (except)
    FD_SET (fd, except);

  /* None are `live' yet.  */
  return 0;
}

/* Implements /dev/zero */
int
__zeroread (struct __unixlib_fd *file_desc, void *data, int nbyte)
{
  memset (data, 0, nbyte);

  return nbyte;
}

/* Implements /dev/random, /dev/urandom */
void *
__randomopen (struct __unixlib_fd *file_desc, const char *file, int mode)
{
  int regs[10];

  /* Test for the existance of the CryptRandom module.  */
  if (__os_swi (CryptRandom_Stir, regs))
    {
      /* Try to load the module.  */
      const _kernel_oserror *err;
      if ((err = SWI_OS_CLI ("RMEnsure CryptRandom 0.12 RMLoad System:Modules.CryptRand")) != NULL
	  || (err = SWI_OS_CLI ("RMEnsure CryptRandom 0.12 Error 16_10F /dev/random support requires CryptRand 0.12 or newer")) != NULL)
	return (void *) __ul_seterr (err, EOPSYS);

      /* If still not available, then the open must fail.  */
      if ((err = __os_swi (CryptRandom_Stir, regs)) != NULL)
	return (void *) __ul_seterr (err, ENOENT);
    }

  return (void *) 1;
}

int
__randomread (struct __unixlib_fd *file_desc, void *data, int nbyte)
{
  int regs[10];
  _kernel_oserror *err;

  regs[0] = (int) data;
  regs[1] = nbyte;
  if ((err = __os_swi (CryptRandom_Block, regs)) != NULL)
    return __ul_seterr (err, EOPSYS);

  return nbyte;
}
