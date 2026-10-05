/* the host's <pthread.h> plus the one macro of UnixLib's that fread.c / fwrite.c use */
#ifndef MODEL_PTHREAD_H
#define MODEL_PTHREAD_H
#include_next <pthread.h>
#define PTHREAD_UNSAFE
#endif
