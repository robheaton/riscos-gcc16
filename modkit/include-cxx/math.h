// math.h for C++ in a module: the C header of the kit (through the cmath of this folder, which includes it), and the overloads of std:: in the global namespace as the C++ library has them.
#ifndef _MODKIT_MATH_H_CXX
#define _MODKIT_MATH_H_CXX 1
#include <cmath>
using std::abs; using std::acos; using std::acosh; using std::asin; using std::asinh; using std::atan; using std::atan2; using std::atanh; using std::cbrt; using std::ceil; using std::copysign;
using std::cos; using std::cosh; using std::exp; using std::exp2; using std::expm1; using std::fabs; using std::fdim; using std::floor; using std::fmax; using std::fmin; using std::fmod; using std::frexp;
using std::hypot; using std::ilogb; using std::ldexp; using std::llrint; using std::llround; using std::log; using std::log10; using std::log1p; using std::log2; using std::logb; using std::lrint;
using std::lround; using std::modf; using std::nearbyint; using std::nextafter; using std::pow; using std::remainder; using std::rint; using std::round; using std::scalbn; using std::sin; using std::sinh;
using std::sqrt; using std::tan; using std::tanh; using std::trunc;
using std::fpclassify; using std::isfinite; using std::isgreater; using std::isgreaterequal; using std::isinf; using std::isless; using std::islessequal; using std::islessgreater; using std::isnan;
using std::isnormal; using std::isunordered; using std::signbit;
#endif
