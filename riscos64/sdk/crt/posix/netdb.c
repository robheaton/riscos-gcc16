/* netdb.c - names of hosts, services and protocols.  Numeric addresses and "localhost" are answered here; other host names go to the Resolver module (Resolver_GetHost, SWI 0x46001: R0 = name, answers R0 =
   status (0 is good) and R1 = a hostent; the hostent is read with the layout of this C library, which is what a 64-bit Resolver has to use too).  Services and protocols come from a small table (the usual ones;
   there is no /etc/services).  IPv4 only.  Not tested on a real Resolver: the build service for Pyromaniac has the Internet SWIs disabled.  The SDK of RISC OS 64. */
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

int h_errno;

static const struct { const char *name; int port; const char *proto; } services[] = {
  { "echo", 7, "tcp" }, { "echo", 7, "udp" }, { "discard", 9, "tcp" }, { "daytime", 13, "tcp" }, { "ftp-data", 20, "tcp" }, { "ftp", 21, "tcp" }, { "ssh", 22, "tcp" }, { "telnet", 23, "tcp" },
  { "smtp", 25, "tcp" }, { "time", 37, "tcp" }, { "domain", 53, "tcp" }, { "domain", 53, "udp" }, { "tftp", 69, "udp" }, { "gopher", 70, "tcp" }, { "http", 80, "tcp" }, { "pop3", 110, "tcp" },
  { "sunrpc", 111, "tcp" }, { "sunrpc", 111, "udp" }, { "nntp", 119, "tcp" }, { "ntp", 123, "udp" }, { "imap", 143, "tcp" }, { "snmp", 161, "udp" }, { "ldap", 389, "tcp" }, { "https", 443, "tcp" },
  { "nfs", 2049, "tcp" }, { "nfs", 2049, "udp" }, { "irc", 6667, "tcp" }, { 0, 0, 0 } };
static const struct { const char *name; int num; } protocols[] = { { "ip", 0 }, { "icmp", 1 }, { "tcp", 6 }, { "udp", 17 }, { 0, 0 } };

const char *hstrerror (int e)
{
  switch (e)
    {
    case HOST_NOT_FOUND: return "Unknown host";
    case TRY_AGAIN: return "Host name lookup failure";
    case NO_RECOVERY: return "Unknown server error";
    case NO_DATA: return "No address associated with name";
    default: return "Resolver error";
    }
}

/* ---- gethostbyname and gethostbyaddr: one hostent, static as the interface says ---- */
static struct hostent he;
static char *he_aliases[1], *he_addrs[2];
static unsigned char he_addr[4];
static char he_name[256];

static struct hostent *set_he (const char *name, const unsigned char a[4])
{
  strncpy (he_name, name, sizeof he_name - 1);
  memcpy (he_addr, a, 4);
  he_aliases[0] = 0; he_addrs[0] = (char *) he_addr; he_addrs[1] = 0;
  he.h_name = he_name; he.h_aliases = he_aliases; he.h_addrtype = AF_INET; he.h_length = 4; he.h_addr_list = he_addrs;
  return &he;
}

/* the host name, resolved: 0 and the address, or an h_errno value */
static int resolve (const char *name, unsigned char out[4])
{
  unsigned char a[4];
  unsigned long r[10] = { (unsigned long) name };
  if (inet_pton (AF_INET, name, a) == 1) { memcpy (out, a, 4); return 0; }
  if (!strcasecmp (name, "localhost")) { out[0] = 127; out[1] = out[2] = 0; out[3] = 1; return 0; }
  if (swi_x (0x46001, r)) return TRY_AGAIN;				/* Resolver_GetHost */
  if (r[0] != 0 || !r[1]) return r[0] ? (int) r[0] : HOST_NOT_FOUND;
  {
    const struct hostent *h = (const struct hostent *) r[1];
    if (h->h_addrtype != AF_INET || h->h_length != 4 || !h->h_addr_list || !h->h_addr_list[0]) return NO_DATA;
    memcpy (out, h->h_addr_list[0], 4);
  }
  return 0;
}

