/* t_termcap.c - the termcap functions over the built-in "riscos" entry, and the size of the screen. */
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termcap.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("ok   %s\n", #c); } while (0)
int main (void)
{
  char area[512], *p = area, *cm, *so;
  struct winsize ws;
  CHECK (tgetent (0, "riscos") == 1);
  CHECK (tgetent (0, "xterm") == 1);			/* any name gets the riscos entry */
  CHECK (tgetflag ("am") == 1 && tgetflag ("xx") == 0);
  CHECK (tgetnum ("li") > 0 && tgetnum ("co") > 0);
  printf ("INFO screen %d x %d\n", tgetnum ("co"), tgetnum ("li"));
  cm = tgetstr ("cm", &p);
  CHECK (cm && !strcmp (cm, "\037%r%.%."));
  CHECK (!strcmp (tgoto (cm, 7, 3), "\037\007\003"));	/* VDU 31 x y: column 7, row 3 */
  CHECK (!strcmp (tgetstr ("cl", &p), "\014"));
  so = tgetstr ("so", &p);
  CHECK (so && strlen (so) == 4 && so[0] == 17 && (unsigned char) so[3] == 0207);
  CHECK (tgetstr ("ce", &p) && (unsigned char) tgetstr ("ce", &p)[0] == 0xFF);
  CHECK ((unsigned char) tgetstr ("ku", &p)[0] == 0217);
  CHECK (!strcmp (tgoto ("%d;%d", 5, 2), "2;5") && !strcmp (tgoto ("%+ %+ ", 1, 2), "\042\041") );
  memset (&ws, 0, sizeof ws);
  CHECK (ioctl (1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0);
  printf ("INFO winsize %d x %d\n", ws.ws_col, ws.ws_row);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
