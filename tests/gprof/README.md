# gprof test (`-pg`)

`pgtest.c` calls two functions, `heavy ()` (two million loop turns) and `light ()` (one million), in turn for a fixed time (the argument, in centiseconds; 600 = 6 seconds), and prints how often each was called. Built with `-pg` it must write `gmon.out` when it ends, and gprof must show

* the call counts the program printed (exactly), and
* `heavy ()` and `light ()` as almost all of the time (the split depends on the machine: on the Compute Module 4 it was 56 to 61 percent for `heavy ()`).

```
./build.sh ~/riscos-gcc16-cross-16.2.0-13-x86_64-linux       # pgtest,e1f (with -pg) and pgtest-plain,e1f (without: the control, no gmon.out)
```

On RISC OS (the runtime 16.2.0-13 or later installed), in a Task window:

```
pgtest 600                  heavy () was called 1257 times, light () 1257 times, in 600 centiseconds ... and gmon/out is written
gprof -b pgtest gmon.out    (the native gprof of the Gcc16 package; or copy gmon.out to Linux and run arm-riscos-gnueabihf-gprof pgtest gmon.out)
```

What was measured on the test machine (Raspberry Pi Compute Module 4, RISC OS 5.30, three runs of 6 seconds): `gmon.out` of 1279 bytes, the call counts equal to the program's own (1212, 1256 and 1257), 237 to 240 samples at 50 a second (4.7 to 4.8 seconds of the 6), `heavy ()` 56 to 61 percent, the native and the Linux gprof printing the same, and about 2 percent more run time than the program built without `-pg`. Every hot loop falls into **one** histogram bin: the processor takes the interrupt at a fixed place of the loop, so only the function a sample falls in means anything, not the instruction.

`GMON_VERBOSE` (any value) makes the program print the number of samples when it ends.
