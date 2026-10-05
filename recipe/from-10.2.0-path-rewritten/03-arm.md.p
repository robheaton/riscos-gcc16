--- gcc/config/arm/arm.md.orig	2020-07-23 07:35:17.344384552 +0100
+++ gcc/config/arm/arm.md	2020-07-24 21:06:05.710065728 +0100
@@ -32,6 +32,7 @@
    (R1_REGNUM	      1)	; Second CORE register
    (R4_REGNUM	      4)	; Fifth CORE register
    (FDPIC_REGNUM      9)	; FDPIC register
+   (SL_REGNUM	     10)	; Stack limit register (APCS-32 only)
    (IP_REGNUM	     12)	; Scratch register
    (SP_REGNUM	     13)	; Stack pointer
    (LR_REGNUM        14)	; Return address register
@@ -6693,7 +6694,7 @@
 	(mem:SI (plus:SI (match_operand:SI 1 "register_operand" "r")
 			 (unspec:SI [(match_operand:SI 2 "" "X")]
 				    UNSPEC_PIC_OFFSET))))]
-  "TARGET_VXWORKS_RTP && TARGET_ARM && flag_pic"
+  "(TARGET_VXWORKS_RTP && TARGET_ARM && flag_pic) || flag_pic == 2"
   "ldr%?\\t%0, [%1,%2]"
   [(set_attr "type" "load_4")]
 )
