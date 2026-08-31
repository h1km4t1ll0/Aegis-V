/* C_alloca for tcc: libiberty.h maps alloca() here when not GCC. */
void *malloc(unsigned long n);

void *C_alloca(unsigned long n)
{
    return malloc(n);
}
