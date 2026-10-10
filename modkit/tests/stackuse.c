/* stackuse.c - how much stack the conversions of the kit use: the stack below main is painted, a function is called, and what it overwrote is counted.  Built with the cross compiler (gcc -mmodule) and run
   on the A32 interpreter (libtest/armrun.py):   arm-riscos-gnueabihf-gcc -mmodule -O2 -o stackuse.elf stackuse.c && python3 libtest/armrun.py stackuse.elf
   The figures are the bytes between the stack pointer of main and the lowest address that the call wrote (+- 16 bytes: the frames of paint () and measure () are the same size). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PAINT 0xA5A5A5A5u
#define WORDS 3072

static void __attribute__ ((noinline)) paint (void)
{
  volatile unsigned a[WORDS];
  int i;
  for (i = 0; i < WORDS; i++) a[i] = PAINT;
}
static int __attribute__ ((noinline)) measure (void)
{
  volatile unsigned a[WORDS];
  int i;
  for (i = 0; i < WORDS && a[i] == PAINT; i++) ;
  return (WORDS - i) * 4;
}

static char big[1200];
static volatile double sink_d;
static volatile int sink_i;
static char out[1400];

#define USE(name, call) do { paint (); call; printf ("%-52s %5d bytes\n", name, measure ()); } while (0)

int main (void)
{
  double d = 0; float f = 0; int n, i;
  char *e;
  static const char *ARGS_NUM = "123.456e7";

  USE ("strtod (\"1.5\")", sink_d = strtod ("1.5", &e));
  USE ("strtod (\"123.456e7\")", sink_d = strtod (ARGS_NUM, &e));
  USE ("strtof (\"123.456e7\")", f = strtof (ARGS_NUM, &e));
  for (i = 0; i < 400; i++) big[i] = (char) ('1' + i % 9);
  big[400] = 0;
  USE ("strtod (400 digits)", sink_d = strtod (big, &e));
  for (i = 0; i < 780; i++) big[i] = (char) ('1' + i % 9);
  big[780] = 0;
  USE ("strtod (780 digits)", sink_d = strtod (big, &e));
  strcpy (big, "1e-320");
  USE ("strtod (\"1e-320\")", sink_d = strtod (big, &e));
  strcpy (big, "0x1.8p-1070");
  USE ("strtod (\"0x1.8p-1070\")", sink_d = strtod (big, &e));
  USE ("sscanf (\"1.5\", \"%lf\")", sink_i = sscanf ("1.5", "%lf", &d));
  USE ("sscanf (\"1.5 7\", \"%lf %d\")", sink_i = sscanf ("1.5 7", "%lf %d", &d, &n));
  USE ("sscanf (\"123\", \"%d\")", sink_i = sscanf ("123", "%d", &n));
  USE ("sscanf (\"abc\", \"%[a-c]\")", sink_i = sscanf ("abc", "%[a-c]", big));
  for (i = 0; i < 61; i++) big[i] = (char) ('1' + i % 9);
  big[61] = 0;
  USE ("sscanf (61 digits, \"%lf\") (no malloc)", sink_i = sscanf (big, "%lf", &d));
  for (i = 0; i < 100; i++) big[i] = (char) ('1' + i % 9);
  big[100] = 0;
  USE ("sscanf (100 digits, \"%lf\") (malloc)", sink_i = sscanf (big, "%lf", &d));
  USE ("snprintf (\"%d\", 12345)", sink_i = snprintf (out, sizeof out, "%d", 12345));
  USE ("snprintf (\"%f\", 1.5)", sink_i = snprintf (out, sizeof out, "%f", 1.5));
  USE ("snprintf (\"%.17g\", 0.1)", sink_i = snprintf (out, sizeof out, "%.17g", 0.1));
  USE ("snprintf (\"%e\", 1e300)", sink_i = snprintf (out, sizeof out, "%e", 1e300));
  USE ("snprintf (\"%.1000f\", 1e300)", sink_i = snprintf (out, sizeof out, "%.1000f", 1e300));
  USE ("snprintf (\"%.1100e\", 5e-324)", sink_i = snprintf (out, sizeof out, "%.1100e", 5e-324));
  USE ("snprintf (\"%a\", 0.1)", sink_i = snprintf (out, sizeof out, "%a", 0.1));
  d = 0.7;
  USE ("sqrt (0.7)", sink_d = sqrt (d));
  USE ("exp (0.7)", sink_d = exp (d));
  USE ("log (0.7)", sink_d = log (d));
  USE ("pow (0.7, 2.5)", sink_d = pow (d, 2.5));
  USE ("sin (0.7)", sink_d = sin (d));
  d = 1e22;
  USE ("sin (1e22) (the big argument reduction)", sink_d = sin (d));
  USE ("tan (1e22)", sink_d = tan (d));
  d = 0.7;
  USE ("atan2 (0.7, -0.3)", sink_d = atan2 (d, -0.3));
  USE ("fmod (1e300, 0.7)", sink_d = fmod (1e300, d));
  USE ("cbrt (0.7)", sink_d = cbrt (d));
  USE ("expm1 (0.7)", sink_d = expm1 (d));
  USE ("hypot (0.7, 1.9)", sink_d = hypot (d, 1.9));
  (void) f;
  return 0;
}
