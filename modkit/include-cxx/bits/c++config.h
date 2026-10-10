// bits/c++config.h for modules: libstdc++'s own, with the explicit instantiations switched off.  The library says (extern template) that std::string, std::basic_ostream and the like are instantiated in libstdc++.a
// and that the code of a program need not do it; a module does not have those members (those of this tool chain's libstdc++.a reach their data through the shared library model of UnixLib: a table at
// 0x8000 that only a program has, and libstdcxx-mod.a leaves them out), so the code of the module instantiates them itself, as the language says it may.
#ifndef _MODKIT_CXXCONFIG_H
#define _MODKIT_CXXCONFIG_H 1
#include_next <bits/c++config.h>
#undef _GLIBCXX_EXTERN_TEMPLATE
#define _GLIBCXX_EXTERN_TEMPLATE 0
#endif
