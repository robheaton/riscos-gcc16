/* ieee754.h - the layout of the IEEE 754 numbers (glibc's header; AArch64: little endian, long double is the 128 bit quad format) */
#ifndef _IEEE754_H
#define _IEEE754_H 1
#ifdef __cplusplus
extern "C" {
#endif
union ieee754_float
{
  float f;
  struct { unsigned int mantissa:23; unsigned int exponent:8; unsigned int negative:1; } ieee;
  struct { unsigned int mantissa:22; unsigned int quiet_nan:1; unsigned int exponent:8; unsigned int negative:1; } ieee_nan;
};
#define IEEE754_FLOAT_BIAS 0x7f
union ieee754_double
{
  double d;
  struct { unsigned int mantissa1:32; unsigned int mantissa0:20; unsigned int exponent:11; unsigned int negative:1; } ieee;
  struct { unsigned int mantissa1:32; unsigned int mantissa0:19; unsigned int quiet_nan:1; unsigned int exponent:11; unsigned int negative:1; } ieee_nan;
};
#define IEEE754_DOUBLE_BIAS 0x3ff
union ieee854_long_double
{
  long double d;
  struct { unsigned int mantissa3:32; unsigned int mantissa2:32; unsigned int mantissa1:32; unsigned int mantissa0:16; unsigned int exponent:15; unsigned int negative:1; } ieee;
  struct { unsigned int mantissa3:32; unsigned int mantissa2:32; unsigned int mantissa1:32; unsigned int mantissa0:15; unsigned int quiet_nan:1; unsigned int exponent:15; unsigned int negative:1; } ieee_nan;
};
#define IEEE854_LONG_DOUBLE_BIAS 0x3fff
#ifdef __cplusplus
}
#endif
#endif
