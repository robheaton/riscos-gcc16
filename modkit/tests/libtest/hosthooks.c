/* hosthooks.c - the kernel as the host build of the library tests sees it: __modlib_xswi / __modlib_xswif of modswi.S, and the models of the SWIs that the library calls (the same models are in armrun.py for the
   interpreter).
   The test SWIs:
     0x5AB00  r0 = r0 + r1, r1 = r2 ^ 0x1234, r2 = r3 * 3, r3 = r4 - r5, r4 = r6 | r7, r5 = r8, r6 = r9, r7 = the old r0, r8 = 0x80000000, r9 = 0xFFFFFFFF; flags: N and Z of the new r0, C when the old
             r0 was odd; r1 = 0xBAD on entry: an error (number 0xB00B, "Test error"), V set
     0x5AB01  sets the real time clock (r0: low 32 bits, r1: bits 32 - 39, centiseconds since 1900) as OS_Word 14, 3 reads it
     0x5AB02  sets the value that OS_ReadMonotonicTime gives (r0)
     0x5AB07  file system faults and files that cannot be made otherwise: see "the files" below
     (the screen and the keyboard of the tests are the functions mk_host_screen_* and mk_host_keys_* below; armrun.py has them as the SWIs 0x5AB04 - 0x5AB06)
   The file system SWIs work on the files of the host: OS_Find (open for input / output / update), OS_GBPB 1 - 4, OS_Args 0 - 3, 254 and 255, OS_File 4 (attributes), 6 (delete), 8 (create a folder) and 17 (information), OS_FSControl 25
   (rename), with the rules of FileSwitch and FileCore that the library depends on (the list is at "the files" below).  OS_WriteC and OS_NewLine (LF CR) are kept in a buffer when the test asks for it;
   OS_ReadLine takes the lines that the test has pushed (Escape when there is none).
   Pointers in the registers are tokens (hostptr.h): a 64 bit pointer does not fit in a register. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct { int errnum; char errmess[252]; } oserr;
static oserr test_err = { 0xB00B, "Test error" }, unknown_err = { 0x1E6, "SWI not known to the host model" };
static oserr e_notfound = { 0xD6, "File not found" }, e_outside = { 0xB7, "Outside file" }, e_exists = { 0xB0, "Bad rename" }, e_isdir = { 0xA8, "Object is a directory" },
             e_access = { 0xBD, "Access violation" }, e_badhandle = { 0xDE, "Channel" }, e_other = { 0x11A, "Operation failed" }, e_notupdate = { 0xC1, "Not open for update" },
             e_fileopen = { 0xC2, "File open" }, e_locked = { 0xC3, "Locked" }, e_notempty = { 0xB4, "Dir not empty" }, e_toomany = { 0xC0, "Too many open files" },
             e_discfull = { 0xC6, "Disc full" };
static unsigned long long g_clock;
static unsigned g_mono;

/* pointer tokens: the last 16 pointers that were given out */
static const void *ptrtab[16];
static unsigned ptrctr;
unsigned mk_ptr_in (const void *p)
{
  unsigned i = ptrctr++ & 15;
  ptrtab[i] = p;
  return 0x60000000u | i;
}
static void *deref (unsigned tok) { return (void *) ptrtab[tok & 15]; }

/* the screen */
static char *screen; static size_t screen_len, screen_cap; static int screen_on;
void mk_host_screen_start (void) { screen_on = 1; screen_len = 0; }
size_t mk_host_screen_take (char *buf, size_t size)
{
  size_t n = screen_len < size ? screen_len : size;
  if (n) memcpy (buf, screen, n);
  screen_on = 0; screen_len = 0;
  return n;
}
static void screen_put (char c)
{
  if (!screen_on) return;
  if (screen_len == screen_cap) screen = realloc (screen, screen_cap = screen_cap ? screen_cap * 2 : 1024);
  screen[screen_len++] = c;
}
/* the keyboard: lines to give to OS_ReadLine; a length of -1 is Escape */
static struct { char *s; int len; } keys[32];
static int nkeys, kpos;
void mk_host_keys_push (const char *s, int len)
{
  keys[nkeys].s = len < 0 ? 0 : strdup (s);
  keys[nkeys].len = len;
  nkeys++;
}

