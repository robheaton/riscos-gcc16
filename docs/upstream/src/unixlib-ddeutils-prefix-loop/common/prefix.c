/* __get_dde_prefix ()
 * Copyright (c) 2000-2012 UnixLib Developers
 */

#include <stdlib.h>
#include <string.h>

#include <unixlib/local.h>
#include <internal/os.h>
#include <internal/unix.h>

/* Returns malloced block of memory holding DDEUtils_Prefix non zero-length
   string value or NULL when there isn't one set.  Caller needs to free
   memory after use.  */
const char *
__get_dde_prefix (void)
{
  char *result;

  /* Try to fetch the prefix of the currently active object.  */
  const char *prefix;
  if (SWI_DDEUtils_ReadPrefix (0, &prefix) == NULL)
    {
      /* DDEUtils_ReadPrefix is supported.  Was there a prefix set ? */
      if (prefix == NULL)
        return NULL;

      /* Returned string can be control char terminated.  */
      const char *end_prefix;
      for (end_prefix = prefix; (unsigned char)*end_prefix >= ' '; ++end_prefix)
        /* */;
      /* Filter out zero length results.  */
      size_t size = end_prefix - prefix;
      if (size == 0)
        return NULL;
      if ((result = (char *)malloc (size + 1)) == NULL)
        return NULL;
      memcpy (result, prefix, size);
      result[size] = '\0';
    }
  else
    {
      /* SWI DDEUtils_ReadPrefix is not known.  */
      result = __getenv_from_os ("Prefix$Dir", NULL, 0, NULL);

      /* Filter out zero length results.  */
      if (result == NULL || result[0] == '\0')
        {
          free (result);
          return NULL;
        }
    }

  return result;
}
