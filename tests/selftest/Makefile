# A small project built with the native GNU make and the native GCC 16.
# Names are the Unix ones; UnixLib's suffix swapping finds c.main, h.util, o.main on RISC OS.
CFLAGS = -O2
OBJS = main.o util.o

app: $(OBJS)
	gcc -o app $(OBJS)

main.o util.o: util.h
