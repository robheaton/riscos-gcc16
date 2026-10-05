/* RISC OS EABI needs a frame pointer for non-leaf functions (arm_frame_pointer_required hack). */
extern int g(int);
int leaf(int a, int b) { return a * 3 + b; }
int nonleaf(int a, int b) { return g(a) + leaf(a, b); }
