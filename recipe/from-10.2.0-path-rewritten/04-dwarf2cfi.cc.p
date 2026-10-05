--- gcc/dwarf2cfi.cc.orig	2020-07-23 07:35:17.580387153 +0100
+++ gcc/dwarf2cfi.cc	2020-07-24 17:16:30.821914653 +0100
@@ -1743,7 +1743,9 @@
 	      /* Rule 3 */
 	      /* Either setting the FP from an offset of the SP,
 		 or adjusting the FP */
+#ifndef TARGET_RISCOSELF
 	      gcc_assert (frame_pointer_needed);
+#endif
 
 	      gcc_assert (REG_P (XEXP (src, 0))
 			  && dwf_regno (XEXP (src, 0)) == cur_cfa->reg);
@@ -1753,6 +1755,14 @@
 	      cur_cfa->offset += offset;
 	      cur_cfa->reg = dw_frame_pointer_regnum;
 	    }
+
+	      /* Rule XX */
+	      else if (GET_CODE (src) == PLUS
+		       && GET_CODE (XEXP (src, 1)) == CONST_INT)
+		{
+		  cur_trace->cfa_temp.reg = REGNO (dest);
+		  cur_trace->cfa_temp.offset = INTVAL (XEXP (src, 1));
+		}
 	  else
 	    {
 	      gcc_assert (GET_CODE (src) != MINUS);
