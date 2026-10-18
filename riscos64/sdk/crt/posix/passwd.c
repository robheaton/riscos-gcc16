/* passwd.c - the user and group database of a system with one user: the user (uid 1, the name from Unix$User, USER or LOGNAME, else "user"; the home folder from HOME, else "/") and the group "users" (gid 1),
   plus root (uid 0) so that programs that look it up find it.  The SDK of RISC OS 64. */
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *user_name (void)
{
  const char *n = getenv ("Unix$User");
  if (!n || !*n) n = getenv ("USER");
  if (!n || !*n) n = getenv ("LOGNAME");
  return n && *n ? n : "user";
}
static const char *home_dir (void) { const char *h = getenv ("HOME"); return h && *h ? h : "/"; }

/* entry i (0 the user, 1 root) into pw, with the strings in buf */
static int fill_pw (int i, struct passwd *pw, char *buf, size_t size)
{
  const char *name = i == 0 ? user_name () : "root", *dir = i == 0 ? home_dir () : "/";
  size_t a = strlen (name) + 1, b = strlen (dir) + 1, c = sizeof "/bin/sh";
  if (a + b + c > size) return ERANGE;
  memcpy (buf, name, a); pw->pw_name = buf;
  memcpy (buf + a, dir, b); pw->pw_dir = buf + a;
  memcpy (buf + a + b, "/bin/sh", c); pw->pw_shell = buf + a + b;
  pw->pw_uid = i == 0 ? 1 : 0;
  pw->pw_gid = i == 0 ? 1 : 0;
  return 0;
}
static int find_pw (const char *name, int has_uid, uid_t uid)
{
  int i;
  for (i = 0; i < 2; i++)
    {
      const char *n = i == 0 ? user_name () : "root";
      if (has_uid ? uid == (i == 0 ? 1 : 0) : !strcmp (name, n)) return i;
    }
  return -1;
}

static struct passwd pwbuf;
static char pwstr[512];
static int pwpos;

struct passwd *getpwnam (const char *name) { struct passwd *r; return getpwnam_r (name, &pwbuf, pwstr, sizeof pwstr, &r) == 0 ? r : 0; }
struct passwd *getpwuid (uid_t uid) { struct passwd *r; return getpwuid_r (uid, &pwbuf, pwstr, sizeof pwstr, &r) == 0 ? r : 0; }
int getpwnam_r (const char *name, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
  int i = find_pw (name, 0, 0), e;
  *res = 0;
  if (i < 0) return 0;
  e = fill_pw (i, pw, buf, size);
  if (!e) *res = pw;
  return e;
}
int getpwuid_r (uid_t uid, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
  int i = find_pw (0, 1, uid), e;
  *res = 0;
  if (i < 0) return 0;
  e = fill_pw (i, pw, buf, size);
  if (!e) *res = pw;
  return e;
}
struct passwd *getpwent (void) { if (pwpos >= 2) return 0; fill_pw (pwpos++, &pwbuf, pwstr, sizeof pwstr); return &pwbuf; }
int getpwent_r (struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
  int e;
  *res = 0;
  if (pwpos >= 2) return ENOENT;
  e = fill_pw (pwpos, pw, buf, size);
  if (e) return e;
  pwpos++;
  *res = pw;
  return 0;
}
void setpwent (void) { pwpos = 0; }
void endpwent (void) { pwpos = 0; }

static struct group grbuf;
static char grstr[256];
static int grpos;

static int fill_gr (int i, struct group *g, char *buf, size_t size)
{
  const char *name = i == 0 ? "users" : "root", *u = i == 0 ? user_name () : "root";
  size_t a = strlen (name) + 1, b = strlen (u) + 1;
  char **mem;
  if (a + b + 2 * sizeof (char *) + 8 > size) return ERANGE;
  mem = (char **) (((size_t) buf + a + b + 7) & ~(size_t) 7);
  if ((char *) (mem + 2) > buf + size) return ERANGE;
  memcpy (buf, name, a); g->gr_name = buf;
  memcpy (buf + a, u, b);
  mem[0] = buf + a; mem[1] = 0;
  g->gr_mem = mem;
  g->gr_gid = i == 0 ? 1 : 0;
  return 0;
}
static int find_gr (const char *name, int has_gid, gid_t gid)
{
  int i;
  for (i = 0; i < 2; i++)
    if (has_gid ? gid == (i == 0 ? 1u : 0u) : !strcmp (name, i == 0 ? "users" : "root")) return i;
  return -1;
}
int getgrnam_r (const char *name, struct group *g, char *buf, size_t size, struct group **res)
{
  int i = find_gr (name, 0, 0), e;
  *res = 0;
  if (i < 0) return 0;
  e = fill_gr (i, g, buf, size);
  if (!e) *res = g;
  return e;
}
int getgrgid_r (gid_t gid, struct group *g, char *buf, size_t size, struct group **res)
{
  int i = find_gr (0, 1, gid), e;
  *res = 0;
  if (i < 0) return 0;
  e = fill_gr (i, g, buf, size);
  if (!e) *res = g;
  return e;
}
struct group *getgrnam (const char *name) { struct group *r; return getgrnam_r (name, &grbuf, grstr, sizeof grstr, &r) == 0 ? r : 0; }
struct group *getgrgid (gid_t gid) { struct group *r; return getgrgid_r (gid, &grbuf, grstr, sizeof grstr, &r) == 0 ? r : 0; }
struct group *getgrent (void) { if (grpos >= 2) return 0; fill_gr (grpos++, &grbuf, grstr, sizeof grstr); return &grbuf; }
void setgrent (void) { grpos = 0; }
void endgrent (void) { grpos = 0; }
