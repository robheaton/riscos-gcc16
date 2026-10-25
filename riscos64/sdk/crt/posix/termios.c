/* termios.c - the terminal functions (tcgetattr, tcsetattr, cfmakeraw, ...), select and poll, for the console of RISC OS 64.  Only descriptors 0, 1 and 2 are terminals.  The flags ECHO and ICANON of c_lflag and
   VMIN of c_cc are acted on by read (0) in riscos_syscalls.c (without ICANON the keys are returned as they come, without line editing); the other flags are stored and handed back.  select and poll say that
   files are always ready and that the keyboard is ready when its buffer holds a character; the timeouts are kept with OS_ReadMonotonicTime.  The SDK of RISC OS 64. */
#include <errno.h>
#include <sys/types.h>
#include <sys/_types.h>
#include <poll.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

extern int swi_x (unsigned long swi, unsigned long *regs);
extern void __ul64_poll_signals (void);
extern int __riscos_tty_lflag, __riscos_tty_vmin;

static struct termios tty;
static int tty_init;

static void init (void)
{
  if (tty_init) return;
  tty_init = 1;
  memset (&tty, 0, sizeof tty);
  tty.c_iflag = ICRNL | IXON;
  tty.c_oflag = OPOST;
  tty.c_cflag = CS8 | CREAD | HUPCL;
  tty.c_lflag = ECHO | ICANON | ISIG | IEXTEN;
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;
  tty.c_cc[VINTR] = 3; tty.c_cc[VEOF] = 4; tty.c_cc[VERASE] = 127; tty.c_cc[VKILL] = 21; tty.c_cc[VSTART] = 17; tty.c_cc[VSTOP] = 19;
  tty.c_ispeed = tty.c_ospeed = B38400;
}

int tcgetattr (int fd, struct termios *t)
{
  if (fd < 0 || fd > 2) { errno = fd < 0 ? EBADF : ENOTTY; return -1; }
  init ();
  *t = tty;
  return 0;
}

int tcsetattr (int fd, int how, const struct termios *t)
{
  if (fd < 0 || fd > 2) { errno = fd < 0 ? EBADF : ENOTTY; return -1; }
  if (how < TCSANOW || how > TCSAFLUSH) { errno = EINVAL; return -1; }
  init ();
  tty = *t;
  __riscos_tty_lflag = (int) (t->c_lflag & (ECHO | ICANON));
  __riscos_tty_vmin = t->c_cc[VMIN];
  if (how == TCSAFLUSH) tcflush (fd, TCIFLUSH);
  return 0;
}

int tcflush (int fd, int q)
{
  if (fd < 0 || fd > 2) { errno = fd < 0 ? EBADF : ENOTTY; return -1; }
  if (q == TCIFLUSH || q == TCIOFLUSH)
    {
      unsigned long r[10] = { 21, 0 };			/* OS_Byte 21: flush the keyboard buffer */
      swi_x (0x06, r);
    }
  return 0;
}

int tcdrain (int fd) { if (fd < 0 || fd > 2) { errno = fd < 0 ? EBADF : ENOTTY; return -1; } return 0; }
int tcflow (int fd, int a) { (void) a; return tcdrain (fd); }
int tcsendbreak (int fd, int d) { (void) d; return tcdrain (fd); }
pid_t tcgetpgrp (int fd) { if (fd < 0 || fd > 2) { errno = fd < 0 ? EBADF : ENOTTY; return -1; } return 1; }
speed_t cfgetispeed (const struct termios *t) { return t->c_ispeed; }
speed_t cfgetospeed (const struct termios *t) { return t->c_ospeed; }
int cfsetispeed (struct termios *t, speed_t s) { t->c_ispeed = s; return 0; }
int cfsetospeed (struct termios *t, speed_t s) { t->c_ospeed = s; return 0; }

void cfmakeraw (struct termios *t)
{
  t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  t->c_oflag &= ~OPOST;
  t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  t->c_cflag &= ~(CSIZE | PARENB);
  t->c_cflag |= CS8;
  t->c_cc[VMIN] = 1;
  t->c_cc[VTIME] = 0;
}

static int key_waiting (void)
{
  unsigned long r[10] = { 128, 255 };			/* OS_Byte 128: the characters waiting in the keyboard buffer */
  return !swi_x (0x06, r) && (r[1] & 255) != 0;
}

static unsigned long now (void)
{
  unsigned long r[10] = { 0 };
  swi_x (0x42, r);					/* OS_ReadMonotonicTime: centiseconds */
  return r[0];
}

extern int __ul64_is_sock (int) __attribute__ ((weak));			/* sockets.c: descriptors 300 and up */
extern int __ul64_sock_ready (unsigned *, unsigned *, unsigned *, int) __attribute__ ((weak));
#define SOCKFD_BASE 300
#define NSOCKBITS 64						/* the stack numbers its sockets from about 30 upwards: room for 64 */
#define ISSOCK(fd) (__ul64_is_sock && __ul64_is_sock (fd) && (fd) - SOCKFD_BASE < NSOCKBITS)
#define SB_SET(a, n) ((a)[(n) >> 5] |= 1u << ((n) & 31))
#define SB_GET(a, n) (((a)[(n) >> 5] >> ((n) & 31)) & 1)
#define SB_ANY(a) ((a)[0] | (a)[1])

