/* bench.c -- compute benchmark with self-checking checksums, for comparing compiler versions / flags on RISC OS.
 *
 * Only exactly-rounded IEEE operations (+ - * / sqrt) are used and floating-point contraction must be off
 * (-ffp-contract=off), so every build must print the same checksum for a kernel: a "MISMATCH" means a
 * miscompilation (or a different FPU behaviour).  Timing uses clock() (10 ms resolution) and runs each kernel
 * repeatedly for at least one second, so the ms/run figures are good to about 1%.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef ROTEST_CFG
#define ROTEST_CFG "unknown"
#endif

static uint64_t lcg_state;
static inline uint32_t rnd32(void)
{
    lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(lcg_state >> 32);
}
static inline uint32_t ror32(uint32_t x, unsigned n) { return (x >> (n & 31)) | (x << ((32 - n) & 31)); }
static uint64_t dbits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }

/* ---------------------------------------------------------------- 1. SHA-256 */
static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
static void sha256_block(uint32_t h[8], const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, hh;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) | ((uint32_t)p[4 * i + 2] << 8) | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
static void sha256(const uint8_t *msg, size_t len, uint32_t out[8])
{
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    size_t i = 0;
    for (; i + 64 <= len; i += 64) sha256_block(h, msg + i);
    uint8_t tail[128];
    size_t rem = len - i;
    memcpy(tail, msg + i, rem);
    tail[rem++] = 0x80;
    size_t padded = (rem <= 56) ? 64 : 128;
    memset(tail + rem, 0, padded - rem);
    uint64_t bits = (uint64_t)len * 8;
    for (int k = 0; k < 8; k++) tail[padded - 1 - k] = (uint8_t)(bits >> (8 * k));
    for (size_t o = 0; o < padded; o += 64) sha256_block(h, tail + o);
    memcpy(out, h, sizeof h);
}
static uint8_t g_buf[1 << 20];
static uint64_t k_sha256(void)
{
    lcg_state = 1;
    for (size_t i = 0; i < sizeof g_buf; i++) g_buf[i] = (uint8_t)(rnd32() >> 24);
    uint32_t acc[8] = { 0 }, d[8];
    for (int pass = 0; pass < 6; pass++) {
        g_buf[0] = (uint8_t)pass;
        sha256(g_buf, sizeof g_buf, d);
        for (int i = 0; i < 8; i++) acc[i] ^= d[i];
    }
    return ((uint64_t)acc[0] << 32) | acc[1];
}

