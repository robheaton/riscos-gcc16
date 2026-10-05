/* C11 _Thread_local, used from C++ */
static _Thread_local int x = 7;
int c_tls_bump(void) { return ++x; }
