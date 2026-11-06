#include <stdio.h>
#include <asm/types.h>
#include <asm-generic/signal-defs.h>
#include <linux/auxvec.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/sched.h>

int main(void)
{
    struct fb_var_screeninfo v;
    if (sizeof(v.xres) != 4)
        return 1;
    puts("uapi ok");
    return 0;
}
