/* tbtest.cc -- the throwback code of the compiler (riscos-throwback.cc, the very file) built as a program, to try it without a compiler run.
     tbtest names FILE...                 the name that the editor would be given for each FILE (UnixLib's translation, then OS_FSControl 37)
     tbtest send FILE LINE i|w|e|s TEXT   one message (information / warning / error / serious error); nothing is shown on the screen: an editor (or tbsink) has to be
                                          registered with DDEUtils, and THROWBACK_DEBUG (any value) makes the reason visible when it does not work
     tbtest wait LOGFILE TEXT SECONDS     wait until TEXT appears in the file LOGFILE (tbsink's log), at most SECONDS; exit status 0 when it did
     tbtest stop STOPFILE                 create STOPFILE (tbsink ends when it sees it)
   Build:  arm-riscos-gnueabihf-g++ -O2 -DRISCOS_TB_STANDALONE tbtest.cc -o tbtest,e1f  */
#include <unistd.h>
#include <time.h>
#include <fstream>
#include <sstream>
#include "../../../new-files/gcc/config/arm/riscos-throwback.cc"

int main (int argc, char **argv)
{
  if (argc >= 3 && !strcmp (argv[1], "names"))
    {
      for (int i = 2; i < argc; i++)
	printf ("%s  ->  %s\n", argv[i], riscos_tb_test_name (argv[i]).c_str ());
      return 0;
    }
  if (argc >= 6 && !strcmp (argv[1], "send"))
    {
      int sev = argv[4][0] == 'i' ? -1 : argv[4][0] == 'w' ? 0 : argv[4][0] == 's' ? 2 : 1;
      std::string text;
      for (int i = 5; i < argc; i++) { if (i > 5) text += ' '; text += argv[i]; }
      riscos_tb_test_report (argv[2], atoi (argv[3]), sev, text.c_str ());
      riscos_tb_test_finish ();
      printf ("sent: %s:%s severity %d: %s\n", argv[2], argv[3], sev, text.c_str ());
      return 0;
    }
  if (argc == 5 && !strcmp (argv[1], "wait"))
    {
      int secs = atoi (argv[4]);
      for (int t = 0; t <= secs * 5; t++)
	{
	  std::ifstream f (argv[2]);
	  std::stringstream ss;
	  ss << f.rdbuf ();
	  if (ss.str ().find (argv[3]) != std::string::npos) { printf ("found '%s' in %s after %d.%d s\n", argv[3], argv[2], t / 5, (t % 5) * 2); return 0; }
	  usleep (200000);
	}
      printf ("'%s' did not appear in %s within %d s\n", argv[3], argv[2], secs);
      return 1;
    }
  if (argc == 3 && !strcmp (argv[1], "stop"))
    {
      FILE *f = fopen (argv[2], "w");
      if (!f) { printf ("cannot create %s\n", argv[2]); return 1; }
      fputs ("stop\n", f);
      fclose (f);
      printf ("created %s\n", argv[2]);
      return 0;
    }
  printf ("usage: tbtest names FILE... | send FILE LINE i|w|e|s TEXT | wait LOGFILE TEXT SECONDS | stop STOPFILE\n");
  return 2;
}
