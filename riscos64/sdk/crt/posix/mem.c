/* mem.c - mmap and munmap for what programs use them for: anonymous memory, and a private copy of a file (it is read into the block, not mapped).  Blocks come from malloc, whose heap is a dynamic area, and are page
   aligned; munmap frees a block that is given back whole, anything else is left (the memory is still the program's).  The SDK of RISC OS 64. */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE 4096UL
struct hdr { void *raw; size_t len; };

void *mmap (void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
  size_t size = (len + PAGE - 1) & ~(PAGE - 1);
  char *raw, *base;
  struct hdr *h;
  (void) addr; (void) prot;
  if (len == 0) { errno = EINVAL; return MAP_FAILED; }
  if (!(flags & MAP_ANONYMOUS) && (fd < 0 || (flags & MAP_SHARED))) { errno = fd < 0 ? EBADF : ENODEV; return MAP_FAILED; }
  raw = (char *) malloc (size + 2 * PAGE);
  if (!raw) { errno = ENOMEM; return MAP_FAILED; }
  base = (char *) (((uintptr_t) raw + PAGE + PAGE - 1) & ~(PAGE - 1));
  memset (base, 0, size);
  h = (struct hdr *) base - 1;
  h->raw = raw; h->len = size;
  if (!(flags & MAP_ANONYMOUS))
    {
      size_t got = 0;
      off_t here = lseek (fd, 0, SEEK_CUR);
      if (lseek (fd, off, SEEK_SET) < 0) { free (raw); errno = EINVAL; return MAP_FAILED; }
      while (got < len) { ssize_t n = read (fd, base + got, len - got); if (n <= 0) break; got += (size_t) n; }
      lseek (fd, here, SEEK_SET);
    }
  return base;
}

int munmap (void *addr, size_t len)
{
  struct hdr *h;
  if (!addr || ((uintptr_t) addr & (PAGE - 1))) { errno = EINVAL; return -1; }
  h = (struct hdr *) addr - 1;
  if (h->len == ((len + PAGE - 1) & ~(PAGE - 1)) && h->raw && (char *) h->raw < (char *) addr) { void *raw = h->raw; h->raw = 0; free (raw); }
  return 0;
}

int mprotect (void *addr, size_t len, int prot) { (void) addr; (void) len; (void) prot; return 0; }
int msync (void *addr, size_t len, int flags) { (void) addr; (void) len; (void) flags; return 0; }
