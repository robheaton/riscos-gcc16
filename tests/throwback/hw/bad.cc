// bad.cc -- C++ errors: overload resolution (a message with several lines), a missing member, an unused variable
struct A { int x; };
void take (A &, int);
void take (A &, double, int);

int main ()
{
  A a;
  take (a, "string");
  int unused_var;
  return a.y;
}
