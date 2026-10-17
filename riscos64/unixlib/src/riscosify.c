/* riscosify.c - the translation of Unix file names to RISC OS file names (UnixLib 64: libunixlib/common/riscosify.c of UnixLib, changed for AArch64:
   SWI registers are 64 bits, no Shared C Library or SOLoader variants).  Copyright (c) 2000-2012 UnixLib Developers; revised BSD licence (licenses/UnixLib-COPYING.txt).  */

#include "ul64.h"

/* Special directory list.  */
struct sdir
{
  const char *name;
  const char *riscos_name;
};

typedef struct
{
  int defined;
  const struct sdir sdir;
} __sdir_default_map;

static const char wimp_scrap[] = "<Wimp$ScrapDir>";

/* /tmp is so common and can't guarantee that $.tmp exists.
   /pipe is used in some programs and can't guarantee that $.pipe exists.  */
static __sdir_default_map def_map[] = {
  { 0, {"pipe", wimp_scrap}, },
  { 0, {"tmp", wimp_scrap}, }
};

/* Maximum number of special directories. This should be dynamic, but
   I cannot be arsed to make it dynamic.  */
#define MAXSDIR 16

/* Special directory list.  */
static struct sdir __sdir[MAXSDIR];

/* Assumes little endian, sizeof (int) == 4 */

struct sfix
{
  struct sfix *next;
  char suffix[1];
};

/* Size of suffix table.  Must be a power of two.  */
#define SFIXSIZE 32

/* Special suffix list.

   This may still change, as the current find API allows the returned string
   to differ from the supplied string. eg "*.cpp" could map to "cc.*".  */
static struct sfix *__sfix[SFIXSIZE] = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static char *copy_or_null (char *to, const char *from, const char *buf_end);

#ifndef __TARGET_SOLOADER__
/* For all UnixFS$/#* global environment values, set up a mapping as
   requested by the user to map Unix directories to RISC OS directories.
   See __riscosify[_std] ().  Called from unix/features.c.  */
void
__sdirinit (void)
{
  int i, j;
  long regs[10];
  char buf[MAXPATHLEN];

  regs[1] = (long) buf;
  regs[3] = 0;

  for (i = 0; i < MAXSDIR; i++)
    {
      char *str;

      /* Use #* so "UnixFS$/" is not matched.  */
      regs[0] = (long) "UnixFS$/#*";
      /* Two less than buf size so can zero terminate below and add '.'.  */
      regs[2] = sizeof (buf) - 2;
      regs[4] = 3;

      /* OS_ReadVarVal fails when no more values found, so exit loop. */
      if (__os_swi (OS_ReadVarVal, regs) != NULL)
	break;

      str = buf + regs[2];
      *str = '\0';

      /* Save name and value.  */
      str = strdup ((char *) regs[3] + sizeof ("UnixFS$"));
      if (str == NULL)
	break;
      __sdir[i].name = str;

      str = strdup (buf);
      if (str == NULL)
	break;
      __sdir[i].riscos_name = str;

      /* Check defaults and mark them defined if found.  */
      for (j = sizeof (def_map) / sizeof (__sdir_default_map) - 1; j; j--)
	if (stricmp (__sdir[i].name, def_map[j].sdir.name) == 0)
	  def_map[j].defined = 1;
    }

  /* Add default special directories if not specified by user.  */
  for (j = sizeof (def_map) / sizeof (__sdir_default_map) - 1; j; j--)
    if (def_map[j].defined == 0)
      __sdir[i++] = def_map[j].sdir;

  /* Terminate list.  */
  if (i < MAXSDIR)
    __sdir[i].name = NULL;
}
#endif

static __inline int
sfix_hash (const char *suffix)
{
  return *suffix & (SFIXSIZE - 1);
}

/* Convert a string of suffixes separated by colons to a list of
   suffixes.  Called from unix/features.c.  */
void
__sfixinit (const char *list)
{
  int i;
  char ch;

  /* Free all memory previously allocated in __sfix.  */
  for (i = 0; i < SFIXSIZE; i++)
    {
      struct sfix *entry;

#ifndef __TARGET_SOLOADER__
      for (entry = __sfix[i]; entry;)
	{
	  struct sfix *next = entry->next;
	  free (entry);
	  entry = next;
	}
#endif
      __sfix[i] = NULL;
    }

  while ((ch = *list++))
    {
      const char *start;
      size_t length;
      struct sfix *entry;
      int where;

      if (ch == ':')
	continue;

      start = list - 1;
      while ((ch = *list++) && ch != ':')
	;

      length = list - start;	/* Includes ':' or '\0'.  */
      entry = (struct sfix *) malloc (length + sizeof (struct sfix *));
      if (entry == NULL)
	return;

      memcpy (entry->suffix, start, --length);
      entry->suffix[length] = '\0';
      where = sfix_hash (entry->suffix);
#ifdef DEBUG
      debug_printf ("%d '%s'\n", where, entry->suffix);
#endif
      entry->next = __sfix[where];
      __sfix[where] = entry;

      if (!ch)
	return;			/* Last suffix terminated on '\0'.  */
    }
}

/* End of init code.  */

enum sdirseg_result
{
  sdirseg_no_match,
  sdirseg_match,
  sdirseg_buf_too_small
};

