/* UnixLib fwrite() implementation.
   Copyright 2001-2011 UnixLib Developers.  */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <pthread.h>
#include <internal/unix.h>

/* #define DEBUG */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

/* Return number of objects written.  */
size_t
fwrite (const void *data, size_t size, size_t count, FILE *stream)
{
  PTHREAD_UNSAFE

  /* Check for any errors.  */
  if (!__validfp (stream))
    {
      (void) __set_errno (EINVAL);
      return 0;
    }

  if (!stream->__mode.__bits.__write)
    stream->__error = 1;

  if (ferror (stream))
    return 0;

  size_t to_write = size * count;
  if (to_write == 0)
    return 0;

#ifdef DEBUG
  debug_printf ("-- fwrite(%d): to_write=%d", stream->fd, to_write);
#endif

  size_t total_bytes = 0;
  if (stream->o_base != NULL)
    {
      /* The special file descriptor of -1 is used when writing to a memory
	 buffer, such as the function 'sprintf' would (cfr __vsnprintf
	 implementation).  In this circumstance, if we have been requested
	 to write more data than the buffer contains, truncate it.  */
      if (stream->fd == -1 && to_write > stream->o_cnt)
	{
	  to_write = stream->o_cnt;
	  if (to_write == 0)
	    return 0;
	}

      /* Optimisations appropriate for a buffered file.  */
      if (to_write > stream->o_cnt)
	{
#ifdef DEBUG
	  debug_printf (", direct write\n");
#endif
	  /* We have lots of data to output. First flush the buffer,
	     then just write the rest out.  */
	  if (__flsbuf (EOF, stream) < 0)
	    return (size_t) 0; /* we wouldn't have written anything yet.  */

	  /* Write it out in a loop, as recommended.  */
	  while (to_write)
	    {
	      size_t bytes = write (stream->fd, data, to_write);
	      if (bytes == -1)
		{
		  stream->__error = 1;
		  break;
		}
	      to_write -= bytes;
	      /* write () may take fewer bytes than it was given: carry on
		 after them, do not write the same bytes again.  */
	      data = (const void *)((const char *)data + bytes);
	      /* Increment the internal file offset.  */
	      stream->__offset += bytes;
	      total_bytes += bytes;
	    }
	}
      else
	{
	  /* The data is small enough to place in the output buffer.  */

	  /* Increment the file pointers (part 1). */
	  total_bytes += to_write;
#ifdef DEBUG
	  debug_printf (", buffering");
#endif
	  size_t bytes;
	  if (stream->__linebuf)
	    {
	      /* Find the last \n and use that as end of the part to at least
		 flush it.  */
	      if (((const unsigned char *)data)[to_write - 1] == '\n')
		bytes = to_write;
	      else
		{
		  const unsigned char *last_nl = memrchr (data, '\n', to_write);
		  bytes = last_nl ? last_nl + 1 - (const unsigned char *)data : to_write;

		  /* Mark there is at least one line buffered stdio stream
		     which has an unflushed buffer so that we can force flush
		     it when reading from a tty attached stream.  */
		  __ul_global.fls_lbstm_on_rd = 1;
		}
	    }
	  else
	    bytes = to_write;

	  do
	    {
	      /* Increment the file pointers (part 2). */
	      stream->o_cnt -= bytes;

	      if (bytes >= 16)
		{
		  memcpy (stream->o_ptr, data, bytes);
		  data = (const void *)((const unsigned char *)data + bytes);
		  stream->o_ptr += bytes;
		}
	      else
		{
		  /* Try the fast case.  */
		  unsigned char *p = stream->o_ptr;
		  size_t nbytes = bytes;
		  while (nbytes-- > 0)
		    *p++ = *(const unsigned char *)data++;
		  stream->o_ptr = p;
		}

	      to_write -= bytes;
	      bytes = to_write;
	      if (bytes != 0 /* line buffering and \n is not the last char to write.  */
		  || stream->fd != -1 && stream->o_ptr[-1] == '\n')
		{
#ifdef DEBUG
		  debug_printf (", flushing\n");
#endif
		  __flsbuf (EOF, stream);
		}
#ifdef DEBUG
	      else
		debug_printf ("\n");
#endif
	    } while (bytes);
	}
    }
  else
    {
#ifdef DEBUG
      debug_printf (", unbuffered write\n");
#endif
      /* Optimisations appropriate for an unbuffered file.
	 We don't have to worry about all that buffer crap :-) */
      while (to_write)
	{
	  /* This check is for (v)snprintf with a NULL buffer */
	  size_t bytes;
	  if (stream->fd == -1)
	    bytes = to_write;
	  else
	    bytes = write (stream->fd, data, to_write);

	  if (bytes == -1)
	    {
	      stream->__error = 1;
	      break;
	    }
	  to_write -= bytes;
	  data = (const void *)((const char *)data + bytes);
	  stream->__offset += bytes;
	  total_bytes += bytes;
	}
    }

  /* Return the number of objects actually written.  */
  return total_bytes / size;
}
strong_alias (fwrite, fwrite_unlocked)
