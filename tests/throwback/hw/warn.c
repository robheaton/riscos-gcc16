/* warn.c -- warnings only: compiles, and the throwback holds only warnings (with -Wall) */
int h (int a)
{
  int unused1;
  int unused2 = 3;
  return a;
}

int main (void)
{
  return h (0);
}