/* sdirseg(&s1,&s2,len) checks s1 to see if it is a special directory name.
   If it is, it copies the translation into s2 and advances s1 to the
   next segment which starts with '/' or '\0'.
   Returns one of enum sdir_result.  */

static enum sdirseg_result
sdirseg (const char **in_p, char **out_p, const char *buf_end)
{
  const char *in, *s3;
  char *out = *out_p;
  char ch;
  int j;

  for (j = 0; j < MAXSDIR; j++)
    {
      s3 = __sdir[j].name;
      if (s3 == NULL)
	break;
      in = *in_p;
      while ((ch = *s3++) != '\0' && ch == *in++)
	;

      /* If we're at the end of s3, then the final in++ didn't occur.  */
      if (ch == '\0' && ((ch = *in) == '\0' || ch == '/'))
	{
	  out = copy_or_null (out, __sdir[j].riscos_name, buf_end);

	  if (!out)
	    return sdirseg_buf_too_small;

	  *in_p = in;
	  *out_p = out;
	  return sdirseg_match;
	}
    }

  return sdirseg_no_match;
}

const char *
__sfixfind (const char *sfix, size_t len)
{
  int where = sfix_hash (sfix);
  struct sfix *entry;

  for (entry = __sfix[where]; entry != NULL; entry = entry->next)
    {
      if (strncmp (entry->suffix, sfix, len) == 0
	  && entry->suffix[len] == '\0')
	return entry->suffix;

      /* Modifiying this struct would allow, e.g., mapping .C to cc.  */
    }

  /* Failed.  */
  return NULL;
}

/* Copy 'from' to 'to'. Return NULL if buffer overflowed, otherwise points
   to terminating 0 character in 'to' buffer.  */
static char *
copy_or_null (char *to, const char *from, const char *buf_end)
{
  while (to <= buf_end)
    if (!(*to++ = *from++))
      return to - 1;

  return NULL;
}

/* Copy from in to out, translating characters as necessary.
   Also handles ,xyz extensions and suffix swapping.
   Return NULL if buffer overflowed. */
