--- gcc/ira.cc.orig	2020-07-23 07:35:17.676388210 +0100
+++ gcc/ira.cc	2020-07-24 17:16:30.845914454 +0100
@@ -2261,7 +2261,9 @@
      sp-adjusting insns for this case.  */
   frame_pointer_needed
     = (! flag_omit_frame_pointer
+#ifndef TARGET_RISCOSELF
        || (cfun->calls_alloca && EXIT_IGNORE_STACK)
+#endif
        /* We need the frame pointer to catch stack overflow exceptions if
 	  the stack pointer is moving (as for the alloca case just above).  */
        || (STACK_CHECK_MOVING_SP
