#include <stdarg.h>
double mix(double a, float b, int c) { return a * b + c; }
double sum(int n, ...) { va_list ap; double t = 0; va_start(ap, n); while (n--) t += va_arg(ap, double); va_end(ap); return t; }
struct pt { int x, y; double w; };
struct pt make(int x, int y) { struct pt p = { x, y, x * 0.5 + y }; return p; }