static char *
translate_or_null (int create_dir, int flags,
		   char *buffer, const char *buf_end, int *filetype,
		   char *out, const char *in, int path)
{
  /* in points to a (possibly partial) unix pathname.
     If it was absolute, it will have had any leading '/'s stripped, and any
     RISC OS specific parts will already have been copied */

  const char *start = in;	/* The start of our input */
  const char *last_slash = in;	/* The next character after the last '/'
				   discovered in the input */
  const char *previous_slash = NULL;	/* The previous value of last_slash */
  char *last_out_slash = out;	/* The position in the output that corresponds
				   to last_slash in the input */
  const char *last_dot = NULL;	/* The last '.' found in the input */
  char *last_out_dot = NULL;	/* The position in the output that corresponds
				   to the last '.' found in the input */
  const char *suffix;

#ifdef DEBUG
  *out = '\0';
  debug_printf ("-- translate_or_null:\n"
		"   output buffer = '%s'\n"
		"   remaining input = '%s'\n", buffer, in);
#endif

  if (out > buf_end)
    return NULL;

  while (*in)
    {
      switch (*in)
	{
	case '.':
	  if (in[1] == '/')
	    {
	      /* Skip any ./
	         If it is at the beginning the output a @. */

	      if (in == start && !path)
		{
		  if (out + 2 > buf_end)
		    return NULL;

		  *out++ = '@';
		  *out++ = '.';

		  last_out_slash = out;
		  in += 2;
		  last_slash = in;
		}
	      else if (in[-1] == '/')
		{
		  /* only skip ./ if it was preceded by a /
		     since we don't want to trim foo. into foo */
		  in += 2;
		  last_slash = in;
		}
	      else
		{
		  *out++ = '/';
		  in++;
		}
	    }
	  else if (in > start && in[-1] == '/' && in[1] == '\0')
	    {
	      /* Skip any /. at the end of a filename
	         The check against start ensures we don't read earlier
	         than the start of the input buffer */
	      in++;
	    }
	  else if (in[1] == '.' && in[2] == '/')
	    {
	      /* Change ../ to ^. */
	      in += 3;
	      last_slash = in;

	      if (out + 2 > buf_end)
		return NULL;

	      *out++ = '^';
	      *out++ = '.';
	      last_out_slash = out;
	    }
	  else if (in > start && in[-1] == '/' && in[1] == '.'
		   && in[2] == '\0')
	    {
	      /* Change .. to ^ when at the end of a filename
	         The check against start ensures we don't read earlier
	         than the start of the input buffer */

	      in += 2;

	      if (previous_slash == start)
		{
		  /* Make a special case for foo/.. as RISC OS will canonicalise
		     this to an empty string, and then complain about the lack
		     of filename */

		  out = buffer;
		  *out++ = '@';
		  *out = '\0';
		}
	      else
		{
		  if (out + 1 > buf_end)
		    return NULL;

		  *out++ = '^';
		}
	    }
	  else
	    {
	      /* Just a . as part of a filename */
	      last_dot = in++;
	      last_out_dot = out;
	      *out++ = '/';

	      if (out > buf_end)
		return NULL;
	    }

	  break;

	case '/':
	  /* Strip out any multiple slashes */
	  while (in[1] == '/')
	    in++;

	  previous_slash = last_slash;

	  last_slash = ++in;

	  /* Copy as a '.', unless nothing else has been output yet or there
	     is already a . */
	  if (out > buffer && out[-1] != '.')
	    *out++ = '.';

	  last_out_slash = out;

	  if (out > buf_end)
	    return NULL;

	  break;

	default:
	  /* Translate all other characters as appropriate */
	  *out++ = __filename_char_map[(unsigned char) *in++];

	  if (out > buf_end)
	    return NULL;

	}
    }

  /* If the input ended in any / then remove them. */
  while (out > buffer + 1 && out[-1] == '.')
    out--;

  if (out == buffer && !path)
    {
      /* Translate an empty filename into the current dir (FIXME: do we really want this ?) */
      if (out + 1 > buf_end)
	return NULL;

      *out++ = '@';
    }

#ifdef DEBUG
  *out = '\0';
  debug_printf ("before ,xyz and suffix checking '%s'\n", buffer);
#endif

#ifndef __TARGET_SOLOADER__
  /* Use MimeMap to find a filetype to match the filename
     extension.  e.g.  file.html -> 0xfaf */
  if (!(flags & __RISCOSIFY_FILETYPE_NOT_SET)
      && filetype != NULL && last_dot != NULL)
    {
      _kernel_swi_regs regs;

      regs.r[0] = MMM_TYPE_DOT_EXTN;	/* Input extension */
      regs.r[1] = (long) last_dot;
      regs.r[2] = MMM_TYPE_RISCOS;	/* Output filetype */

      /* If there's an error, then the filetype will remain
         __RISCOSIFY_FILETYPE_NOT_FOUND.  */
#ifdef __TARGET_SCL__
      if (!_kernel_swi (MimeMap_Translate, &regs, &regs))
#else
      /* Avoid _kernel_swi if possible because it can corrupt errno and
         _kernel_last_oserror if the mimemap translation fails.
         __os_swi is not available in the SCL. */
      if (!__os_swi (MimeMap_Translate, regs.r))
#endif
	*filetype = regs.r[3];

    }
#endif

#ifndef __TARGET_SOLOADER__
  /* Check if we have "blabla,xyz" as filename where `xyz' is a
     valid 12 bit hex number.  Only perform the check if
     __RISCOSIFY_FILETYPE_EXT is set and xyz != 0xfff.  However
     relax the latter condition if __RISCOSIFY_FILETYPE_FFF_EXT is
     set.  */
  if (out - last_out_slash > 4 && out[-4] == ','
      && isxdigit (out[-3]) && isxdigit (out[-2]) && isxdigit (out[-1])
      && (flags & __RISCOSIFY_FILETYPE_EXT))
    {
      int ftype = 0;
      int i;

      for (i = -3; i < 0; i++)
	ftype = (ftype << 4) + ((out[i] <= '9')
				? (out[i] - '0')
				: (toupper (out[i]) - 'A' + 10));

      if (filetype != NULL
	  && (ftype != 0xFFF || (flags & __RISCOSIFY_FILETYPE_FFF_EXT)))
	*filetype = ftype;

      /* Remove the comma and extension from the output */
      out -= 4;
    }
#endif

  /* Terminate the output */
  *out = '\0';

  /* See if the leafname suffix matches a known suffix */
  if (last_out_dot && !(flags & __RISCOSIFY_NO_SUFFIX))
    suffix = __sfixfind (last_out_dot + 1, strlen (last_out_dot + 1));
  else
    suffix = NULL;

  if (suffix)
    {
      /* Suffix matches, so do suffix swapping */
      int leaf_len;

      /* Rewind output buffer to just after the last directory separator copied.
         Could be the start of the buffer if no dir separators encountered */
      out = last_out_slash;

      /* Copy the new suffix */
      while (*suffix)
	{
	  *out++ = *suffix++;

	  if (out > buf_end)
	    return NULL;
	}

#ifndef __TARGET_SOLOADER__
      if (create_dir)
	{
	  /* Terminate the output buffer */
	  *out = '\0';

	  /* Create the directory if it doesn't exist.  */
	  int objtype;
	  if (!SWI_OS_File_ReadCatInfo (buffer, &objtype, NULL, NULL, NULL, NULL)
	      && !objtype)
	    {
	      _kernel_swi_regs regs;
	      regs.r[0] = 8;
	      regs.r[1] = (long) buffer;
	      regs.r[4] = 0;	/* Default number of entries in dir.  */
	      _kernel_swi (OS_File, &regs, &regs);
	    }
	}
#endif

      /* Copy the leafname without suffix */
      leaf_len = last_out_dot - last_out_slash;

      if (out + leaf_len > buf_end)
	return NULL;

      *out++ = '.';

      while (leaf_len-- > 0)
	*out++ = __filename_char_map[(unsigned char) *last_slash++];
    }

  /* Terminate the output */
  *out = '\0';

#ifdef DEBUG
  debug_printf ("final output '%s'\n", buffer);
#endif

  return out;
}


/* Guess if filename is RISC OS or unix, then call copy_or_null or
   translate_or null as appropriate. Used for path vars and relative filenames
   where we cannot reliably work out what format it is in.
   If path is nonzero then buffer will already contain a path var, eg gcc:
   otherwise buffer will contain the chars upto the first dot or slash.
   Return NULL if buffer overflowed. */