int poll (struct pollfd *fds, nfds_t n, int ms)
{
  unsigned long start = now (), limit = ms > 0 ? (unsigned long) (ms + 9) / 10 : 0;
  for (;;)
    {
      int ready = 0, sockerr = 0;
      unsigned rd[2] = { 0, 0 }, wr[2] = { 0, 0 }, ex[2] = { 0, 0 };
      nfds_t i;
      for (i = 0; i < n; i++)					/* the sockets, looked at together */
	if (fds[i].fd >= 0 && ISSOCK (fds[i].fd))
	  {
	    int sn = fds[i].fd - SOCKFD_BASE;
	    if (fds[i].events & POLLIN) SB_SET (rd, sn);
	    if (fds[i].events & POLLOUT) SB_SET (wr, sn);
	    if (fds[i].events & POLLPRI) SB_SET (ex, sn);
	  }
      if ((SB_ANY (rd) | SB_ANY (wr) | SB_ANY (ex)) && __ul64_sock_ready (rd, wr, ex, NSOCKBITS) < 0) sockerr = 1;
      for (i = 0; i < n; i++)
	{
	  short want = fds[i].events, got = 0;
	  int fd = fds[i].fd;
	  if (fd < 0) { fds[i].revents = 0; continue; }
	  if (ISSOCK (fd))
	    {
	      int sn = fd - SOCKFD_BASE;
	      if (sockerr) got = POLLERR;
	      else { if (SB_GET (rd, sn)) got |= POLLIN; if (SB_GET (wr, sn)) got |= POLLOUT; if (SB_GET (ex, sn)) got |= POLLPRI; }
	      fds[i].revents = got;
	      if (got) ready++;
	      continue;
	    }
	  if (fd == 0) { if ((want & POLLIN) && key_waiting ()) got |= POLLIN; }
	  else if (fd == 1 || fd == 2) got |= want & POLLOUT;
	  else got |= want & (POLLIN | POLLOUT);
	  fds[i].revents = got;
	  if (got) ready++;
	}
      if (ready || ms == 0) return ready;
      __ul64_poll_signals ();
      if (ms > 0 && now () - start >= limit) return 0;
    }
}

int select (int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
  unsigned long start = now (), limit = tv ? (unsigned long) (tv->tv_sec * 100 + (tv->tv_usec + 9999) / 10000) : 0;
  fd_set ri, wi;
  int fd;
  if (nfds < 0 || nfds > FD_SETSIZE) { errno = EINVAL; return -1; }
  if (r) ri = *r; else FD_ZERO (&ri);
  if (w) wi = *w; else FD_ZERO (&wi);
  for (;;)
    {
      int ready = 0;
      unsigned srd[2] = { 0, 0 }, swr[2] = { 0, 0 }, sex[2] = { 0, 0 };
      fd_set ro, wo, eo;
      FD_ZERO (&ro); FD_ZERO (&wo); FD_ZERO (&eo);
      for (fd = 0; fd < nfds; fd++)				/* the sockets, looked at together */
	if (ISSOCK (fd))
	  {
	    int sn = fd - SOCKFD_BASE;
	    if (FD_ISSET (fd, &ri)) SB_SET (srd, sn);
	    if (FD_ISSET (fd, &wi)) SB_SET (swr, sn);
	    if (e && FD_ISSET (fd, e)) SB_SET (sex, sn);
	  }
      if ((SB_ANY (srd) | SB_ANY (swr) | SB_ANY (sex)) && __ul64_sock_ready (srd, swr, sex, NSOCKBITS) < 0) return -1;
      for (fd = 0; fd < nfds; fd++)
	{
	  if (ISSOCK (fd))
	    {
	      int sn = fd - SOCKFD_BASE;
	      if (SB_GET (srd, sn)) { FD_SET (fd, &ro); ready++; }
	      if (SB_GET (swr, sn)) { FD_SET (fd, &wo); ready++; }
	      if (SB_GET (sex, sn)) { FD_SET (fd, &eo); ready++; }
	      continue;
	    }
	  if (FD_ISSET (fd, &ri) && (fd == 0 ? key_waiting () : fd > 2)) { FD_SET (fd, &ro); ready++; }
	  if (FD_ISSET (fd, &wi) && fd != 0) { FD_SET (fd, &wo); ready++; }
	}
      __ul64_poll_signals ();
      if (ready || (tv && now () - start >= limit))
	{
	  if (r) *r = ro;
	  if (w) *w = wo;
	  if (e) *e = eo;
	  return ready;
	}
    }
}
