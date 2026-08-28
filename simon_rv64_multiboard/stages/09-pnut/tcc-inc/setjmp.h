#ifndef _SETJMP_H
#define _SETJMP_H

typedef int jmp_buf[16];
int setjmp(jmp_buf buf);
void longjmp(jmp_buf buf, int val);

#endif
