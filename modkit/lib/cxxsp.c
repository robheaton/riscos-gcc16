/* cxxsp.c - std::_Sp_make_shared_tag::_S_eq, which std::make_shared uses to recognise its own deleter (libstdc++'s member is not in libstdcxx-mod.a: it contains a barrier instruction).  The argument is the
   address of the static tag that the header makes in each translation unit that uses make_shared; this object is linked only for a program that does, so the tag is there. */
#pragma GCC optimize ("Os")
/* std::_Sp_make_shared_tag::_S_eq (const std::type_info &): is it the tag of make_shared? */
extern const char _ZZNSt19_Sp_make_shared_tag5_S_tiEvE5__tag[];
int _ZNSt19_Sp_make_shared_tag5_S_eqERKSt9type_info (const void *ti) { return ti == (const void *) _ZZNSt19_Sp_make_shared_tag5_S_tiEvE5__tag; }