/* the files: the model of FileSwitch / FileCore that the library is written for (the same rules are in fsmodel.py, for the interpreter):
   - a file that is open for writing cannot be opened again, nor deleted, nor renamed (&C2); a file open for reading can be opened for reading again
   - a file that is read only (the host's write bit) or locked is opened for update or output as a stream that cannot be written, with no word and with no truncation: the first write is &C1 "Not open for update";
     OS_Args 254 says it (bit 7 = write, bit 6 = read); a locked file cannot be deleted or renamed (&C3)
   - OS_Args 1 beyond the end of a file that can be written makes it longer with zeros, for a file that cannot be written it is &B7; OS_GBPB 1 / 2 give a full disc (&C6) when the host cannot write, never a short write
   - an extent is an unsigned 32 bit number; a directory is &A8, a directory that is not empty &B4, a rename onto a file &B0
   - the test SWI 0x5AB07 (mk_host_fs_ctl): 1 = set the length of a file (also makes one: a sparse file of 2.5 GB), 2 = the next OS_Args 255 gives a full disc, 3 = the next OS_GBPB write gives a full disc,
     4 = no more faults */
typedef struct { int fd; long ptr; int writable, modified; dev_t dev; ino_t ino; } handle;
static handle H[64];
#define H0 0x20
static int fault_flush, fault_write;
static char locked[8][256];
static handle *get_handle (unsigned h) { return h >= H0 && h < H0 + 64 && H[h - H0].fd > 0 ? &H[h - H0] : 0; }
static unsigned extent (handle *h) { struct stat st; return fstat (h->fd, &st) ? 0 : (unsigned) st.st_size; }
/* the attributes that OS_File 4 set, as OS_File 17 gives them back (bit 0 read, bit 1 write, bit 3 locked, bits 4 and 5 public): kept by name, forgotten when the file is made again, deleted or renamed */
static struct { char name[256]; int attr; } attrs[8];
static void attr_forget (const char *name) { for (int i = 0; i < 8; i++) if (attrs[i].name[0] && !strcmp (attrs[i].name, name)) attrs[i].name[0] = 0; }
static void attr_set (const char *name, int attr)
{
  int i;
  attr_forget (name);
  for (i = 0; i < 8; i++) if (!attrs[i].name[0]) break;
  if (i < 8) { strncpy (attrs[i].name, name, 255); attrs[i].attr = attr; }
}
static int attr_get (const char *name, int dflt) { for (int i = 0; i < 8; i++) if (attrs[i].name[0] && !strcmp (attrs[i].name, name)) return attrs[i].attr; return dflt; }
static int is_locked (const char *name) { for (int i = 0; i < 8; i++) if (locked[i][0] && !strcmp (locked[i], name)) return 1; return 0; }
static void set_locked (const char *name, int on)
{
  for (int i = 0; i < 8; i++) if (locked[i][0] && !strcmp (locked[i], name)) { if (!on) locked[i][0] = 0; return; }
  if (on) for (int i = 0; i < 8; i++) if (!locked[i][0]) { strncpy (locked[i], name, 255); return; }
}
/* 0: not open; 1: open for reading only; 2: open for writing */
static int open_state (const struct stat *st)
{
  int r = 0;
  for (int i = 0; i < 64; i++) if (H[i].fd > 0 && H[i].dev == st->st_dev && H[i].ino == st->st_ino) r = H[i].writable ? 2 : r ? r : 1;
  return r;
}

int mk_host_read_clock (unsigned char *b)
{
  for (int i = 0; i < 5; i++) b[i] = (unsigned char) (g_clock >> (8 * i));
  return 0;
}

void mk_host_fs_ctl (int reason, const char *name, unsigned value)
{
  if (reason == 1) { int fd = open (name, O_RDWR | O_CREAT, 0666); if (fd >= 0) { if (ftruncate (fd, (off_t) value)) abort (); close (fd); } }
  else if (reason == 2) fault_flush = 1;
  else if (reason == 3) fault_write = 1;
  else fault_flush = fault_write = 0;
}

