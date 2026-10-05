/* C23 features: build with -std=gnu23 (GCC 15+/16 only). */
constexpr int c23_k = 7;

int c23_probe(int x)
{
    auto y = x * c23_k;
    typeof(y) z = y + 1;
    bool b = z > 10;
    int *p = nullptr;
    return (p == nullptr && b) ? z : -z;
}
