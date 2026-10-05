/*
 * File taken from glibc 2.2.5.
 * Following changes were made:
 *  - SCL poison added.
 */
#ifdef __TARGET_SCL__
#  error "SCL build should not use (L)GPL code."
#endif


/* Copyright (C) 1996, 1997, 1998, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

/* Internal glibc-style alias; defined in string/strnlen.c but not declared in any header.  */
extern size_t __strnlen (const char *, size_t);

char *
__strndup (s, n)
     const char *s;
     size_t n;
{
  size_t len = __strnlen (s, n);
  char *new = (char *) malloc (len + 1);

  if (new == NULL)
    return NULL;

  new[len] = '\0';
  return (char *) memcpy (new, s, len);
}
weak_alias (__strndup, strndup)
