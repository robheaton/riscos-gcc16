/* envext.c - setenv, putenv, unsetenv (the system variables of RISC OS: OS_SetVarVal / OS_ReadVarVal, the way UnixLib's SharedCLibrary version does them), gethostname and getdomainname
   (Inet$HostName and Inet$LocalDomain, the way UnixLib reads them). */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>
#include <unistd.h>

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
#define X 0x20000u

/* does the system variable exist?  OS_ReadVarVal with a negative buffer size only measures: a variable that is there gives R2 = the complement of its length (and the kernel sets V with the
   "Buffer overflow" error as well: Kernel/s/Arthur2, ReadVarVal_BufWillOFlow), one that is not there gives the "variable not found" error and R2 = 0 */
static int var_exists (const char *name)
{
  char dummy[4];
  unsigned r[10] = { (unsigned) name, (unsigned) dummy, (unsigned) -1, 0, 0 };
  __modlib_xswi (0x23 | X, r);
  return (int) r[2] < 0;
}

int setenv (const char *name, const char *value, int replace)
{
  if (!name || !*name || strchr (name, '=')) { errno = EINVAL; return -1; }
  if (!replace && var_exists (name)) return 0;
  if (_kernel_setenv (name, value ? value : "")) { errno = EINVAL; return -1; }
  return 0;
}
int unsetenv (const char *name)
{
  if (!name || !*name || strchr (name, '=')) { errno = EINVAL; return -1; }
  if (!var_exists (name)) return 0;
  if (_kernel_setenv (name, 0)) { errno = EINVAL; return -1; }
  return 0;
}
int putenv (char *string)
{
  char name[256];
  const char *eq = strchr (string, '=');
  size_t n;
  if (!eq) return unsetenv (string);
  n = (size_t) (eq - string);
  if (!n || n >= sizeof name) { errno = EINVAL; return -1; }
  memcpy (name, string, n);
  name[n] = 0;
  if (_kernel_setenv (name, eq + 1)) { errno = EINVAL; return -1; }
  return 0;
}

/* "acorn" and the station number from CMOS RAM byte 1 (OS_Byte 161), as UnixLib makes a name when there is none */
static int fakehostname (char *name, size_t len)
{
  static const char hex[] = "0123456789ABCDEF";
  unsigned r[10] = { 161, 1 };
  char buf[8] = "acorn";
  if (__modlib_xswi (6 | X, r)) { errno = EOPSYS; return -1; }
  buf[5] = hex[(r[2] >> 4) & 15]; buf[6] = hex[r[2] & 15]; buf[7] = 0;
  if (len < 8) { memcpy (name, buf, len); errno = ENAMETOOLONG; return -1; }
  strcpy (name, buf);
  return 0;
}
int gethostname (char *name, size_t len)
{
  const char *host, *domain;
  size_t hlen;
  if (!name) { errno = EFAULT; return -1; }
  host = getenv ("Inet$HostName");
  if (!host) return fakehostname (name, len);
  hlen = strlen (host);
  if (hlen >= len) { memcpy (name, host, len); errno = ENAMETOOLONG; return -1; }       /* (UnixLib's behaviour: as much as fits, not terminated) */
  strcpy (name, host);
  if (strchr (name, '.')) return 0;                                                       /* a dot: the name is qualified already */
  domain = getenv ("Inet$LocalDomain");
  if (!domain) return 0;
  if (*domain == '.') domain++;
  name += hlen; len -= hlen + 1;
  *name++ = '.';
  if (strlen (domain) >= len) { memcpy (name, domain, len); errno = ENAMETOOLONG; return -1; }
  strcpy (name, domain);
  return 0;
}
int getdomainname (char *name, size_t len)
{
  const char *domain = getenv ("Inet$LocalDomain");
  if (!name) { errno = EFAULT; return -1; }
  if (!domain)
    {
      domain = getenv ("Inet$HostName");
      if (domain && (domain = strchr (domain, '.')) != 0) domain++;
      if (!domain) { errno = ENOSYS; return -1; }
    }
  if (strlen (domain) >= len) { memcpy (name, domain, len); errno = ENAMETOOLONG; return -1; }
  strcpy (name, domain);
  return 0;
}
