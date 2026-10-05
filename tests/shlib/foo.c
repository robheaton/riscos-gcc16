static int table[3] = {10, 20, 30};
int foo_counter;
int foo_add(int n) { foo_counter += n; return table[n % 3] + foo_counter; }