static char *
guess_or_null (int create_dir, int flags, char *buffer, const char *buf_end,
	       int *filetype, char *out, const char *in, const char *name,
	       int path)
{
  char *last;
  char *penultimate;
  const char *orig_in = name;
  char *orig_out = buffer;
  char *slash = NULL;

#ifdef DEBUG
  *out = '\0';
  debug_printf ("-- guess_or_null:\n"
		"   output buffer = '%s'\n"
		"   remaining input = '%s'\n", buffer, in);
#endif

  /* Set orig_* to the bit after the pathvar, if any */
  if (path)
    {
      orig_out = out;
      orig_in = in;
    }

  /* Need to guess it could be a RISC OS filename ?  */
  if ((flags & __RISCOSIFY_STRICT_UNIX_SPECS) != 0)
    return translate_or_null (create_dir, flags, buffer, buf_end, filetype,
			      orig_out, orig_in, path);

  /* Find the last two dots or slashes */
  last = NULL;
  penultimate = NULL;
  while (*in)
    {
      if (*in == '.')
	{
	  penultimate = last;
	  last = out;
	}
      else if (*in == '/')
	{
	  penultimate = last;
	  last = out;
	  slash = out;
	}

      *out++ = *in++;

      if (out > buf_end)
	return NULL;
    }

  *out = '\0';

  if (last == NULL)
    {
      /* Just a leafname, so let translate_or_null deal with ,xyz etc. */
      return translate_or_null (create_dir, flags, buffer, buf_end,
				filetype, orig_out, orig_in, path);
    }

  if (penultimate == NULL)
    penultimate = orig_out - 1;

  if (*last == '.')
    {
      *last = '\0';

      if (slash == NULL
	  && __sfixfind (penultimate + 1, strlen (penultimate + 1)))
	{
	  /* foo.c.getopt so it's probably RISC OS, unless there was
	     a slash earlier in the input */
	  return copy_or_null (orig_out, orig_in, buf_end);
	}

      if (__sfixfind (last + 1, strlen (last + 1)))
	{
	  /* foo/getopt.c so it's probably unix */
	  return translate_or_null (create_dir, flags, buffer, buf_end,
				    filetype, orig_out, orig_in, path);
	}
    }
  else
    {
      *last = '\0';

      if (__sfixfind (penultimate + 1, strlen (penultimate + 1)))
	{
	  /* foo/c/getopt so it's probably unix */
	  return translate_or_null (create_dir, flags, buffer,
				    buf_end, filetype, orig_out, orig_in,
				    path);
	}

      if (__sfixfind (last + 1, strlen (last + 1)))
	{
	  /* foo.getopt/c so it's probably RISC OS */
	  return copy_or_null (orig_out, orig_in, buf_end);
	}
    }

  /* No known suffix found at the start or end.
     Could be RISC OS or unix, and there's no reliable way to tell.
     If it starts with a path var then assume it's RISC OS format, otherwise
     unix */
  if (path)
    return copy_or_null (orig_out, orig_in, buf_end);

  return translate_or_null (create_dir, flags, buffer, buf_end, filetype,
			    orig_out, orig_in, path);
}


/* Canonicalise a Unix path of the form
   /foo///bar/../baz/.////
   into a standard Unix path
   /foo/baz/
   Returns NULL if buffer overloaded

   We must cope with things like:
   ../foo     = ../foo
   ./foo/bar  = foo/bar
   ..foo      = ..foo
   /../foo    = /foo
   foo/bar/.  = foo/bar
   foo./bar/  = foo./bar/

   Currently this is only used for absolute Unix paths, though it can cope
   with relative paths too.
 */

static char *
canonicalise_unix_path (char *to, const char *from, const char *buf_end)
{
  char *out = to;
  const char *in = from;
  char *current_slash = to;	/* the current directory terminating slash */
  char *previous_slash = NULL;	/* the parent directory terminating slash */

#ifdef DEBUG
  debug_printf ("-- canonicalise_unix_path:\n"
		"   input = '%s'\n", from);
#endif
  /* sanity check our input parameters */
  if (!from || !to || !buf_end)
    return NULL;


  while (out <= buf_end && in[0])
    {
      switch (in[0])
	{
	case '/':
	  /* copy one slash, but not multiple */
	  previous_slash = current_slash;
	  current_slash = out;
	  *out++ = *in++;
	  while (*in == '/')
	    in++;
	  break;
	case '.':
	  if (in == from || in[-1] == '/')
	    {
	      /* if this is an initial '.' or was preceded by a '/' */
	      if (in[1] == '.' && (in[2] == '/' || in[2] == '\0'))
		{
		  /* if '..', rewind to previous slash found if there was one */
		  if (previous_slash)
		    {
		      out = previous_slash;
		      in += 2;
		    }
		  else
		    {
		      /* if there wasn't a previous slash just copy '..' verbatim
		       * (this path started with '..' )
		       */
		      *out++ = '.';
		      if (out <= buf_end)
			*out++ = '.';
		      else
			return NULL;
		      in += 2;
		    }
		}
	      else if (in[1] == '/')
		/* ignore any './' */
		in += 2;
	      else if (in[1] == '\0' && in[-1] == '/' && in != from)
		/* ignore any terminating '/.' */
		in++;
	      else
		/* otherwise it's just a '.' at the start of a name */
		*out++ = *in++;
	    }
	  else
	    /* otherwise it's just a '.' in the middle of a name */
	    *out++ = *in++;

	  break;
	default:
	  *out++ = *in++;
	  break;
	}
    }
  *out = '\0';
#ifdef DEBUG
  debug_printf ("output buffer = '%s'\n", to);
#endif
  if (out <= buf_end)
    return NULL;
  else
    return to;
}

