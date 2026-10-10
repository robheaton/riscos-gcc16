/* netdb.c - gethostbyname (the Resolver module's Resolver_GetHostByName, SWI &46000: R1 -> the name; R0 = an error number (0: found), R1 -> the host details {name, aliases, address type, address size,
   addresses}), the Internet address conversions (inet_addr ...), services (the file InetDBase:Services, else a small list) and protocols (a list).  The answers are in static buffers. */
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "kernel.h"

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);

int h_errno;

/* ---------------------------------------------------------------- inet_aton and the others */
int inet_aton (const char *cp, struct in_addr *addr)
{
  unsigned long parts[4];
  int n = 0;
  const char *p = cp;
  for (;;)
    {
      unsigned long v = 0;
      int base = 10, any = 0;
      if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
      else if (p[0] == '0') { base = 8; any = 1; p++; }
      for (;; p++)
        {
          int d;
          if (*p >= '0' && *p <= '9') d = *p - '0';
          else if (base == 16 && isxdigit ((unsigned char) *p)) d = (*p | 0x20) - 'a' + 10;
          else break;
          if (d >= base) return 0;
          v = v * (unsigned) base + (unsigned) d;
          if (v > 0xFFFFFFFFul) return 0;
          any = 1;
        }
      if (!any) return 0;
      if (n == 4) return 0;
      parts[n++] = v;
      if (*p == '.') { p++; continue; }
      break;
    }
  if (*p && !isspace ((unsigned char) *p)) return 0;
  {
    unsigned long a;
    int i;
    for (i = 0; i < n - 1; i++) if (parts[i] > 255) return 0;
    switch (n)
      {
      case 1: a = parts[0]; break;
      case 2: if (parts[1] > 0xFFFFFFul) return 0; a = (parts[0] << 24) | parts[1]; break;
      case 3: if (parts[2] > 0xFFFFul) return 0; a = (parts[0] << 24) | (parts[1] << 16) | parts[2]; break;
      default: if (parts[3] > 255) return 0; a = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]; break;
      }
    if (addr) addr->s_addr = htonl ((unsigned) a);
  }
  return 1;
}

in_addr_t inet_addr (const char *cp)
{
  struct in_addr a;
  return inet_aton (cp, &a) ? a.s_addr : INADDR_NONE;
}

in_addr_t inet_network (const char *cp)
{
  struct in_addr a;
  return inet_aton (cp, &a) ? ntohl (a.s_addr) : INADDR_NONE;
}

