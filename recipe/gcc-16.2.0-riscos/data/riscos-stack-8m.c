/* The main stack of GNU make on RISC OS: 8 MB (libunixlib 16.2.0-6 and later; an older libunixlib ignores it and the stack stays 1 MB).  make recurses for every level of a chain of
   prerequisites and of nested $(call) functions; 1 MB is enough for ordinary makefiles, 8 MB (what Linux gives a program) for the odd one.  Costs address space only (ARMEABISupport
   maps stack pages when they are first touched).  Linked into make by recipe/make-4.4.1-riscos/scripts/build-make.sh.  */
int __stack_size = 8 * 1024 * 1024;
