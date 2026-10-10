/* cxxeh.c - the entry points of the C++ exception runtime that the code of libstdc++ names (its members were compiled with exceptions, and libstdcxx-mod.a has them without the runtime: install-modkit.sh
   leaves the eh_*.o members out).  A module has no exceptions: std::__throw_* (cxxrt.c) end the call, so nothing throws, and these are never reached; if one is, the call ends the same way (abort). */
#pragma GCC optimize ("Os")
#include <stdlib.h>
void __cxa_begin_catch (void) { abort (); }
void __cxa_end_catch (void) { abort (); }
void __cxa_rethrow (void) { abort (); }
void __cxa_throw (void) { abort (); }
void __cxa_end_cleanup (void) { abort (); }
void __cxa_call_unexpected (void) { abort (); }
void __gxx_personality_v0 (void) { abort (); }
void _Unwind_Resume (void) { abort (); }
void _ZSt9terminatev (void) { abort (); }                                     /* std::terminate () */