char *inet_ntoa (struct in_addr in)
{
  static char buf[16];
  unsigned a = ntohl (in.s_addr);
  sprintf (buf, "%u.%u.%u.%u", (a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255);
  return buf;
}

int inet_pton (int af, const char *src, void *dst)
{
  unsigned v[4];
  int n = 0;
  const char *p = src;
  if (af != AF_INET) { errno = EAFNOSUPPORT; return -1; }
  for (;;)
    {
      unsigned x = 0;
      int digits = 0;
      while (*p >= '0' && *p <= '9') { x = x * 10 + (unsigned) (*p++ - '0'); if (++digits > 3 || x > 255) return 0; }
      if (!digits) return 0;
      v[n++] = x;
      if (n == 4) break;
      if (*p++ != '.') return 0;
    }
  if (*p) return 0;
  ((struct in_addr *) dst)->s_addr = htonl ((v[0] << 24) | (v[1] << 16) | (v[2] << 8) | v[3]);
  return 1;
}

const char *inet_ntop (int af, const void *src, char *dst, socklen_t size)
{
  const char *t;
  if (af != AF_INET) { errno = EAFNOSUPPORT; return NULL; }
  t = inet_ntoa (*(const struct in_addr *) src);
  if (strlen (t) + 1 > size) { errno = ENOSPC; return NULL; }
  strcpy (dst, t);
  return dst;
}

struct in_addr inet_makeaddr (in_addr_t net, in_addr_t host)
{
  struct in_addr a;
  if (net < 128) a.s_addr = htonl ((net << 24) | (host & 0xFFFFFF));
  else if (net < 65536) a.s_addr = htonl ((net << 16) | (host & 0xFFFF));
  else a.s_addr = htonl ((net << 8) | (host & 0xFF));
  return a;
}

in_addr_t inet_lnaof (struct in_addr in)
{
  unsigned a = ntohl (in.s_addr);
  if (IN_CLASSA (a)) return a & 0xFFFFFF;
  if (IN_CLASSB (a)) return a & 0xFFFF;
  return a & 0xFF;
}

in_addr_t inet_netof (struct in_addr in)
{
  unsigned a = ntohl (in.s_addr);
  if (IN_CLASSA (a)) return (a >> 24) & 0xFF;
  if (IN_CLASSB (a)) return (a >> 16) & 0xFFFF;
  return (a >> 8) & 0xFFFFFF;
}

/* ---------------------------------------------------------------- hosts */
#define MAXADDR 8
static struct hostent host;
static char host_name[256];
static char *host_aliases[1];
static char *host_addrs[MAXADDR + 1];
static unsigned char host_addr_store[MAXADDR][4];

static struct hostent *host_set (const char *name, const unsigned char *addrs, int naddr)
{
  int i;
  strncpy (host_name, name, sizeof host_name - 1);
  host_name[sizeof host_name - 1] = 0;
  if (naddr > MAXADDR) naddr = MAXADDR;
  for (i = 0; i < naddr; i++) { memcpy (host_addr_store[i], addrs + 4 * i, 4); host_addrs[i] = (char *) host_addr_store[i]; }
  host_addrs[naddr] = NULL;
  host_aliases[0] = NULL;
  host.h_name = host_name; host.h_aliases = host_aliases; host.h_addrtype = AF_INET; host.h_length = 4; host.h_addr_list = host_addrs;
  return &host;
}

struct hostent *gethostbyname (const char *name)
{
  struct in_addr a;
  unsigned r[10] = { 0 };
  _kernel_oserror *e;
  if (!name || !*name) { h_errno = HOST_NOT_FOUND; return NULL; }
  if (isdigit ((unsigned char) name[0]) && inet_aton (name, &a)) return host_set (name, (const unsigned char *) &a.s_addr, 1);
  r[1] = (unsigned) name;
  e = __modlib_xswi (0x66000, r);                                  /* XResolver_GetHostByName */
  if (e) { h_errno = NO_RECOVERY; return NULL; }
  if (r[0] != 0 || !r[1]) { h_errno = r[0] == 0 ? HOST_NOT_FOUND : (r[0] == TRY_AGAIN ? TRY_AGAIN : HOST_NOT_FOUND); return NULL; }
  {
    const unsigned *d = (const unsigned *) r[1];                    /* {name, aliases, address type, address size, addresses} */
    const unsigned char *const *list = (const unsigned char *const *) d[4];
    unsigned char addrs[MAXADDR * 4];
    int n = 0;
    if (d[2] != AF_INET || d[3] != 4 || !list) { h_errno = NO_DATA; return NULL; }
    while (n < MAXADDR && list[n]) { memcpy (addrs + 4 * n, list[n], 4); n++; }
    if (!n) { h_errno = NO_DATA; return NULL; }
    return host_set (d[0] ? (const char *) d[0] : name, addrs, n);
  }
}

struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type)
{
  (void) addr; (void) len; (void) type;
  h_errno = NO_RECOVERY;
  return NULL;
}

const char *hstrerror (int err)
{
  switch (err)
    {
    case 0: return "Resolver Error 0 (no error)";
    case HOST_NOT_FOUND: return "Unknown host";
    case TRY_AGAIN: return "Host name lookup failure";
    case NO_RECOVERY: return "Unknown server error";
    case NO_DATA: return "No address associated with name";
    }
  return "Unknown resolver error";
}

void herror (const char *s)
{
  if (s && *s) fprintf (stderr, "%s: ", s);
  fprintf (stderr, "%s\n", hstrerror (h_errno));
}

/* ---------------------------------------------------------------- services and protocols */
static struct servent serv;
static char serv_name[64], serv_proto[16];
static char *serv_aliases[1];