struct hostent *gethostbyname (const char *name)
{
  unsigned char a[4];
  int e = resolve (name, a);
  if (e) { h_errno = e; return 0; }
  return set_he (name, a);
}
struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type)
{
  const unsigned char *a = addr;
  if (type != AF_INET || len != 4) { h_errno = NO_RECOVERY; return 0; }
  if (a[0] == 127 && !a[1] && !a[2] && a[3] == 1) return set_he ("localhost", a);
  {
    char text[16];
    inet_ntop (AF_INET, a, text, sizeof text);				/* no reverse lookup: the name is the dotted address */
    return set_he (text, a);
  }
}

/* ---- services and protocols ---- */
static struct servent se;
static char *se_aliases[1];
static struct protoent pe;
static char *pe_aliases[1];

static struct servent *set_se (int i)
{
  se_aliases[0] = 0;
  se.s_name = (char *) services[i].name; se.s_aliases = se_aliases; se.s_port = htons ((unsigned short) services[i].port); se.s_proto = (char *) services[i].proto;
  return &se;
}
struct servent *getservbyname (const char *name, const char *proto)
{
  int i;
  for (i = 0; services[i].name; i++)
    if (!strcmp (services[i].name, name) && (!proto || !strcmp (proto, services[i].proto))) return set_se (i);
  return 0;
}
struct servent *getservbyport (int port, const char *proto)
{
  int i;
  for (i = 0; services[i].name; i++)
    if (htons ((unsigned short) services[i].port) == (port & 0xFFFF) && (!proto || !strcmp (proto, services[i].proto))) return set_se (i);
  return 0;
}
static struct protoent *set_pe (int i)
{
  pe_aliases[0] = 0;
  pe.p_name = (char *) protocols[i].name; pe.p_aliases = pe_aliases; pe.p_proto = protocols[i].num;
  return &pe;
}
struct protoent *getprotobyname (const char *name)
{
  int i;
  for (i = 0; protocols[i].name; i++) if (!strcmp (protocols[i].name, name)) return set_pe (i);
  return 0;
}
struct protoent *getprotobynumber (int n)
{
  int i;
  for (i = 0; protocols[i].name; i++) if (protocols[i].num == n) return set_pe (i);
  return 0;
}

/* ---- getaddrinfo ---- */
const char *gai_strerror (int e)
{
  switch (e)
    {
    case 0: return "Success";
    case EAI_BADFLAGS: return "Invalid value for ai_flags";
    case EAI_NONAME: return "Name or service not known";
    case EAI_AGAIN: return "Temporary failure in name resolution";
    case EAI_FAIL: return "Non-recoverable failure in name resolution";
    case EAI_FAMILY: return "ai_family not supported";
    case EAI_SOCKTYPE: return "ai_socktype not supported";
    case EAI_SERVICE: return "Servname not supported for ai_socktype";
    case EAI_MEMORY: return "Memory allocation failure";
    case EAI_SYSTEM: return "System error";
    case EAI_OVERFLOW: return "Argument buffer overflow";
    default: return "Unknown error";
    }
}

static struct addrinfo *make_ai (const unsigned char a[4], int port_net, int socktype, int protocol, const char *canon)
{
  struct addrinfo *ai = calloc (1, sizeof *ai);
  struct sockaddr_in *sin = calloc (1, sizeof *sin);
  if (!ai || !sin) { free (ai); free (sin); return 0; }
  sin->sin_family = AF_INET; sin->sin_port = (in_port_t) port_net; memcpy (&sin->sin_addr, a, 4);
  ai->ai_family = AF_INET; ai->ai_socktype = socktype; ai->ai_protocol = protocol; ai->ai_addrlen = sizeof *sin; ai->ai_addr = (struct sockaddr *) sin;
  if (canon) ai->ai_canonname = strdup (canon);
  return ai;
}

