/* sfcheck.c - is the Sunfish filing system registered and answering?  A UnixLib program (the cross compiler of 16.2.0-22).  It asks FileSwitch for the catalogue information of a file on a disc that is not
   mounted: a registered Sunfish answers "Disc not found" (its own error), a missing one gives "Filing system not found" or similar.  Then it calls the module's SWI Sunfish_ListMounts (&58BC2) and
   Sunfish_Free reason 1.  It changes nothing: no mount is made, nothing is written. */
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>

int main (void)
{
  _kernel_oserror *e;
  int objtype = -1, a = -1, b = -1, c = -1;
  char name[64];

  e = _swix (OS_File, _INR (0, 1) | _OUT (0), 17, "Sunfish::NoSuchDisc.$.x", &objtype);
  printf ("1. OS_File 17 on Sunfish::NoSuchDisc.$.x: %s\n", e ? e->errmess : "no error (object type is shown below)");
  if (!e) printf ("   object type %d\n", objtype);
  e = _swix (0x58BC2, _IN (0) | _OUTR (0, 2), 0, &a, &b, &c);
  printf ("2. Sunfish_ListMounts: %s, next %#x, disc name %#x, special field %#x\n", e ? e->errmess : "no error", a, b, c);
  memset (name, 0, sizeof name);
  e = _swix (0x58BC3, _INR (0, 3), 1, 0, name, "Nothing");
  printf ("3. Sunfish_Free reason 1: %s, name '%s'\n", e ? e->errmess : "no error", name);
  printf ("sfcheck: done\n");
  return 0;
}
