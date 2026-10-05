/* five values live across a call: needs several callee-saved registers */
extern int g(int);
int many(int a, int b, int c, int d, int e) { int x = g(a); return x + a + b + c + d + e; }
