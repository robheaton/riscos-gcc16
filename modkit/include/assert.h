/* assert.h - assert (e): when e is false the message "Assertion failed: e, file F, line N" is written to stdout (printf: OS_WriteC and OS_NewLine) and the module is stopped with an error (abort ()).  NDEBUG switches it off.
   Like the standard header this one can be included again after defining or undefining NDEBUG. */
#undef assert
#ifdef NDEBUG
#define assert(e)	((void) 0)
#else
extern void __modlib_assert (const char *expr, const char *file, int line) __attribute__ ((noreturn));
#define assert(e)	((e) ? (void) 0 : __modlib_assert (#e, __FILE__, __LINE__))
#endif
