/* UnixLib fread() implementation.
   Copyright 2000-2011 UnixLib Developers.  */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include <pthread.h>
#include <internal/unix.h>

/* Return number of objects read.  */
size_t
fread (void *data, size_t size, size_t count, FILE *stream)
{
  PTHREAD_UNSAFE

  /* Check for any errors.  */
  if (!__validfp (stream))
    {
      (void) __set_errno (EINVAL);
      return 0;
    }

  if (!stream->__mode.__bits.__read)
    stream->__error = 1;

  if (feof (stream) || ferror (stream))
    return 0;

  size_t to_read = size * count;
  if (to_read == 0)
    return 0;

  /* When we have possibly unflushed data in one of our line buffered streams
     and we're reading from a tty attached stream, flush all those streams.  */
  if (__ul_global.fls_lbstm_on_rd && isatty (fileno (stream)) && __flslbbuf ())
    return 0;

  /* If we don't do this ungetc() followed by fread() screws up.  */
  if (stream->__pushedback == 1)
    {
      stream->__pushedback = 0;
      stream->i_cnt = stream->__pushedi_cnt;

      *(char *)data = stream->__pushedchar;
      data = (void *)((char *)data + 1);
      to_read--;
    }

  if (stream->i_base != NULL)
    {
      /* Optimisations appropriate for a buffered file.  */

      /* Take the first set of data out of the file buffer.  */
      size_t bytes = (to_read > stream->i_cnt) ? stream->i_cnt : to_read;
      memcpy (data, stream->i_ptr, bytes);
      data = (void *)((char *)data + bytes);

      /* Increment the file pointers. */
      stream->i_cnt -= bytes;
      stream->i_ptr += bytes;
      to_read -= bytes;
    }

  if (stream->__string_istream)
    {
      if (to_read)
	stream->__eof = 1;
    }
  else
    {
      while (to_read)
	{
	  /* Read the rest of the data straight from the file.  */
	  ssize_t bytes = read (stream->fd, data, to_read);
	  if (bytes == 0)
	    {
	      /* Reading non-zero number of bytes and actually getting
		 zero bytes means having reached EOF.  */
	      stream->__eof = 1;
	      break;
	    }
	  else if (bytes == -1)
	    {
	      stream->__error = 1;
	      break;
	    }
	  else
	    {
	      to_read -= bytes;
	      /* read () may return fewer bytes than it was asked for: the next
		 read must continue after them, not store over them (the count
		 came out right and the data did not).  */
	      data = (void *)((char *)data + bytes);
	      /* Fix the file offset pointer.  */
	      stream->__offset += bytes;
	    }
	}
    }

  /* Return the number of objects actually read.  */
  return to_read ? count - (to_read + size - 1) / size : count;
}
strong_alias (fread, fread_unlocked)
