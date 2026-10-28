/* fficheck - libffi on RISC OS (hard-float ARM): calls with integer, float and double arguments, a struct, and a closure */
#include <stdio.h>
#include <string.h>
#include <ffi.h>

static int fails;
#define CHECK(c, what) do { int ok_ = (c); printf ("  %-44s %s\n", what, ok_ ? "ok" : "FAILED"); if (!ok_) fails++; } while (0)

static double mix (int a, double b, float c, long long d, char e) { return a + b * 2 + c * 3 + (double) d * 4 + e; }
struct pt { double x, y; int n; };
static struct pt grow (struct pt p, int k) { struct pt r = { p.x * k, p.y + k, p.n + 1 }; return r; }
static int add3 (int a, int b, int c) { return a + b + c; }

static void closure_fn (ffi_cif *cif, void *ret, void **args, void *user)
{
  (void) cif;
  *(double *) ret = *(int *) args[0] * *(double *) args[1] + *(int *) user;
}

int main (void)
{
  ffi_cif cif;
  printf ("libffi %s, default ABI %d (FFI_VFP is %d)\n", ffi_get_version (), (int) FFI_DEFAULT_ABI, (int) FFI_VFP);
  {
    ffi_type *at[5] = { &ffi_type_sint, &ffi_type_double, &ffi_type_float, &ffi_type_sint64, &ffi_type_schar };
    int a = 7; double b = 1.5; float c = 2.25f; long long d = 1000000000000LL; char e = 'A';
    void *av[5] = { &a, &b, &c, &d, &e };
    double r = 0;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 5, &ffi_type_double, at) == FFI_OK, "prep_cif (int, double, float, int64, char)");
    ffi_call (&cif, FFI_FN (mix), &r, av);
    CHECK (r == mix (a, b, c, d, e), "call: mixed arguments, double result");
  }
  {
    ffi_type *elems[4] = { &ffi_type_double, &ffi_type_double, &ffi_type_sint, 0 };
    ffi_type pt_t = { 0, 0, FFI_TYPE_STRUCT, elems };
    ffi_type *at[2] = { &pt_t, &ffi_type_sint };
    struct pt p = { 1.5, 2.5, 3 }, r;
    int k = 4;
    void *av[2] = { &p, &k };
    struct pt want = grow (p, k);
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &pt_t, at) == FFI_OK, "prep_cif (struct by value)");
    ffi_call (&cif, FFI_FN (grow), &r, av);
    CHECK (r.x == want.x && r.y == want.y && r.n == want.n, "call: struct in, struct out");
  }
  {
    ffi_type *at[3] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_sint };
    int a = 1, b = 20, c = 300; void *av[3] = { &a, &b, &c }; ffi_arg r = 0;
    ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 3, &ffi_type_sint, at);
    ffi_call (&cif, FFI_FN (add3), &r, av);
    CHECK ((int) r == 321, "call: three ints");
  }
  {
    ffi_closure *cl;
    void *code;
    ffi_cif ccif;
    ffi_type *at[2] = { &ffi_type_sint, &ffi_type_double };
    int user = 5;
    cl = ffi_closure_alloc (sizeof (ffi_closure), &code);
    CHECK (cl != 0, "closure_alloc");
    if (cl)
      {
	double (*fn) (int, double);
	ffi_prep_cif (&ccif, FFI_DEFAULT_ABI, 2, &ffi_type_double, at);
	CHECK (ffi_prep_closure_loc (cl, &ccif, closure_fn, &user, code) == FFI_OK, "prep_closure_loc");
	fn = (double (*) (int, double)) code;
	CHECK (fn (6, 2.5) == 6 * 2.5 + 5, "closure called as an ordinary C function");
	CHECK (fn (-3, 0.5) == -3 * 0.5 + 5, "closure called again");
	ffi_closure_free (cl);
      }
  }
  printf ("fficheck: %s\n", fails ? "FAILED" : "all passed");
  return fails != 0;
}
