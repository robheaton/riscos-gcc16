--- gcc/config/arm/arm.cc.orig	2020-07-23 07:35:17.344384552 +0100
+++ gcc/config/arm/arm.cc	2020-07-24 17:16:31.201911500 +0100
@@ -3544,6 +3544,12 @@
        it.  */
     target_flags |= MASK_SINGLE_PIC_BASE;
 
+#ifdef TARGET_RISCOSELF
+  /* The data segment is stored seperately from the text segment at an
+   * address determined at runtime.  */
+  arm_pic_data_is_text_relative = 0;
+#endif
+
   /* If stack checking is disabled, we can use r10 as the PIC register,
      which keeps r9 available.  The EABI specifies r9 as the PIC register.  */
   if (flag_pic && TARGET_SINGLE_PIC_BASE)
@@ -8097,6 +8103,17 @@
 
   if (pic_reg == NULL_RTX)
     pic_reg = cfun->machine->pic_reg;
+#ifdef TARGET_RISCOSELF
+  if (flag_pic == 2)
+    {
+      emit_insn (gen_rtx_SET (pic_reg, GEN_INT(0x8000)));
+      emit_insn (gen_pic_offset_arm (pic_reg, pic_reg, GEN_INT(0x38)));
+
+      pic_tmp = gen_rtx_SYMBOL_REF (Pmode, RISCOS_GOTT_INDEX);
+      emit_insn (gen_pic_offset_arm (pic_reg, pic_reg, pic_tmp));
+    }
+  else
+#endif
   if (TARGET_VXWORKS_RTP)
     {
       pic_rtx = gen_rtx_SYMBOL_REF (Pmode, VXWORKS_GOTT_BASE);
@@ -29785,6 +29802,13 @@
   if (cfun->has_nonlocal_label)
     return true;
 
+#ifdef TARGET_RISCOSELF
+  /* For the RISC OS version of ARMEABI, the frame pointer is required
+     for non-leaf functions.  */
+  if (!crtl->is_leaf)
+    return true;
+#endif
+
   /* The frame pointer is required for non-leaf APCS frames.  */
   if (TARGET_ARM && TARGET_APCS_FRAME && !crtl->is_leaf)
     return true;
