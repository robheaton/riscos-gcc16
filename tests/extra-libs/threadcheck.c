/* threadcheck - OpenSSL 1.1.1 used from several threads at once: digests, random numbers, EC key generation and TLS handshakes between a client and a server that talk through memory BIOs, with the two SSL_CTX objects shared by all the threads (session cache, reference counts and the error queue are what the locks protect) */
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/ec.h>

#define NTHREADS 4
#define ROUNDS 6

static SSL_CTX *sctx, *cctx;
static unsigned char expect[NTHREADS][32];

struct job { int id, fails, handshakes; };

static void digest (int id, unsigned char *out)
{
  unsigned char buf[4096];
  unsigned mdl = 0;
  int i;
  EVP_MD_CTX *mc = EVP_MD_CTX_new ();
  memset (buf, 0x30 + id, sizeof buf);
  EVP_DigestInit_ex (mc, EVP_sha256 (), 0);
  for (i = 0; i < 64; i++) EVP_DigestUpdate (mc, buf, sizeof buf);
  EVP_DigestFinal_ex (mc, out, &mdl);
  EVP_MD_CTX_free (mc);
}

static int handshake (void)
{
  SSL *s = SSL_new (sctx), *c = SSL_new (cctx);
  BIO *b1 = 0, *b2 = 0;
  int i, done = 0, ok = 0;
  char msg[32];
  if (!s || !c) return 0;
  BIO_new_bio_pair (&b1, 0, &b2, 0);
  SSL_set_bio (s, b1, b1); SSL_set_bio (c, b2, b2);
  SSL_set_accept_state (s); SSL_set_connect_state (c);
  for (i = 0; i < 50 && !done; i++)
    {
      int rc = SSL_do_handshake (c), rs = SSL_do_handshake (s);
      if (rc == 1 && rs == 1) done = 1;
    }
  if (done && SSL_write (c, "hello", 5) == 5 && SSL_read (s, msg, sizeof msg) == 5 && !memcmp (msg, "hello", 5)) ok = 1;
  SSL_free (s); SSL_free (c);
  return ok;
}

static void *worker (void *arg)
{
  struct job *j = arg;
  unsigned char d[32], r[64];
  int i, k, nz;
  for (i = 0; i < ROUNDS; i++)
    {
      digest (j->id, d);
      if (memcmp (d, expect[j->id], 32)) j->fails++;
      RAND_bytes (r, sizeof r);
      for (nz = k = 0; k < (int) sizeof r; k++) nz |= r[k];
      if (!nz) j->fails++;
      if (handshake ()) j->handshakes++; else j->fails++;
      if (i == 0)
	{
	  EVP_PKEY *pk = 0;
	  EVP_PKEY_CTX *kc = EVP_PKEY_CTX_new_id (EVP_PKEY_EC, 0);
	  EVP_PKEY_keygen_init (kc);
	  EVP_PKEY_CTX_set_ec_paramgen_curve_nid (kc, NID_X9_62_prime256v1);
	  if (EVP_PKEY_keygen (kc, &pk) != 1 || !pk) j->fails++;
	  EVP_PKEY_free (pk); EVP_PKEY_CTX_free (kc);
	}
    }
  return 0;
}

int main (void)
{
  pthread_t th[NTHREADS];
  struct job jobs[NTHREADS];
  EVP_PKEY *pk = 0;
  EVP_PKEY_CTX *kc = EVP_PKEY_CTX_new_id (EVP_PKEY_EC, 0);
  X509 *x = X509_new ();
  X509_NAME *nm;
  int i, bad = 0;
  printf ("%s, %d threads, %d rounds each\n", OpenSSL_version (OPENSSL_VERSION), NTHREADS, ROUNDS);
  for (i = 0; i < NTHREADS; i++) digest (i, expect[i]);
  EVP_PKEY_keygen_init (kc);
  EVP_PKEY_CTX_set_ec_paramgen_curve_nid (kc, NID_X9_62_prime256v1);
  EVP_PKEY_keygen (kc, &pk);
  X509_set_version (x, 2);
  ASN1_INTEGER_set (X509_get_serialNumber (x), 1);
  X509_gmtime_adj (X509_getm_notBefore (x), 0); X509_gmtime_adj (X509_getm_notAfter (x), 3600);
  X509_set_pubkey (x, pk);
  nm = X509_get_subject_name (x);
  X509_NAME_add_entry_by_txt (nm, "CN", MBSTRING_ASC, (const unsigned char *) "riscos-test", -1, -1, 0);
  X509_set_issuer_name (x, nm);
  X509_sign (x, pk, EVP_sha256 ());
  sctx = SSL_CTX_new (TLS_server_method ()); cctx = SSL_CTX_new (TLS_client_method ());
  SSL_CTX_use_certificate (sctx, x); SSL_CTX_use_PrivateKey (sctx, pk);
  SSL_CTX_set_verify (cctx, SSL_VERIFY_NONE, 0);
  if (!sctx || !cctx || !pk) { printf ("setup failed\n"); ERR_print_errors_fp (stdout); return 1; }
  for (i = 0; i < NTHREADS; i++)
    {
      jobs[i].id = i; jobs[i].fails = jobs[i].handshakes = 0;
      if (pthread_create (&th[i], 0, worker, &jobs[i])) { printf ("pthread_create failed\n"); return 1; }
    }
  for (i = 0; i < NTHREADS; i++)
    {
      pthread_join (th[i], 0);
      printf ("  thread %d: %d handshakes, %d failures  %s\n", i, jobs[i].handshakes, jobs[i].fails, jobs[i].fails ? "FAILED" : "ok");
      bad += jobs[i].fails;
    }
  if (bad) ERR_print_errors_fp (stdout);
  printf (bad ? "threadcheck: %d FAILED\n" : "threadcheck: all passed\n", bad);
  return bad != 0;
}