static oserr *os_find (unsigned *r)
{
  if (r[0] == 0)                                                   /* close */
    {
      handle *h = get_handle (r[1]);
      if (!h) return &e_badhandle;
      close (h->fd);
      h->fd = 0;
      return 0;
    }
  {
    unsigned how = r[0] & 0xC0;
    const char *name = deref (r[1]);
    struct stat st;
    int fd, i, exists = stat (name, &st) == 0, writable;
    if (exists && S_ISDIR (st.st_mode)) return &e_isdir;
    if (!exists)
      {
        if (how != 0x80) { if (r[0] & 8) return &e_notfound; r[0] = 0; return 0; }
        writable = 1;
        attr_forget (name);
      }
    else writable = how != 0x40 && access (name, W_OK) == 0 && !is_locked (name);
    if (exists)
      {
        int o = open_state (&st);
        if (o == 2 || (o == 1 && writable)) return &e_fileopen;                              /* FileCore: a file that is open for writing is open for nobody, and a writer needs it not open */
      }
    for (i = 0; i < 64; i++) if (H[i].fd <= 0) break;
    if (i == 64) return &e_toomany;
    fd = open (name, writable ? (how == 0x80 ? O_RDWR | O_CREAT | O_TRUNC : O_RDWR | O_CREAT) : O_RDONLY, 0666);
    if (fd < 0) return errno == ENOENT ? &e_notfound : &e_access;
    fstat (fd, &st);
    H[i].fd = fd; H[i].ptr = 0; H[i].writable = writable; H[i].modified = how == 0x80; H[i].dev = st.st_dev; H[i].ino = st.st_ino;
    r[0] = (unsigned) (H0 + i);
    return 0;
  }
}
static oserr *os_gbpb (unsigned *r, unsigned *fl)
{
  handle *h = get_handle (r[1]);
  unsigned reason = r[0];
  unsigned char *buf = deref (r[2]);
  long n = (long) r[3], got;
  if (!h) return &e_badhandle;
  if (reason < 1 || reason > 4) return &unknown_err;
  if (reason == 1 || reason == 3) h->ptr = (long) r[4];
  if (reason == 1 || reason == 2)
    {
      if (!h->writable) return &e_notupdate;
      if (fault_write) { fault_write = 0; return &e_discfull; }
      got = pwrite (h->fd, buf, (size_t) n, h->ptr);
      if (got < n) return &e_discfull;
      h->modified = 1;                                /* never a short write: a full disc is an error */
    }
  else
    {
      got = pread (h->fd, buf, (size_t) n, h->ptr);
      if (got < 0) got = 0;
    }
  h->ptr += got;
  r[2] += (unsigned) got;                                            /* (the token moves on: not used) */
  r[3] = (unsigned) (n - got);
  r[4] = (unsigned) h->ptr;
  *fl = got < n ? 0x20000000u : 0;
  return 0;
}
static oserr *os_args (unsigned *r)
{
  handle *h;
  if (r[1] == 0) return &unknown_err;
  h = get_handle (r[1]);
  if (!h) return &e_badhandle;
  switch (r[0])
    {
    case 0: r[2] = (unsigned) h->ptr; return 0;
    case 1:
      if (r[2] > extent (h))
        {
          if (!h->writable) return &e_outside;
          if (ftruncate (h->fd, (off_t) r[2])) return &e_discfull;                 /* longer, with zeros */
        }
      h->ptr = (long) r[2];
      return 0;
    case 2: r[2] = extent (h); return 0;
    case 3:
      if (!h->writable) return &e_notupdate;
      if (ftruncate (h->fd, (off_t) r[2])) return &e_discfull;
      h->modified = 1;
      if ((unsigned long) h->ptr > r[2]) h->ptr = (long) r[2];
      return 0;
    case 254: r[0] = 0x40u | (h->writable ? 0x80u : 0) | (h->modified ? 0x100u : 0); r[2] = 0; return 0;
    case 255: if (fault_flush) { fault_flush = 0; return &e_discfull; } return 0;
    default: return &unknown_err;
    }
}

