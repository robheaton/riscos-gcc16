/* socket.c - the BSD socket calls fail with ENOSYS until the 64-bit Internet module has an interface the SDK can use (UnixLib 64 will provide them).  The SDK of RISC OS 64. */
#include <errno.h>
#include <sys/socket.h>

#define FAIL do { errno = ENOSYS; return -1; } while (0)
int socket (int d, int t, int p) { (void) d; (void) t; (void) p; FAIL; }
int socketpair (int d, int t, int p, int sv[2]) { (void) d; (void) t; (void) p; (void) sv; FAIL; }
int bind (int s, const struct sockaddr *a, socklen_t l) { (void) s; (void) a; (void) l; FAIL; }
int listen (int s, int b) { (void) s; (void) b; FAIL; }
int accept (int s, struct sockaddr *a, socklen_t *l) { (void) s; (void) a; (void) l; FAIL; }
int connect (int s, const struct sockaddr *a, socklen_t l) { (void) s; (void) a; (void) l; FAIL; }
ssize_t send (int s, const void *b, size_t n, int f) { (void) s; (void) b; (void) n; (void) f; FAIL; }
ssize_t recv (int s, void *b, size_t n, int f) { (void) s; (void) b; (void) n; (void) f; FAIL; }
ssize_t sendto (int s, const void *b, size_t n, int f, const struct sockaddr *a, socklen_t l) { (void) s; (void) b; (void) n; (void) f; (void) a; (void) l; FAIL; }
ssize_t recvfrom (int s, void *b, size_t n, int f, struct sockaddr *a, socklen_t *l) { (void) s; (void) b; (void) n; (void) f; (void) a; (void) l; FAIL; }
int shutdown (int s, int h) { (void) s; (void) h; FAIL; }
int setsockopt (int s, int lv, int n, const void *v, socklen_t l) { (void) s; (void) lv; (void) n; (void) v; (void) l; FAIL; }
int getsockopt (int s, int lv, int n, void *v, socklen_t *l) { (void) s; (void) lv; (void) n; (void) v; (void) l; FAIL; }
int getsockname (int s, struct sockaddr *a, socklen_t *l) { (void) s; (void) a; (void) l; FAIL; }
int getpeername (int s, struct sockaddr *a, socklen_t *l) { (void) s; (void) a; (void) l; FAIL; }