char *
__riscosify (const char *name, int create_dir,
	     int flags, char *buffer, size_t buf_len, int *filetype);

/* Call __riscosify() with __riscosfy_control as the flags.
   Place adjacent to __riscosify() to help with cache hits.  */
char *
__riscosify_std (const char *name, int create_dir,
		 char *buffer, size_t buf_len, int *filetype)
{
  return __riscosify (name, create_dir, __get_riscosify_control (), buffer,
		      buf_len, filetype);
}

char *
__riscosify (const char *name, int create_dir,
	     int flags, char *buffer, size_t buf_len, int *filetype)
{
  const char *buf_end = buffer + buf_len - 1;	/* The last char in the buffer. */
  const char *in = name;
  char *out = buffer;

#if __UNIXLIB_PARANOID
  assert ((flags & ~__RISCOSIFY_MASK) == 0);
#endif

  /* Check for silly user input and return errors appropriately.  */
  if (!name || !buffer || !buf_len)
    return NULL;

#ifdef DEBUG
  debug_printf ("-- __riscosify: '%s'\n", name);
#endif

  if (filetype != NULL)
    *filetype = __RISCOSIFY_FILETYPE_NOTFOUND;

  /* We can safely make the assumption that here is space for at least
     one character in the buffer.  */

  /* If the user has specified this flag, then we only need to copy
     the filename to the destination buffer.  */
  if (flags & __RISCOSIFY_NO_PROCESS)
    return copy_or_null (buffer, name, buf_end);

  /* catch ./path:file.h */
  if (in[0] == '.' && in[1] == '/' && strchr (in, ':'))
    in++;

  /* We can tell quite a lot from the first character of the path */
  switch (*in)
    {
    case '~':
      /* Reference to home directory.  We need to handle:

         ~/file.c
         ~<user>/somefile.c

         We skip over the user part, since there's nothing we can
         do about that, and just use our value for $HOME.  We then
         make a new buffer containing a concatentaion of the path
         passed in and our HOME value, and call __riscosify again
         (but making sure we don't infinitely loop).
       */
      {
	char new_buffer[MAXPATHLEN];
	const char *home = __ul64_home ();
	strcpy (new_buffer, home);

	do
	  {
	    in++;
	  }
	while (*in != '\0' && *in != '/');

	if (new_buffer[0] != '~')
	  {
	    strcat (new_buffer, in);
	    return __riscosify (new_buffer, create_dir, flags, buffer,
				buf_len, filetype);
	  }
      }
      /* Fall through */

    case '/':
      /* The directory separator must be a slash. Could be an absolute unix
         pathname, or a unixified RISC OS path.

         /idefs::hd/$/fred/preset/s  idefs::hd.$.fred.preset.s
         /idefs::hd/$/fred/preset.s  idefs::hd.$.fred.s.preset
         /Sunfish#192.0.2.50::/home/joty.$/projects/gccsdk/index.html
				     Sunfish#192.0.2.50::/home/joty.$.projects.gccsdk.index/html
	 /HostFS:$/jo.html	     HostFS:$.jo/html
         /arm/rname.c                $.arm.c.rname
         /lib/something/cc           %.something.cc
         /<GCC$Dir>/config           <GCC$Dir>.config
         /gcc:/libgcc.o              gcc:o.libgcc
         /gcc:libgcc.o               gcc:o.libgcc
         /dev/tty                    tty:
         /usr/xxx                    xxx
         /xxx                        xxx */

      in++;			/* Skip the '/' */

      if (*in == '<')
	{
	  /* Must be a sysvar
	     /<GCC$dir>/something */

	  /* Copy everything upto the '>' */
	  do
	    {
	      *out++ = *in++;

	      if (out > buf_end)
		return NULL;
	    }
	  while (*in && *in != '>');

	  if (out + 2 > buf_end)
	    return NULL;

	  if (*in)
	    *out++ = *in++;	/* Copy the '>' */

	  if (*in == '/')
	    {
	      /* Copy the first '/' as a '.' */
	      *out++ = '.';
	      in++;
	    }

	  /* Skip any further '/'s */
	  while (*in == '/')
	    in++;
	}
      else
	{
	  /* Not a sysvar */

	  /* Copy everything upto the first '.', '/', ':' or '#' */
	  while (*in && *in != ':' && *in != '.' && *in != '/' && *in != '#')
	    {
	      *out++ = *in++;

	      if (out > buf_end)
		return NULL;
	    }

	  if (*in == '#')
	    {
	      /* Copy any fileswitch special field (i.e. the nfs#station::)
		 These start with '#' and can contain almost anything
		 except ':'.  */
	      while (*in && *in != ':')
		{
		  *out++ = *in++;

		  if (out > buf_end)
		    return NULL;
		}
	    }

	  if (*in == ':')
	    {
	      /* Must be a RISC OS filing system or path variable.
	         /idefs:: or /gcc: */

	      *out++ = *in++;	/* Copy the ':' */

	      if (out > buf_end)
		return NULL;

	      if (*in == ':')
		{
		  /* Must be a filing system
		     /idefs:: */

		  /* Copy discname which is all chars upto the next '$'
		     /idefs::hd/$
                     /idefs::/foo/bar.$ */
		  while (*in && *in != '$')
		    {
		      *out++ = *in++;

		      if (out > buf_end)
			return NULL;
		    }
		  if (*in == '$')
		    {
		      /* Ensure a dot before '$'.  */
		      if (out[-1] == '/')
			out[-1] = '.';

		      *out++ = *in++;
		      if (out + 1 > buf_end)
			return NULL;

		      if (*in == '/')
			{
			  *out++ = '.';

			  /* Skip any further '/'s */
			  while (*in == '/')
			    in++;
			}
		    }
		}
	      else if (*in == '$')
		{
		  /* Cases: /HostFS:          -> HostFS:
			    /HostFS:$         -> HostFS:$
			    /HostFS:$.jo.html -> HostFS:$.jo/html
			    /HostFS:$/jo.html -> HostFS:$.jo/html */
		  *out++ = *in++;
		  if (out + 1 > buf_end)
		    return NULL;
		  if (*in)
		    {
		      *out++ = '.';
		      if (*in == '.')
			in++;
		      else
			/* Skip any '/'s */
			while (*in == '/')
			  in++;
		    }
		}
	      else if (*in == '/')
		{
		  /* Must be a path var followed by a '/'
		     /gcc:/ */

		  /* Skip any '/'s */
		  do
		    {
		      in++;
		    }
		  while (*in == '/');
		}
	      else
		{
		  /* Must be a path var
		     /gcc:something
		     in already points to the char after the ':', so nothing
		     else needs copying.  */
		}
	    }
	  else
	    {
	      /* Must be an absolute unix path, with no RISC OS specific
		 parts, so rewind to just after the initial slashes but
		 we must first attempt to convert the path we've been
		 given into a canonicalised Unix path because otherwise
		 we won't match on things like /home/foo/../riscos//./env/
		 */
	      int matched = 0;
	      char canonical_name[MAXPATHLEN];
	      const char *cname = canonical_name;

	      canonicalise_unix_path (canonical_name, name,
				      canonical_name + MAXPATHLEN);

	      out = buffer;

	      /* Check for special paths, starting with /dev  */
	      if (cname[0] == '/' && cname[1] == 'd' && cname[2] == 'e'
		  && cname[3] == 'v' && cname[4] == '/' && cname[5] != '\0')
		{
		  /* Copy to the destination string as {device}:  e.g. /dev/tty
		     would become tty:.  */

		  out = copy_or_null (out, cname + 5, buf_end);
		  if (out == NULL)
		    return NULL;

		  *out++ = ':';
		  *out = '\0';
		  return out;
		}

	      /* /usr/xxx and /var/xxx. Try matching xxx segment.  */
	      if (cname[0] == '/'
		  && ((cname[1] == 'u' && cname[2] == 's')
		      || (cname[1] == 'v' && cname[2] == 'a'))
		  && cname[3] == 'r' && cname[4] == '/')
		{
		  cname += 5;
		  switch (sdirseg (&cname, &out, buf_end))
		    {
		      case sdirseg_buf_too_small:
			return NULL;
			break;

		      case sdirseg_match:
			matched = 1;

			/* If the matched segment was a path var then consume
			   any '/'s in the input to prevent them being copied
			   as '.'s (so we don't end up with gcc:.foo).  */
			if (out > buffer && out[-1] == ':')
			  while (*cname == '/')
			    cname++;
			break;

		      default:
			cname -= 5;
		    }
		}

/* FIXME: the __ELF__ test is a hack until our ELF build system @ GCCSDK 4
   has a concept of RO_ENV. Also test/filename/testriscosify.c needs to be
   changed.  */
#ifndef __ELF__
	      /* /home/riscos/env/xxx. (or configured path) Try matching
	         xxx segment.  */
	      else if (strncmp (cname, RO_ENV, sizeof (RO_ENV) - 1) == 0)
		{
		  cname += sizeof (RO_ENV) - 1;
		  while (*cname && *cname == '/')
		    cname++;

		  switch (sdirseg (&cname, &out, buf_end))
		    {
		    case sdirseg_buf_too_small:
		      return NULL;
		      break;

		    case sdirseg_match:
		      matched = 1;
		      /* If the matched segment was a path var then consume
			 any '/'s in the input to prevent them being copied
			 as '.'s (so we don't end up with gcc:.foo) */
		      if (out > buffer && out[-1] == ':')
			while (*cname == '/')
			  cname++;

		      break;

		    default:
		      /* Not matched anything in the list of UnixFS$...
			 variables.  So we assume this is a vanilla
			 path: translate /home/riscos/env/foo/bar/baz
			 into foo:bar.baz  */
		      if (*cname == '\0')
			{
			  /* this is just /home/riscos/env/
			     so just return it as $.home.riscos.env because
			     it doesn't match anything under RISC OS.
			     Wind back to after the first / in /home/... */
			  cname -= sizeof (RO_ENV) - 1;
			  break;
			}

		      /* copy the name of the path variable */
		      while (*cname != '/' && *cname != '\0')
			*out++ = *cname++;

		      /* ...consuming any trailing slash */
		      if (*cname == '/')
			cname++;

		      /* this is a RISC OS path */
		      *out++ = ':';
		      matched = 1;
		    }
		}
#endif

	      if (!matched)
		{
		  cname++;
		  /* Try matching against a user defined /xxx.  */
		  switch (sdirseg (&cname, &out, buf_end))
		    {
		      case sdirseg_buf_too_small:
			return NULL;
			break;

		      case sdirseg_match:
			matched = 1;

			/* If the matched segment was a path var then consume
			   any '/'s in the input to prevent them being copied
			   as '.'s (so we don't end up with gcc:.foo) */
			if (out > buffer && out[-1] == ':')
			  while (*cname == '/')
			    cname++;

			break;

		      case sdirseg_no_match:
			cname--;
			break;
		    }
		}

	      if (!matched)
		{
		  /* Did not match any user defined paths, so treat it as a
		     path on the current filing system */
		  if (out + 2 > buf_end)
		    return NULL;

		  *out++ = '$';
		  *out++ = '.';
		}

	      return translate_or_null (create_dir, flags, buffer, buf_end,
					filetype, out, cname, 1);
	    }
	}

      /* By this point, all RISC OS specific parts have been copied across and
         all that remains is something like foo/bar/baz.c */
      return translate_or_null (create_dir, flags, buffer, buf_end, filetype,
				out, in, 1);
      break;


    case '$':
    case '@':
    case '%':
    case '\\':
    case '&':
    case '^':
      /* If the path starts with any of these chars then it must be a RISC OS
         path, although the dot and slashed could still be reversed.

         $.work.!unixlib  $.work.!unixlib
         $/work/!unixlib  $.work.!unixlib
         %.cc1            %.cc1
         @/fred.c         @.c.fred
         ^.fred           ^.fred */

      *out++ = *in++;		/* We know there is at least one space in the buffer */

      /* The next char must be the directory separator */
      if (*in == '/')
	{
	  if (out > buf_end)
	    return NULL;

	  /* Copy the / as . */
	  *out++ = '.';
	  in++;

	  /* Skip any further '/'s */
	  while (*in == '/')
	    in++;

	  return translate_or_null (create_dir, flags, buffer, buf_end,
				    filetype, out, in, 1);
	}

      return copy_or_null (out, in, buf_end);
      break;


    case '<':
      /* The path must start with a sysvar.
         <GCC$dir>.cc.smart    <GCC$dir>.cc.smart
         <GCC$dir>.smart.cc    <GCC$Dir>.smart.cc
         <GCC$Dir>/cc.smart    <GCC$Dir>.cc/smart
         <GCC$Dir>/smart.cc    <GCC$Dir>.cc.smart */

      /* Copy everything upto the '>' */
      do
	{
	  *out++ = *in++;

	  if (out > buf_end)
	    return NULL;
	}
      while (*in && *in != '>');

      /* Copy the '>' */
      if (*in)
	*out++ = *in++;

      if (out > buf_end)
	return NULL;

      /* The char following the '>' must be the directory separator */
      if (*in == '/')
	{
	  /* Copy the / as . */
	  *out++ = '.';
	  in++;

	  /* Skip any further '/'s */
	  while (*in == '/')
	    in++;

	  return translate_or_null (create_dir, flags, buffer, buf_end,
				    filetype, out, in, 1);
	}
      else
	return copy_or_null (out, in, buf_end);

      break;


    case '.':
      /* RISC OS paths never start with a '.'
         ./././c.cool      @.c/cool
         ../../../c.cool   ^.^.^.c/cool
         ../cool.c         ^.c.cool
         .plan             /plan
         ..plan            //plan
         .                 @
         ..                ^
         ./.               @
         ./..              @.^ */

      if (in[1] == '\0')
	{
	  if (out + 1 > buf_end)
	    return NULL;

	  *out++ = '@';
	  *out = '\0';

	  return out;
	}

      if (in[1] == '.' && in[2] == '\0')
	{
	  if (out + 1 > buf_end)
	    return NULL;

	  *out++ = '^';
	  *out = '\0';

	  return out;
	}

      return translate_or_null (create_dir, flags, buffer, buf_end, filetype,
				out, in, 0);

      break;


    default:
      /* Could be an absolute RISC OS path or a path variable, possibly with
         '.' and '/' swapped. Otherwise it must be a relative path, but could
         be either RISC OS or unix, with no easy way to tell.

         idefs::hd.$.fred/c  idefs::hd.$.fred/c
         idefs::hd/$/fred.c  idefs::hd.$.c.fred
         gcc:c.getopt        gcc:c.getopt
         gcc:/getopt.c       gcc:c.getopt
         fred.c              c.fred
         c/fred              c.fred
         c.fred              c.fred
         gcc:/getopt.c       gcc:getopt.c
         gcc:o.libgcc        gcc:o.libgcc
         gcc:/o.libgcc       gcc:o/libgcc
         foo/bar.s           foo.s.bar
         foo/bar.php         foo.bar/php
         gcc-2.7.2.2.tar.gz  gcc-2/7/2/2/tar/gz */

      /* Copy everything upto the first '.', '/', ':' or '#' */
      while (*in && *in != ':' && *in != '.' && *in != '/' && *in != '#')
	{
	  *out++ = *in++;

	  if (out > buf_end)
	    return NULL;
	}

      if (*in == '#')
	{
	  /* Copy any fileswitch special field (i.e. the nfs#station::)
	     These start with '#' and can contain almost anything except ':' */
	  while (*in && *in != ':')
	    {
	      *out++ = *in++;

	      if (out > buf_end)
		return NULL;
	    }
	}

      if (*in == ':')
	{
	  /* Must be a RISC OS filing system or path var
	     idefs:: or gcc: */

	  *out++ = *in++;	/* Copy the ':' */
	  if (out > buf_end)
	    return NULL;

	  if (*in == ':')
	    {
	      /* Must be a filing system */
	      while (*in && *in != '.' && *in != '/')
		{
		  *out++ = *in++;	/* Copy the discname */

		  if (out > buf_end)
		    return NULL;
		}


	      /* Copy the .$ or /$ as .$ */
	      if (in[0] && in[1] == '$')
		{
		  /* Add the .$ */
		  if (out + 2 > buf_end)
		    return NULL;

		  *out++ = '.';
		  *out++ = '$';
		  in += 2;
		}

	      if (*in == '/')
		{
		  /* Copy the / as . */
		  *out++ = '.';
		  in++;

		  /* Skip any further '/'s */
		  while (*in == '/')
		    in++;

		  return translate_or_null (create_dir, flags, buffer,
					    buf_end, filetype, out, in, 1);
		}

	      return copy_or_null (out, in, buf_end);
	    }
	  else
	    {
	      /* Must be a path var
	         gcc: */
	      if (*in == '/')
		{
		  /* Skip over any '/'s */
		  while (*in == '/')
		    in++;

		  return translate_or_null (create_dir, flags, buffer,
					    buf_end, filetype, out, in, 1);
		}
	      else
		{
		  /* Have to make a guess if it is RISC OS or unix format */
		  return guess_or_null (create_dir, flags, buffer, buf_end,
					filetype, out, in, name, 1);

		}
	    }
	}
      else
	{
	  /* Must be a relative path. Could be RISC OS or unix, and there's no
	     reliable way to tell. */
	  return guess_or_null (create_dir, flags, buffer, buf_end, filetype,
				out, in, name, 0);
	}
    }

  /* Not reached */
  return NULL;
}