oserr *__modlib_xswif (unsigned swi, unsigned *r, unsigned *flags)
{
  unsigned n = swi & ~0x20000u, fl = 0;
  oserr *err = 0;
  switch (n)
    {
    case 0x5AB00:
      {
	unsigned in0 = r[0], o[10];
	if (r[1] == 0xBAD) { err = &test_err; break; }
	o[0] = r[0] + r[1]; o[1] = r[2] ^ 0x1234; o[2] = r[3] * 3; o[3] = r[4] - r[5]; o[4] = r[6] | r[7]; o[5] = r[8]; o[6] = r[9]; o[7] = in0; o[8] = 0x80000000u; o[9] = 0xFFFFFFFFu;
	memcpy (r, o, sizeof o);
	fl = (o[0] & 0x80000000u) | (o[0] == 0 ? 0x40000000u : 0) | ((in0 & 1) ? 0x20000000u : 0);
	break;
      }
    case 0x5AB01: g_clock = r[0] | ((unsigned long long) (r[1] & 0xFF) << 32); break;
    case 0x5AB02: g_mono = r[0]; break;
    case 0x5AB07: mk_host_fs_ctl ((int) r[0], r[0] == 1 ? (const char *) deref (r[1]) : 0, r[2]); break;
    case 0x42: r[0] = g_mono; break;
    case 0x00: screen_put ((char) r[0]); break;                     /* OS_WriteC */
    case 0x03: screen_put ('\n'); screen_put ('\r'); break;         /* OS_NewLine: a line feed and a carriage return */
    case 0x06: break;                                               /* OS_Byte (acknowledge Escape) */
    case 0x0E:                                                      /* OS_ReadLine */
      if (kpos >= nkeys || keys[kpos].len < 0) { if (kpos < nkeys) kpos++; fl = 0x20000000u; break; }
      {
	int len = keys[kpos].len;
	if (len > (int) r[1]) len = (int) r[1];
	memcpy (deref (r[0]), keys[kpos].s, (size_t) len);
	((char *) deref (r[0]))[len] = '\r';
	r[1] = (unsigned) len;
	kpos++;
      }
      break;
    case 0x0D: err = os_find (r); break;
    case 0x0C: err = os_gbpb (r, &fl); break;
    case 0x09: err = os_args (r); break;
    case 0x08:                                                      /* OS_File */
      {
	const char *name = deref (r[1]);
	struct stat st;
	if (r[0] == 6)                                              /* delete */
	  {
	    if (lstat (name, &st)) { r[0] = 0; break; }
	    r[0] = S_ISDIR (st.st_mode) ? 2 : 1;
	    if (is_locked (name)) err = &e_locked;
	    else if (open_state (&st)) err = &e_fileopen;
	    else if (S_ISDIR (st.st_mode) ? rmdir (name) : unlink (name)) err = errno == ENOTEMPTY ? &e_notempty : &e_access;
	    else attr_forget (name);
	  }
	else if (r[0] == 17)                                        /* read the catalogue information: the type, the length, the attributes */
	  {
	    if (stat (name, &st)) { r[0] = 0; break; }
	    r[0] = S_ISDIR (st.st_mode) ? 2 : 1;
	    r[4] = (unsigned) st.st_size;
	    r[5] = (unsigned) attr_get (name, 1 | ((st.st_mode & 0200) ? 2 : 0) | (is_locked (name) ? 8 : 0));
	  }
	else if (r[0] == 8) { if (mkdir (name, 0777)) err = &e_access; }                    /* create a folder */
	else if (r[0] == 4)                                         /* write the attributes: the write bit is the host's, the lock is the model's */
	  {
	    if (stat (name, &st)) { err = &e_notfound; break; }
	    if (chmod (name, (st.st_mode & ~0222) | ((r[5] & 2) ? 0200 : 0))) err = &e_access;
	    set_locked (name, (r[5] & 8) != 0);
	    attr_set (name, (int) (r[5] & 0xFF));
	  }
	else err = &unknown_err;
	break;
      }
    case 0x29:                                                      /* OS_FSControl */
      if (r[0] == 25)
	{
	  const char *from = deref (r[1]), *to = deref (r[2]);
	  struct stat st;
	  struct stat st2;
	  if (lstat (from, &st)) { err = &e_notfound; break; }
	  if (!lstat (to, &st2) && !(st.st_dev == st2.st_dev && st.st_ino == st2.st_ino)) { err = &e_exists; break; }          /* FileSwitch looks at the destination first (as the Pi showed) ... */
	  if (is_locked (from)) { err = &e_locked; break; }                                                                  /* ... then FileCore at the source */
	  if (open_state (&st)) { err = &e_fileopen; break; }
	  if (rename (from, to)) err = &e_other;
	  else { attr_forget (from); attr_forget (to); }
	}
      else err = &unknown_err;
      break;
    default: err = &unknown_err;
    }
  if (err) { if (flags) *flags = 0x10000000u; return err; }
  if (flags) *flags = fl;
  return 0;
}
oserr *__modlib_xswi (unsigned swi, unsigned *r) { return __modlib_xswif (swi, r, 0); }

/* the hooks that exit.c has on the machine (the host build leaves exit.c out) */
void (*__modlib_stdio_end_hook) (int);