int getaddrinfo (const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
  int flags = hints ? hints->ai_flags : 0, family = hints ? hints->ai_family : AF_UNSPEC, type = hints ? hints->ai_socktype : 0, proto = hints ? hints->ai_protocol : 0;
  int port = 0, i, n = 0;
  unsigned char a[4];
  struct addrinfo *head = 0, **tail = &head;
  static const int types[2] = { SOCK_STREAM, SOCK_DGRAM };
  static const int protos[2] = { 6, 17 };
  if (!node && !service) return EAI_NONAME;
  if (family != AF_UNSPEC && family != AF_INET) return EAI_FAMILY;
  if (type && type != SOCK_STREAM && type != SOCK_DGRAM) return EAI_SOCKTYPE;
  if (service)
    {
      char *end;
      long v = strtol (service, &end, 10);
      if (*service && !*end && v >= 0 && v < 65536) port = htons ((unsigned short) v);
      else if (flags & AI_NUMERICSERV) return EAI_NONAME;
      else
	{
	  const struct servent *s = getservbyname (service, type == SOCK_DGRAM ? "udp" : type == SOCK_STREAM ? "tcp" : 0);
	  if (!s) return EAI_SERVICE;
	  port = s->s_port;
	}
    }
  if (!node) { a[0] = a[1] = a[2] = a[3] = 0; if (!(flags & AI_PASSIVE)) { a[0] = 127; a[3] = 1; } }
  else if (inet_pton (AF_INET, node, a) != 1)
    {
      int e;
      if (flags & AI_NUMERICHOST) return EAI_NONAME;
      e = resolve (node, a);
      if (e) return e == TRY_AGAIN ? EAI_AGAIN : EAI_NONAME;
    }
  for (i = 0; i < 2; i++)
    {
      struct addrinfo *ai;
      if (type && type != types[i]) continue;
      if (proto && proto != protos[i]) continue;
      ai = make_ai (a, port, types[i], protos[i], (flags & AI_CANONNAME) && n == 0 && node ? node : 0);
      if (!ai) { freeaddrinfo (head); return EAI_MEMORY; }
      *tail = ai; tail = &ai->ai_next; n++;
    }
  if (!n) return EAI_SOCKTYPE;
  *res = head;
  return 0;
}
void freeaddrinfo (struct addrinfo *ai)
{
  while (ai)
    {
      struct addrinfo *next = ai->ai_next;
      free (ai->ai_addr); free (ai->ai_canonname); free (ai);
      ai = next;
    }
}

int getnameinfo (const struct sockaddr *sa, socklen_t len, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags)
{
  const struct sockaddr_in *sin = (const struct sockaddr_in *) sa;
  if (!sa || len < sizeof *sin || sa->sa_family != AF_INET) return EAI_FAMILY;
  if (host && hostlen)
    {
      if (flags & NI_NAMEREQD) return EAI_NONAME;
      if (!inet_ntop (AF_INET, &sin->sin_addr, host, hostlen)) return EAI_OVERFLOW;
    }
  if (serv && servlen)
    {
      const struct servent *s = (flags & NI_NUMERICSERV) ? 0 : getservbyport (sin->sin_port, (flags & NI_DGRAM) ? "udp" : "tcp");
      char text[8];
      const char *t = text;
      int p = ntohs (sin->sin_port), k = 0;
      if (s) t = s->s_name;
      else { char rev[8]; if (!p) rev[k++] = '0'; while (p) { rev[k++] = (char) ('0' + p % 10); p /= 10; } for (p = 0; p < k; p++) text[p] = rev[k - 1 - p]; text[k] = 0; }
      if (strlen (t) + 1 > servlen) return EAI_OVERFLOW;
      strcpy (serv, t);
    }
  return 0;
}
