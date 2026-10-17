/* UnixLib internal function declarations.
   Copyright (c) 2002-2014 UnixLib Developers.  */

#ifndef __UNIXLIB_LOCAL_H
#define __UNIXLIB_LOCAL_H 1

#include <stddef.h>

/* UnixLib 64: the file name translation of UnixLib (riscos64/unixlib), from libunixlib/include/unixlib/local.h */
#ifdef __cplusplus
extern "C" {
#endif

extern void __sfixinit (const char *list);

/* This bit map controls how riscosify[_std]() processes filenames.
   Flags value for internal conversions (in this thread?) of UnixLib.
   The default value is 0 unless user program overrules this by
   defining an explicit instance of __riscosify_control of its own.
     Conversion on
     Do perform suffix processing
     Don't create suffix dirs (open/rename functions DO create dirs).
     Don't look at/react on ",xyz" filetype extensions.
     Set filetype according to MimeMap.  */

extern int __riscosify_control; /* Note: this is a weak symbol.  */

/* Gets the __riscosify_control value which can be defined by
   the global variable __riscosify_control in the user program.
   Returns a copy of __riscosify_control_internal (with a default
   value of 0) when __riscosify_control is not defined.  */
extern int __get_riscosify_control (void);

/* Sets the __riscosify_control value when it's defined.
   Otherwise __riscosify_control_internal gets written.  */
extern void __set_riscosify_control (int __riscosify_flags);

/* Bits 1 - 5 (incl), 7 and 15 - 31 (incl) of __riscosify_control are not
   allocated.  */

/* When set, __riscosify[_std]() won't try to recognise RISC OS filenames
   but assume that all filenames are Unix based.  */
#define __RISCOSIFY_STRICT_UNIX_SPECS   0x0001
/* Don't actually process filenames, copy verbatim into the output buffer.
   When set, this has priority over all other bits.  */
#define __RISCOSIFY_NO_PROCESS		0x0040

/* Disable suffix swapping (suffix swapping is implicitly disabled for
   directories).  */
#define __RISCOSIFY_NO_SUFFIX		0x0100

/* Disable checking if section between two '/'s exists as directory.  */
#define __RISCOSIFY_DONT_CHECK_DIR	0x0200

/* accept only if dir found above is in the suffix list.  */
#define __RISCOSIFY_CHECK_DIR_IS_SUFFIX	0x0400

/* Disable reverse suffix swapping (RISC OS -> Unix).  */
#define __RISCOSIFY_NO_REVERSE_SUFFIX	0x0800

/* Support file type extensions: ",xyz" at the end of the Unix filename
   means that the file on creation gets the filetype xyz (3 hex nibbles).
   If the filetype extension is not specified or this option isn't set,
   the filetype for freshly created files is `Text' (0xfff).  */
#define __RISCOSIFY_FILETYPE_EXT    	0x1000

/* When set and __RISCOSIFY_FILETYPE_EXT is set, RISC OS text files
   will get a ",fff" extension when translated to their Unix form.  */
#define __RISCOSIFY_FILETYPE_FFF_EXT	0x2000

/* If the filename has a unix-style extension, look up the extension
   using RISC OS MimeMap SWIs and set a filetype.  If the filetype
   could not be found, use 0xFFF.
   This is turned on by default - set this bit to turn it off. */
#define __RISCOSIFY_FILETYPE_NOT_SET    0x4000

/* Mask of acceptable values. Keep other bits zero. Checks may be made.  */
#define __RISCOSIFY_MASK                0x7F41

/* Value indicating that __riscosify[_std] didn't see a filetype extension
   in its argument __name or that it wasn't instructed to look for one.  */
#define __RISCOSIFY_FILETYPE_NOTFOUND   -1

/* Value indicating for __unixify that there is no filetype (e.g. a
   directory) even when __RISCOSIFY_FILETYPE_EXT is specified as one of
   the flag bits.  */
#define __RISCOSIFY_FILETYPE_NOTSPECIFIED -1

/* Convert Unix filenames/pathnames to RISC OS format creating the final
   directory if necessary and __create_dir is non-zero.
   Pass RISC OS pathnames through unchanged (only when
   __RISCOSIFY_STRICT_UNIX_SPECS is clear).
   Returns pointer to terminating '\0' in buffer,
   or NULL if there was a problem.  */
extern char *__riscosify (const char *__name, int __create_dir,
			  int __riscosify_flags,
			  char *__buffer, size_t __buf_len,
			  int *__filetype);

/* Calls __riscosify() with __riscosify_flags equal to
   __riscosify_control.  */
extern char *__riscosify_std (const char *__name, int __create_dir,
			      char *__buffer, size_t __buf_len,
			      int *__filetype);

/* Convert __name into a Unix style pathname and store in __buffer.
   If buffer is non-NULL then it is at least __buf_len long.  If __buffer
   is NULL then a buffer is malloc'ed to store the Unix pathname.
   Return a pointer to the result or NULL on failure.  */
extern char *__unixify (const char *__name, int __unixify_flags,
			char *__buffer, size_t __buf_len, int __filetype);

extern char *__unixify_std (const char *__name,
			    char *__buffer, size_t __buf_len,
			    int __filetype);
extern char *__unixify_ext (const char *name, char *buffer, size_t buflen,
                            int filetype, const char *ext);

#ifdef __cplusplus
}
#endif

#endif
