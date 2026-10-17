/* runswap.c - __ul64_run_swap: run a RISC OS command line and come back, although the program started by OS_CLI is loaded over this one (design v2, riscos64-research/E-process-model.md).
   The image (&8000 up to the start of the heap) and the stack (the stack pointer up to the RAM limit) are copied to a dynamic area, an exit handler that lives in that area is set, the command runs, and when
   the child ends the handler copies the image and the stack back and jumps to a function of this file, which longjmps to the place where the swap started.  The heap is a dynamic area and is not touched.
   STATE: written from the design, NOT yet run (needs the answers of exp_exit: whether the exit handler is called for a child, and its registers).  It is used only when the system variable
   UnixEnv$swap is set to 1; without it system () and posix_spawn () run the command with OS_CLI as before.  The SDK of RISC OS 64. */
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern char __heap_start[];
extern unsigned long __riscos_ramlimit;
#define OS_CLI 0x05
#define OS_ChangeEnvironment 0x40
#define OS_DynamicArea 0x66
#define OS_SynchroniseCodeAreas 0x6E

#define IMAGE_BASE 0x8000UL
#define HANDLER_SIZE 512				/* room for the handler code and its data block */
#define DA_STACK 4096					/* the handler's own stack, at the end of the area's first page */

/* The handler: position independent, entered by the kernel when the child ends.  It has no stack of its own (the kernel's is not known), so it takes the top of its data page.  The data block
   (exp_swap_data, after the code) holds: 0 source of the image, 1 its length, 2 source of the stack, 3 where the stack goes, 4 its length, 5 the resume address, 6 the stack for the handler. */
__asm__ (
  ".text\n.balign 16\n.global ul64_sw_start\n.global ul64_sw_end\n"
  "ul64_sw_start:\n"
  "	adr	x16, ul64_sw_data\n"
  "	ldr	x17, [x16, #48]\n	mov	sp, x17\n"
  "	ldr	x0, [x16, #0]\n	mov	x1, #0x8000\n	ldr	x2, [x16, #8]\n"		/* the image back to &8000 */
  "1:	ldp	x4, x5, [x0], #16\n	stp	x4, x5, [x1], #16\n	subs	x2, x2, #16\n	b.hi	1b\n"
  "	ldr	x0, [x16, #16]\n	ldr	x1, [x16, #24]\n	ldr	x2, [x16, #32]\n"	/* the stack back */
  "2:	ldp	x4, x5, [x0], #16\n	stp	x4, x5, [x1], #16\n	subs	x2, x2, #16\n	b.hi	2b\n"
  "	ldr	x19, [x16, #40]\n"
  "	mov	x0, #1\n	mov	x1, #0x8000\n	ldr	x2, [x16, #8]\n	add	x2, x2, x1\n"	/* OS_SynchroniseCodeAreas, a range */
  "	mov	x10, #0x6E\n	orr	x10, x10, #0x20000\n	svc	#0\n"
  "	br	x19\n"
  ".balign 16\nul64_sw_data:	.space	64\n"
  "ul64_sw_end:\n");
extern char ul64_sw_start[], ul64_sw_end[];

static jmp_buf resume_point;				/* in the image: saved with it, valid again when it is back */
static unsigned long old_handler[4];

static void resume (void)				/* called by the handler once the memory is back */
{
  longjmp (resume_point, 1);
}

static int swap_enabled (void)
{
  char buf[8];
  unsigned long r[10] = { (unsigned long) "UnixEnv$swap", (unsigned long) buf, sizeof buf - 1, 0, 3 };
  return !swi_x (0x23, r) && r[2] == 1 && buf[0] == '1';
}

/* 0: the command ran and the program is back; -1: not done (errno set; swap not enabled gives ENOSYS), the caller falls back to OS_CLI */
int __ul64_run_swap (const char *cmd)
{
  unsigned long r[10] = { 0, (unsigned long) -1, 4096, (unsigned long) -1, 0, 0, 0, 0, (unsigned long) "UL64 parent" };
  unsigned long image = (unsigned long) __heap_start - IMAGE_BASE;
  unsigned long sp_now, stack_len, area, base, total, *d;
  char *p;
  if (!swap_enabled ()) { errno = ENOSYS; return -1; }
  image = (image + 15) & ~15UL;
  sp_now = (unsigned long) __builtin_frame_address (0) - 256;			/* a little below this frame; the stack part is copied from here */
  sp_now &= ~15UL;
  stack_len = (__riscos_ramlimit - sp_now + 15) & ~15UL;
  total = ((HANDLER_SIZE + DA_STACK + image + stack_len) + 4095) & ~4095UL;
  r[5] = total;
  if (swi_x (OS_DynamicArea, r)) { errno = ENOMEM; return -1; }
  area = r[1] & 0xFFFFFFFF;
  base = r[3];
  /* the page after the handler is its stack, then the saved image, then the saved stack */
  memcpy ((void *) base, ul64_sw_start, (size_t) (ul64_sw_end - ul64_sw_start));
  d = (unsigned long *) (base + (size_t) (ul64_sw_end - ul64_sw_start) - 64);	/* ul64_sw_data */
  p = (char *) base + HANDLER_SIZE + DA_STACK;
  d[0] = (unsigned long) p;							/* the saved image */
  d[1] = image;
  d[2] = (unsigned long) p + image;						/* the saved stack */
  d[3] = sp_now;
  d[4] = stack_len;
  d[5] = (unsigned long) resume;
  d[6] = base + HANDLER_SIZE + DA_STACK;
  if (setjmp (resume_point) == 0)
    {
      unsigned long e[10];
      unsigned long c[10] = { (unsigned long) cmd };
      e[0] = 11; e[1] = 0; e[2] = 0; e[3] = 0;					/* read the handler that is set now: it must be in the image before the image is copied */
      if (swi_x (OS_ChangeEnvironment, e)) { unsigned long k[10] = { 1, area }; swi_x (OS_DynamicArea, k); errno = ENOSYS; return -1; }
      old_handler[0] = e[1]; old_handler[1] = e[2]; old_handler[2] = e[3];
      memcpy (p, (void *) IMAGE_BASE, image);
      memcpy (p + image, (void *) sp_now, stack_len);
      { unsigned long s[10] = { 1, base, base + HANDLER_SIZE }; swi_x (OS_SynchroniseCodeAreas, s); }
      e[0] = 11; e[1] = base; e[2] = 0; e[3] = base + 0x1000 - 64;
      if (swi_x (OS_ChangeEnvironment, e)) { unsigned long k[10] = { 1, area }; swi_x (OS_DynamicArea, k); errno = ENOSYS; return -1; }
      swi_x (OS_CLI, c);							/* if the child ends the normal way the handler takes over and this never returns */
      {											/* the parent came back by itself: the kernel returned from OS_CLI (no copy needed) */
	unsigned long b[10] = { 11, old_handler[0], old_handler[1], old_handler[2] };
	unsigned long k[10] = { 1, area };
	swi_x (OS_ChangeEnvironment, b);
	swi_x (OS_DynamicArea, k);
	return 0;
      }
    }
  {											/* back through the handler: the image and the stack are those of before the run */
    unsigned long b[10] = { 11, old_handler[0], old_handler[1], old_handler[2] };
    unsigned long k[10] = { 1, area };
    swi_x (OS_ChangeEnvironment, b);
    swi_x (OS_DynamicArea, k);
  }
  return 0;
}