#ifdef __TARGET_SCL__

static const char __sfix_default[] = "c:cc:f:h:i:ii:l:o:p:s:y";

char *
__riscosify_scl (const char *name, int create_dir)
{
  static char buffer1[MAXPATHLEN];
  static char buffer2[MAXPATHLEN];
  static int current = 0;
  static int riscosify_init = 0;

  if (!riscosify_init)
    {
#ifdef __TARGET_SOLOADER__
      riscosify_init = 1;
      __sfixinit (__sfix_default);
#else
      char buf[1024];
      char *cmd, *start;

      cmd = _kernel_command_string ();

      /* skip leading spaces */
      while (*cmd == ' ')
	cmd++;

      /* Take the program name, and find the last segment of it
         (using '.' and ':' as the directory separator).  */
      start = cmd;
      while (*cmd && *cmd != ' ')
	{
	  if (*cmd == '.' || *cmd == ':')
	    start = cmd + 1;
	  cmd++;
	}

      snprintf (buf, sizeof buf, "UnixEnv$%.*s$sfix", cmd - start, start);
      cmd = getenv (buf);

      riscosify_init = 1;
      __sdirinit ();
      __sfixinit (cmd ? cmd : __sfix_default);
#endif
    }

  /* Swap between two static buffers to allow the rename call to work */
  current = 1 - current;

  __riscosify (name, create_dir, __get_riscosify_control (),
	       current ? buffer1 : buffer2, sizeof (buffer1), NULL);
  return current ? buffer1 : buffer2;
}

#endif


/* This table has
   '/' and '.' transposed
   '?' and '#' transposed
   0x01 - ' ', '"', '$', '%', '&', ':', '@', '\\', '^', '_', '|' and 0x7F
   all mapping to '_'.

   If you are going to be a stickler for following the PRM or you use L format
   or earlier then 0x80 - 0xFF should also be mapped to '_'.

   This table is also used to 'go the other way' in dirent and getcwd.  */

const char __filename_char_map[256] = {
  0x00, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
  0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
  0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
  0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
  0xA0, 0x21, 0x5F, 0x3F, 0x5F, 0x5F, 0x5F, 0x27,
  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2F, 0x2E,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x5F, 0x3B, 0x3C, 0x3D, 0x3E, 0x23,
  0x5F, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5A, 0x5B, 0x5F, 0x5D, 0x5F, 0x5F,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7A, 0x7B, 0x5F, 0x7D, 0x7E, 0x5F,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
  0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
  0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
  0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
  0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
  0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
  0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
  0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
