#!/bin/bash
# Extract cond_deadline () from the patched cond.c, make it 32-bit (long -> int), build and run the host model.
set -e
cd "$(dirname "$0")"
awk '/^static int$/ { buf=$0; getline; if ($0 ~ /^cond_deadline/) { print buf; print; inside=1; next } else if (!inside) next }
     inside { print } inside && /^}$/ { exit }' cond.c \
  | sed -e 's/\blong long\b/@LL@/g' -e 's/\bunsigned long\b/unsigned int/g' -e 's/\blong\b/int/g' -e 's/@LL@/long long/g' -e 's/\([0-9]\)L\b/\1/g' > cond_deadline.inc
grep -c . cond_deadline.inc | sed 's/^/extracted lines: /'
gcc -O2 -g -Wall -Wextra -fsanitize=undefined,address -fno-sanitize-recover=undefined cond_model.c -o cond_model -lm
./cond_model
