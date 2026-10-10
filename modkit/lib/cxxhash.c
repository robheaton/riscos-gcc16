/* cxxhash.c - what the hash containers of libstdc++ (<unordered_map>, <unordered_set>) need of its library: the rehash policy (the number of buckets, a prime, for the number of elements and the maximum load
   factor) and std::_Hash_bytes (the hash of a string: the Murmur hash of the 32 bit size_t).  libstdc++'s own members use the VFP, so they are not in libstdcxx-mod.a; these are written for the kit, in C with the
   mangled names of the C++ ABI, and give the same kind of answers (the bucket counts are not the same primes, so a container may iterate in another order than the host's).  */
#pragma GCC optimize ("Os")
#include <stddef.h>
#include <math.h>

typedef struct { float max_load_factor; unsigned next_resize; } policy;       /* std::__detail::_Prime_rehash_policy: next_resize is mutable */
typedef struct { unsigned char need; unsigned count; } rehash_result;           /* std::pair<bool, size_t> */

static const unsigned primes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741, 3221225473u, 4294967291u };
#define NPRIMES (sizeof primes / sizeof primes[0])

/* the first prime that is at least N (the largest one if there is none), and the number of elements that fit in it before the next rehash */
unsigned _ZNKSt8__detail20_Prime_rehash_policy11_M_next_bktEj (const policy *p, unsigned n)
{
  unsigned i = 0, lo = 0, hi = (unsigned) NPRIMES - 1;
  if (n == 0) return 1;                                                      /* a container that is made with 0 buckets: 1, and nothing to resize before the first insertion */
  while (lo < hi) { i = (lo + hi) / 2; if (primes[i] < n) lo = i + 1; else hi = i; }
  ((policy *) p)->next_resize = primes[lo] == primes[NPRIMES - 1] && n > primes[lo] ? 0xFFFFFFFFu : (unsigned) floor (primes[lo] * (double) p->max_load_factor);
  return primes[lo];
}

/* does inserting N_INS elements into a table of N_BKT buckets that holds N_ELT need more buckets?  The answer is returned in memory (a pair of 8 bytes: the first argument is the address). */
void _ZNKSt8__detail20_Prime_rehash_policy14_M_need_rehashEjjj (rehash_result *r, const policy *p, unsigned n_bkt, unsigned n_elt, unsigned n_ins)
{
  r->need = 0; r->count = 0;
  if ((unsigned long long) n_elt + n_ins > p->next_resize)
    {
      unsigned want = n_elt + n_ins;
      double min_bkts;
      if (!p->next_resize && want < 11) want = 11;                           /* nothing allocated so far: start with 11 buckets at least */
      min_bkts = want / (double) p->max_load_factor;
      if (min_bkts >= n_bkt)
        {
          unsigned grow = (unsigned) floor (min_bkts) + 1, twice = n_bkt * 2;
          r->need = 1;
          r->count = _ZNKSt8__detail20_Prime_rehash_policy11_M_next_bktEj (p, grow > twice ? grow : twice);
        }
      else ((policy *) p)->next_resize = (unsigned) floor (n_bkt * (double) p->max_load_factor);
    }
}

/* std::_Hash_bytes (const void *ptr, size_t len, size_t seed): the Murmur hash (version 2) of the bytes */
unsigned _ZSt11_Hash_bytesPKvjj (const void *ptr, unsigned len, unsigned seed)
{
  const unsigned m = 0x5bd1e995u;
  const unsigned char *b = ptr;
  unsigned h = seed ^ len;
  while (len >= 4)
    {
      unsigned k = b[0] | (unsigned) b[1] << 8 | (unsigned) b[2] << 16 | (unsigned) b[3] << 24;
      k *= m; k ^= k >> 24; k *= m;
      h *= m; h ^= k;
      b += 4; len -= 4;
    }
  switch (len)
    {
    case 3: h ^= (unsigned) b[2] << 16; /* fall through */
    case 2: h ^= (unsigned) b[1] << 8; /* fall through */
    case 1: h ^= b[0]; h *= m;
    }
  h ^= h >> 13; h *= m; h ^= h >> 15;
  return h;
}