static const struct { const char *name; int port; const char *proto; } services[] = {
  { "echo", 7, "tcp" }, { "echo", 7, "udp" }, { "discard", 9, "tcp" }, { "daytime", 13, "tcp" }, { "ftp-data", 20, "tcp" }, { "ftp", 21, "tcp" }, { "ssh", 22, "tcp" }, { "telnet", 23, "tcp" },
  { "smtp", 25, "tcp" }, { "time", 37, "tcp" }, { "time", 37, "udp" }, { "domain", 53, "tcp" }, { "domain", 53, "udp" }, { "bootps", 67, "udp" }, { "bootpc", 68, "udp" }, { "tftp", 69, "udp" },
  { "http", 80, "tcp" }, { "pop3", 110, "tcp" }, { "ntp", 123, "udp" }, { "imap", 143, "tcp" }, { "snmp", 161, "udp" }, { "https", 443, "tcp" }, { "syslog", 514, "udp" }, { "printer", 515, "tcp" },
  { "imaps", 993, "tcp" }, { "pop3s", 995, "tcp" }, { NULL, 0, NULL } };

static struct servent *serv_set (const char *name, int port, const char *proto)
{
  strncpy (serv_name, name, sizeof serv_name - 1); serv_name[sizeof serv_name - 1] = 0;
  strncpy (serv_proto, proto, sizeof serv_proto - 1); serv_proto[sizeof serv_proto - 1] = 0;
  serv_aliases[0] = NULL;
  serv.s_name = serv_name; serv.s_aliases = serv_aliases; serv.s_port = (int) htons ((unsigned short) port); serv.s_proto = serv_proto;
  return &serv;
}

/* one line of InetDBase:Services: name port/proto [aliases]; NAME (or PORT when NAME is NULL) and PROTO (or NULL) select it */
static struct servent *serv_lookup (const char *name, int port, const char *proto)
{
  FILE *f = fopen ("InetDBase:Services", "r");
  char line[200];
  int i;
  if (f)
    {
      while (fgets (line, sizeof line, f))
        {
          char n[64], pr[16];
          int p, hit;
          char *c = strchr (line, '#'), *slash, *w;
          if (c) *c = 0;
          if (sscanf (line, "%63s %d/%15s", n, &p, pr) != 3) continue;
          if (proto && strcmp (proto, pr)) continue;
          hit = name ? strcmp (name, n) == 0 : p == port;
          if (!hit && name && (slash = strchr (line, '/')) != NULL)       /* the aliases follow the protocol */
            for (w = strtok (slash, " \t\n"), w = strtok (NULL, " \t\n"); w; w = strtok (NULL, " \t\n"))
              if (!strcmp (w, name)) hit = 1;
          if (!hit) continue;
          fclose (f);
          return serv_set (n, p, pr);
        }
      fclose (f);
    }
  for (i = 0; services[i].name; i++)
    if ((!proto || !strcmp (proto, services[i].proto)) && (name ? !strcmp (name, services[i].name) : services[i].port == port))
      return serv_set (services[i].name, services[i].port, services[i].proto);
  return NULL;
}

struct servent *getservbyname (const char *name, const char *proto) { return serv_lookup (name, 0, proto); }
struct servent *getservbyport (int port, const char *proto) { return serv_lookup (NULL, (int) ntohs ((unsigned short) port), proto); }

static struct protoent proto;
static char proto_name[16];
static char *proto_aliases[1];
static const struct { const char *name; int number; } protocols[] = { { "ip", 0 }, { "icmp", 1 }, { "igmp", 2 }, { "tcp", 6 }, { "udp", 17 }, { NULL, 0 } };

static struct protoent *proto_set (int i)
{
  strcpy (proto_name, protocols[i].name);
  proto_aliases[0] = NULL;
  proto.p_name = proto_name; proto.p_aliases = proto_aliases; proto.p_proto = protocols[i].number;
  return &proto;
}

struct protoent *getprotobyname (const char *name)
{
  int i;
  for (i = 0; protocols[i].name; i++) if (!strcmp (protocols[i].name, name)) return proto_set (i);
  return NULL;
}

struct protoent *getprotobynumber (int number)
{
  int i;
  for (i = 0; protocols[i].name; i++) if (protocols[i].number == number) return proto_set (i);
  return NULL;
}
