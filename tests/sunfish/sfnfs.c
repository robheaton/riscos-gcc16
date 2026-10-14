/* sfnfs.c - a READ-ONLY test of the Sunfish module against an NFS server: mount the export with the module's own SWI Sunfish_Mount (&58BC0), list the top directory (OS_GBPB 10), search down the directories
   for the first regular file (at most 40 directories listed), read its first 64 KB (OS_Find, OS_GBPB 3) and print its CRC-32 and its path, then Sunfish_Dismount.  Nothing is written: no NFS write call is made (the program only lists and reads).
   usage: sfnfs SERVER EXPORT [DISCNAME]   (for example: sfnfs myserver /srv/nfs/test NAS; DISCNAME defaults to NAS; the export is mounted as NFS3 over UDP).
   A line is printed (and flushed) BEFORE each step, so a hang shows where. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>

#define Sunfish_Mount 0x58BC0
#define Sunfish_Dismount 0x58BC1

static unsigned cs (void) { int t = 0; _swix (OS_ReadMonotonicTime, _OUT (0), &t); return (unsigned) t; }
static void say (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#include <stdarg.h>
static void say (const char *fmt, ...) { va_list ap; va_start (ap, fmt); vprintf (fmt, ap); va_end (ap); fflush (stdout); }

static unsigned crc32 (const unsigned char *p, unsigned n)
{
  unsigned c = 0xFFFFFFFFu, i, k;
  for (i = 0; i < n; i++) { c ^= p[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
  return ~c;
}

struct entry { char name[256]; unsigned len, type; };
static int listings = 0;

/* list DIR (a RISC OS path); fill FILEPATH with the path of the first regular file that has data, searching down the directories; returns 1 when found */
static int search (const char *dir, int depth, char *filepath, unsigned *filelen, int show)
{
  static char buf[8192];
  struct entry *subs = malloc (40 * sizeof *subs);
  int nsubs = 0, start = 0, count, i, n = 0, found = 0;
  _kernel_oserror *e;
  unsigned t0 = cs ();

  if (!subs || listings >= 40) { free (subs); return 0; }
  listings++;
  say ("sfnfs: listing %s (OS_GBPB 10) ...\n", dir);
  while (start != -1 && !found)
    {
      count = 0;
      e = _swix (OS_GBPB, _INR (0, 6) | _OUTR (3, 4), 10, dir, buf, 1000, start, sizeof buf, 0, &count, &start);
      if (e) { say ("sfnfs: OS_GBPB 10 failed: %s\n", e->errmess); break; }
      const unsigned char *p = (const unsigned char *) buf;
      for (i = 0; i < count; i++)
        {
          unsigned load = *(unsigned *) p, len = *(unsigned *) (p + 8), attr = *(unsigned *) (p + 12), type = *(unsigned *) (p + 16);
          const char *name = (const char *) (p + 20);
          if (n < 6 || (show && n < 25)) say ("   %-4s %10u bytes  attr %02x  %s%s\n", type == 2 ? "dir" : type == 1 ? "file" : "?", len, attr, name, (load >> 20) == 0xFFF ? "" : "  (load/exec address)");
          n++;
          if (type == 1 && len > 0 && !found) { snprintf (filepath, 600, "%s.%s", dir, name); *filelen = len; found = 1; }
          else if (type == 2 && nsubs < 40) { strncpy (subs[nsubs].name, name, 255); subs[nsubs].name[255] = 0; nsubs++; }
          p += 20 + ((strlen (name) + 1 + 3) & ~3u);
        }
    }
  say ("sfnfs: %d entries in %s in %u cs%s\n", n, dir, cs () - t0, found ? " (a file with data is here)" : "");
  for (i = 0; i < nsubs && !found && depth < 5; i++)
    {
      char sub[600];
      snprintf (sub, sizeof sub, "%s.%s", dir, subs[i].name);
      found = search (sub, depth + 1, filepath, filelen, 0);
    }
  free (subs);
  return found;
}

int main (int argc, char **argv)
{
  const char *server = argc > 1 ? argv[1] : 0;
  const char *export_ = argc > 2 ? argv[2] : 0;
  const char *disc = argc > 3 ? argv[3] : "NAS";
  char config[512], dir[128], path[600];
  static unsigned char data[65536];
  _kernel_oserror *e;
  unsigned t0, firstlen = 0;
  int fh = 0, got = 0;

  if (!server || !export_) { fprintf (stderr, "usage: sfnfs SERVER EXPORT [DISCNAME]\n"); return 2; }

  snprintf (config, sizeof config, "Protocol: NFS3\nServer: %s\nExport: %s\nLogging: 0\n", server, export_);
  say ("sfnfs: mounting %s:%s as Sunfish::%s (NFS3 over UDP) ...\n", server, export_, disc);
  t0 = cs ();
  e = _swix (Sunfish_Mount, _INR (0, 2), disc, 0, config);
  say ("sfnfs: Sunfish_Mount took %u cs: %s\n", cs () - t0, e ? e->errmess : "no error");
  if (e) return 1;
  snprintf (dir, sizeof dir, "Sunfish::%s.$", disc);
  path[0] = 0;
  if (search (dir, 0, path, &firstlen, 1))
    {
      say ("sfnfs: opening %s (%u bytes) with OS_Find ...\n", path, firstlen);
      e = _swix (OS_Find, _INR (0, 1) | _OUT (0), 0x4F, path, &fh);
      if (e) say ("sfnfs: OS_Find failed: %s\n", e->errmess);
      else
        {
          unsigned want = firstlen < sizeof data ? firstlen : sizeof data, notdone = 0;
          say ("sfnfs: reading %u bytes (OS_GBPB 3) ...\n", want);
          t0 = cs ();
          e = _swix (OS_GBPB, _INR (0, 4) | _OUT (3), 3, fh, data, want, 0, &notdone);
          if (e) say ("sfnfs: OS_GBPB 3 failed: %s\n", e->errmess);
          else
            {
              got = (int) (want - notdone);
              say ("sfnfs: read %d bytes in %u cs, CRC-32 %08X, first bytes %02X %02X %02X %02X %02X %02X %02X %02X\n", got, cs () - t0, crc32 (data, got), data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
              say ("sfnfs: file %s length %u\n", path, firstlen);
            }
          _swix (OS_Find, _INR (0, 1), 0, fh);
        }
    }
  else say ("sfnfs: no regular file with data found in %d directories\n", listings);
  say ("sfnfs: dismounting ...\n");
  e = _swix (Sunfish_Dismount, _INR (0, 1), disc, 0);
  say ("sfnfs: Sunfish_Dismount: %s\n", e ? e->errmess : "no error");
  say ("sfnfs: done\n");
  return 0;
}
