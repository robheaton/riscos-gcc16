/* sslcheck - OpenSSL 1.1.1 on RISC OS: digests, AES, random numbers, EC key + certificate, and a TLS handshake between a client and a server that talk through memory BIOs */
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/ec.h>
#include <openssl/pem.h>

static int fails;
#define CHECK(c, what) do { int ok_ = (c); printf ("  %-48s %s\n", what, ok_ ? "ok" : "FAILED"); if (!ok_) { fails++; ERR_print_errors_fp (stdout); } } while (0)

static void hex (const unsigned char *d, unsigned n, char *out) { unsigned i; for (i = 0; i < n; i++) sprintf (out + 2 * i, "%02x", d[i]); }

int main (void)
{
  unsigned char md[64], key[16], iv[16], ct[64], pt[64];
  unsigned mdl = 0;
  char h[160];
  int l1 = 0, l2 = 0, pl1 = 0, pl2 = 0;
  EVP_MD_CTX *mc;
  EVP_CIPHER_CTX *cc;
  printf ("%s\n", OpenSSL_version (OPENSSL_VERSION));
  mc = EVP_MD_CTX_new ();
  EVP_DigestInit_ex (mc, EVP_sha256 (), 0); EVP_DigestUpdate (mc, "abc", 3); EVP_DigestFinal_ex (mc, md, &mdl);
  hex (md, mdl, h);
  CHECK (!strcmp (h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 of abc");
  EVP_DigestInit_ex (mc, EVP_sha1 (), 0); EVP_DigestUpdate (mc, "abc", 3); EVP_DigestFinal_ex (mc, md, &mdl);
  hex (md, mdl, h);
  CHECK (!strcmp (h, "a9993e364706816aba3e25717850c26c9cd0d89d"), "SHA-1 of abc");
  EVP_MD_CTX_free (mc);
  CHECK (RAND_bytes (key, 16) == 1 && RAND_bytes (iv, 16) == 1 && memcmp (key, iv, 16) != 0, "RAND_bytes (reads /dev/urandom)");
  cc = EVP_CIPHER_CTX_new ();
  EVP_EncryptInit_ex (cc, EVP_aes_128_cbc (), 0, key, iv);
  EVP_EncryptUpdate (cc, ct, &l1, (const unsigned char *) "the quick brown fox jumps over", 30); EVP_EncryptFinal_ex (cc, ct + l1, &l2);
  EVP_CIPHER_CTX_free (cc);
  cc = EVP_CIPHER_CTX_new ();
  EVP_DecryptInit_ex (cc, EVP_aes_128_cbc (), 0, key, iv);
  EVP_DecryptUpdate (cc, pt, &pl1, ct, l1 + l2); EVP_DecryptFinal_ex (cc, pt + pl1, &pl2);
  EVP_CIPHER_CTX_free (cc);
  CHECK (l1 + l2 == 32 && pl1 + pl2 == 30 && !memcmp (pt, "the quick brown fox jumps over", 30), "AES-128-CBC encrypt and decrypt");
  {
    EVP_PKEY *pk = 0;
    EVP_PKEY_CTX *kc = EVP_PKEY_CTX_new_id (EVP_PKEY_EC, 0);
    X509 *x = 0;
    X509_NAME *nm;
    SSL_CTX *sc = 0, *cx = 0;
    SSL *s = 0, *c = 0;
    BIO *b1 = 0, *b2 = 0;
    int i, done = 0;
    char msg[64];
    EVP_PKEY_keygen_init (kc);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid (kc, NID_X9_62_prime256v1);
    EVP_PKEY_keygen (kc, &pk);
    CHECK (pk != 0, "EC P-256 key generation");
    x = X509_new ();
    X509_set_version (x, 2);
    ASN1_INTEGER_set (X509_get_serialNumber (x), 1);
    X509_gmtime_adj (X509_getm_notBefore (x), 0); X509_gmtime_adj (X509_getm_notAfter (x), 3600);
    X509_set_pubkey (x, pk);
    nm = X509_get_subject_name (x);
    X509_NAME_add_entry_by_txt (nm, "CN", MBSTRING_ASC, (const unsigned char *) "riscos-test", -1, -1, 0);
    X509_set_issuer_name (x, nm);
    CHECK (X509_sign (x, pk, EVP_sha256 ()) > 0, "self-signed certificate");
    sc = SSL_CTX_new (TLS_server_method ()); cx = SSL_CTX_new (TLS_client_method ());
    SSL_CTX_use_certificate (sc, x); SSL_CTX_use_PrivateKey (sc, pk);
    SSL_CTX_set_verify (cx, SSL_VERIFY_NONE, 0);
    s = SSL_new (sc); c = SSL_new (cx);
    BIO_new_bio_pair (&b1, 0, &b2, 0);
    SSL_set_bio (s, b1, b1); SSL_set_bio (c, b2, b2);
    SSL_set_accept_state (s); SSL_set_connect_state (c);
    for (i = 0; i < 50 && !done; i++)
      {
	int rc = SSL_do_handshake (c), rs = SSL_do_handshake (s);
	if (rc == 1 && rs == 1) done = 1;
      }
    CHECK (done, "TLS handshake (client <-> server through BIOs)");
    if (done)
      {
	int n;
	char buf[64];
	printf ("  negotiated %s, cipher %s\n", SSL_get_version (c), SSL_get_cipher_name (c));
	SSL_write (c, "hello over tls", 14);
	n = SSL_read (s, buf, sizeof buf);
	snprintf (msg, sizeof msg, "%.*s", n > 0 ? n : 0, buf);
	CHECK (n == 14 && !strcmp (msg, "hello over tls"), "application data client -> server");
	SSL_write (s, "and back", 8);
	n = SSL_read (c, buf, sizeof buf);
	snprintf (msg, sizeof msg, "%.*s", n > 0 ? n : 0, buf);
	CHECK (n == 8 && !strcmp (msg, "and back"), "application data server -> client");
      }
    SSL_free (s); SSL_free (c); SSL_CTX_free (sc); SSL_CTX_free (cx); X509_free (x); EVP_PKEY_free (pk); EVP_PKEY_CTX_free (kc);
  }
  printf ("sslcheck: %s\n", fails ? "FAILED" : "all passed");
  return fails != 0;
}
