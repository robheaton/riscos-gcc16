/* Exercises the RISC OS PIC register (GOTT) load: global data, function pointers, calls. */
extern int ext_counter;
static int local_table[4] = {1, 2, 3, 4};
int (*hook)(int);
int bump(int n) { ext_counter += n; return local_table[n & 3] + ext_counter; }
int call_hook(int n) { return hook ? hook(n) + bump(n) : bump(n); }