/* ---------------------------------------------------------------- 2. CRC-32 */
static uint32_t crc_tab[256];
static uint64_t k_crc32(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
        crc_tab[i] = c;
    }
    lcg_state = 2;
    for (size_t i = 0; i < sizeof g_buf; i++) g_buf[i] = (uint8_t)(rnd32() >> 20);
    uint32_t crc = 0xFFFFFFFFu;
    for (int pass = 0; pass < 8; pass++)
        for (size_t i = 0; i < sizeof g_buf; i++) crc = crc_tab[(crc ^ g_buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* ---------------------------------------------------------------- 3. sieve */
#define SIEVE_N 4000000
static uint8_t g_sieve[SIEVE_N + 1];
static uint64_t k_sieve(void)
{
    memset(g_sieve, 1, sizeof g_sieve);
    g_sieve[0] = g_sieve[1] = 0;
    for (uint32_t i = 2; (uint64_t)i * i <= SIEVE_N; i++)
        if (g_sieve[i])
            for (uint32_t j = i * i; j <= SIEVE_N; j += i) g_sieve[j] = 0;
    uint64_t count = 0, last = 0;
    for (uint32_t i = 0; i <= SIEVE_N; i++)
        if (g_sieve[i]) { count++; last = i; }
    return (count << 32) | last;
}

/* ---------------------------------------------------------------- 4. quicksort */
#define SORT_N 300000
static uint32_t g_arr[SORT_N];
static void qsort_u32(uint32_t *a, int lo, int hi)
{
    while (hi - lo > 16) {
        uint32_t p = a[(lo + hi) / 2], t;
        int i = lo, j = hi;
        while (i <= j) {
            while (a[i] < p) i++;
            while (a[j] > p) j--;
            if (i <= j) { t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
        }
        if (j - lo < hi - i) { qsort_u32(a, lo, j); lo = i; } else { qsort_u32(a, i, hi); hi = j; }
    }
    for (int i = lo + 1; i <= hi; i++) {
        uint32_t v = a[i];
        int j = i - 1;
        while (j >= lo && a[j] > v) { a[j + 1] = a[j]; j--; }
        a[j + 1] = v;
    }
}
static uint64_t k_sort(void)
{
    lcg_state = 3;
    for (int i = 0; i < SORT_N; i++) g_arr[i] = rnd32();
    qsort_u32(g_arr, 0, SORT_N - 1);
    uint64_t s = 0;
    for (int i = 0; i < SORT_N; i++) s += (uint64_t)g_arr[i] * (uint64_t)(i + 1);
    for (int i = 1; i < SORT_N; i++) if (g_arr[i - 1] > g_arr[i]) return 0xBAD;
    return s;
}

/* ---------------------------------------------------------------- 5. matrix multiply (double and float) */
#define MM_N 128
static double Ad[MM_N][MM_N], Bd[MM_N][MM_N], Cd[MM_N][MM_N];
static float Af[MM_N][MM_N], Bf[MM_N][MM_N], Cf[MM_N][MM_N];
static uint64_t k_matmul_d(void)
{
    for (int i = 0; i < MM_N; i++) for (int j = 0; j < MM_N; j++) { Ad[i][j] = (double)((i * 7 + j * 3) % 17) * 0.125; Bd[i][j] = (double)((i * 5 + j * 11) % 13) * 0.25 - 1.0; Cd[i][j] = 0.0; }
    for (int i = 0; i < MM_N; i++) for (int k = 0; k < MM_N; k++) { double a = Ad[i][k]; for (int j = 0; j < MM_N; j++) Cd[i][j] += a * Bd[k][j]; }
    uint64_t x = 0;
    for (int i = 0; i < MM_N; i++) for (int j = 0; j < MM_N; j++) x = (x ^ dbits(Cd[i][j])) * 0x100000001b3ULL;
    return x;
}
static uint64_t k_matmul_f(void)
{
    for (int i = 0; i < MM_N; i++) for (int j = 0; j < MM_N; j++) { Af[i][j] = (float)((i * 7 + j * 3) % 17) * 0.125f; Bf[i][j] = (float)((i * 5 + j * 11) % 13) * 0.25f - 1.0f; Cf[i][j] = 0.0f; }
    for (int i = 0; i < MM_N; i++) for (int k = 0; k < MM_N; k++) { float a = Af[i][k]; for (int j = 0; j < MM_N; j++) Cf[i][j] += a * Bf[k][j]; }
    uint64_t x = 0;
    for (int i = 0; i < MM_N; i++) for (int j = 0; j < MM_N; j++) { uint32_t u; memcpy(&u, &Cf[i][j], 4); x = (x ^ (uint64_t)u) * 0x100000001b3ULL; }
    return x;
}

/* ---------------------------------------------------------------- 6. FFT (radix-2, complex double) */
#define FFT_N 4096
static double fre[FFT_N], fim[FFT_N], tre[FFT_N / 2], tim[FFT_N / 2];
static uint64_t k_fft(void)
{
    /* twiddles by repeated complex multiplication (exact IEEE ops only) */
    const double wr = 0.99999882345170190992, wi = -0.00153398018628476562;   /* e^(-2 pi i / 4096) */
    double cr = 1.0, ci = 0.0;
    for (int i = 0; i < FFT_N / 2; i++) { tre[i] = cr; tim[i] = ci; double t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t; }
    uint64_t x = 0;
    for (int rep = 0; rep < 6; rep++) {
        lcg_state = 4 + (uint64_t)rep;
        for (int i = 0; i < FFT_N; i++) { fre[i] = (double)(int32_t)rnd32() * (1.0 / 2147483648.0); fim[i] = 0.0; }
        for (int i = 1, j = 0; i < FFT_N; i++) {            /* bit reversal */
            int bit = FFT_N >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { double t = fre[i]; fre[i] = fre[j]; fre[j] = t; t = fim[i]; fim[i] = fim[j]; fim[j] = t; }
        }
        for (int len = 2; len <= FFT_N; len <<= 1) {
            int step = FFT_N / len;
            for (int i = 0; i < FFT_N; i += len)
                for (int k = 0; k < len / 2; k++) {
                    double ur = fre[i + k], ui = fim[i + k];
                    double xr = fre[i + k + len / 2], xi = fim[i + k + len / 2];
                    double vr = xr * tre[k * step] - xi * tim[k * step], vi = xr * tim[k * step] + xi * tre[k * step];
                    fre[i + k] = ur + vr; fim[i + k] = ui + vi;
                    fre[i + k + len / 2] = ur - vr; fim[i + k + len / 2] = ui - vi;
                }
        }
        for (int i = 0; i < FFT_N; i++) x ^= (dbits(fre[i]) + 3 * dbits(fim[i])) * (uint64_t)(i + 1);
    }
    return x;
}

/* ---------------------------------------------------------------- 7. Mandelbrot */
static uint64_t k_mandel(void)
{
    uint64_t total = 0;
    for (int py = 0; py < 96; py++)
        for (int px = 0; px < 128; px++) {
            double cx = -2.0 + px * (3.0 / 128.0), cy = -1.0 + py * (2.0 / 96.0), zx = 0.0, zy = 0.0;
            int n = 0;
            while (n < 300 && zx * zx + zy * zy <= 4.0) { double t = zx * zx - zy * zy + cx; zy = 2.0 * zx * zy + cy; zx = t; n++; }
            total += (uint64_t)n * (uint64_t)(px + 1);
        }
    return total;
}

/* ---------------------------------------------------------------- 8. n-body (uses sqrt) */
struct body { double x, y, z, vx, vy, vz, m; };
static struct body bodies[5];
static uint64_t k_nbody(void)
{
    static const struct body init[5] = {
        { 0, 0, 0, 0, 0, 0, 39.47841760435743 },
        { 4.84143144246472090, -1.16032004402742839, -0.103622044471123109, 0.606326392995832, 2.81198684491626, -0.02521836165988763, 0.03769367487038949 },
        { 8.34336671824457987, 4.12479856412430479, -0.403523417114321381, -1.0107743461787924, 1.8256623712304119, 0.008415761376584154, 0.011286326131968767 },
        { 12.8943695621391310, -15.1111514016986312, -0.223307578892655734, 1.0827910064415354, 0.8687130181696082, -0.010832637401363636, 0.0017237240570597112 },
        { 15.3796971148509165, -25.9193146099879641, 0.179258772950371181, 0.979090732243898, 0.5946989986476762, -0.034755955504078104, 0.0020336868699246304 } };
    memcpy(bodies, init, sizeof bodies);
    for (int step = 0; step < 20000; step++) {
        for (int i = 0; i < 5; i++)
            for (int j = i + 1; j < 5; j++) {
                double dx = bodies[i].x - bodies[j].x, dy = bodies[i].y - bodies[j].y, dz = bodies[i].z - bodies[j].z;
                double d2 = dx * dx + dy * dy + dz * dz;
                double mag = 0.01 / (d2 * sqrt(d2));
                bodies[i].vx -= dx * bodies[j].m * mag; bodies[i].vy -= dy * bodies[j].m * mag; bodies[i].vz -= dz * bodies[j].m * mag;
                bodies[j].vx += dx * bodies[i].m * mag; bodies[j].vy += dy * bodies[i].m * mag; bodies[j].vz += dz * bodies[i].m * mag;
            }
        for (int i = 0; i < 5; i++) { bodies[i].x += 0.01 * bodies[i].vx; bodies[i].y += 0.01 * bodies[i].vy; bodies[i].z += 0.01 * bodies[i].vz; }
    }
    uint64_t x = 0;
    for (int i = 0; i < 5; i++) x ^= dbits(bodies[i].x) + 7 * dbits(bodies[i].y) + 13 * dbits(bodies[i].z);
    return x;
}

/* ---------------------------------------------------------------- 9. 32-bit division / modulo */
static uint64_t k_divmod(void)
{
    uint32_t acc = 0;
    for (uint32_t i = 1; i <= 1500000; i++) {
        acc += (i * 2654435761u) % 1000003u;
        acc ^= i / 7u;
        acc += (uint32_t)((int32_t)(i * 17u) / (int32_t)(i % 13u + 1u));
    }
    return acc;
}

/* ---------------------------------------------------------------- 10. 64-bit arithmetic */
static uint64_t k_ll64(void)
{
    uint64_t x = 88172645463325252ULL, acc = 0;
    for (uint32_t i = 1; i <= 150000; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        acc += x % (uint64_t)(i | 1u);
        acc += (uint64_t)((int64_t)x / (int64_t)((i & 255u) + 1u));
        acc ^= x * 0x9E3779B97F4A7C15ULL;
    }
    return acc;
}

/* ---------------------------------------------------------------- 11. bit operations */
static uint64_t k_bitops(void)
{
    lcg_state = 5;
    uint64_t acc = 0;
    for (uint32_t i = 0; i < 3000000; i++) {
        uint32_t v = rnd32();
        acc += (uint32_t)__builtin_popcount(v) + (uint32_t)__builtin_clz(v | 1u) + (uint32_t)__builtin_ctz(v | 0x80000000u);
        acc ^= (uint64_t)ror32(v, i) + (__builtin_bswap32(v) & 0xFFu);
    }
    return acc;
}

/* ---------------------------------------------------------------- 12. byte/string processing */
static char g_text[1 << 20];
static uint64_t k_strings(void)
{
    lcg_state = 6;
    for (size_t i = 0; i < sizeof g_text; i++) { uint32_t r = rnd32() >> 24; g_text[i] = (r % 7 == 0) ? ' ' : (char)('a' + r % 26); }
    uint64_t words = 0, vowels = 0, pairs = 0, hash = 5381;
    int inword = 0;
    for (size_t i = 0; i < sizeof g_text; i++) {
        char c = g_text[i];
        if (c == ' ') { inword = 0; } else { if (!inword) words++; inword = 1; }
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') vowels++;
        if (i + 1 < sizeof g_text && c == 'a' && g_text[i + 1] == 'b') pairs++;
        hash = hash * 33 + (unsigned char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    return words * 1000003ULL + vowels * 31ULL + pairs + hash;
}

struct kernel { const char *name; uint64_t (*fn)(void); uint64_t expect; };
static const struct kernel kernels[] = {
    { "sha256",   k_sha256,   EXPECT_sha256 },
    { "crc32",    k_crc32,    EXPECT_crc32 },
    { "sieve",    k_sieve,    EXPECT_sieve },
    { "qsort",    k_sort,     EXPECT_qsort },
    { "matmul-d", k_matmul_d, EXPECT_matmul_d },
    { "matmul-f", k_matmul_f, EXPECT_matmul_f },
    { "fft",      k_fft,      EXPECT_fft },
    { "mandel",   k_mandel,   EXPECT_mandel },
    { "nbody",    k_nbody,    EXPECT_nbody },
    { "divmod",   k_divmod,   EXPECT_divmod },
    { "int64",    k_ll64,     EXPECT_ll64 },
    { "bitops",   k_bitops,   EXPECT_bitops },
    { "strings",  k_strings,  EXPECT_strings },
};

int main(int argc, char **argv)
{
    double minsec = (argc > 1) ? atof(argv[1]) : 1.0;       /* seconds each kernel runs (default 1) */
    printf("bench 1.0 [%s] gcc %s\n", ROTEST_CFG, __VERSION__);
    uint32_t d[8];
    sha256((const uint8_t *)"abc", 3, d);
    printf("  sha256 self-test: %s\n", (d[0] == 0xba7816bf && d[1] == 0x8f01cfea && d[7] == 0xf20015ad) ? "ok" : "FAILED");
    double total = 0;
    int bad = 0;
    for (unsigned k = 0; k < sizeof kernels / sizeof kernels[0]; k++) {
        clock_t c0 = clock(), c1;
        uint64_t sum = 0;
        long reps = 0;
        do { sum = kernels[k].fn(); reps++; c1 = clock(); } while ((double)(c1 - c0) / CLOCKS_PER_SEC < minsec);
        double ms = (double)(c1 - c0) * 1000.0 / CLOCKS_PER_SEC / (double)reps;
        int ok = (sum == kernels[k].expect);
        if (!ok) bad++;
        total += ms;
        printf("  %-9s sum=%016llx %9.2f ms/run (%ld runs) %s\n", kernels[k].name, (unsigned long long)sum, ms, reps, ok ? "ok" : "MISMATCH");
    }
    printf("TOTAL [%s] %.1f ms  (sum of the ms/run figures; lower is faster; %d checksum mismatches)\n", ROTEST_CFG, total, bad);
    return bad ? 1 : 0;
}
