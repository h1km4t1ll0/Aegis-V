/* 0xffffffffU stored in a uint64 field must stay zero-extended.
   tcc's constant folder used a uint32 cast that sign-extends. */
typedef unsigned long long u64;

struct opc {
    u64 match;
    u64 mask;
};

static struct opc table = { 0xffffffffc0001073ull, 0xffffffffu };

int main(void)
{
    u64 mask = 0xffffffffu;

    if (table.mask != 0xffffffffull)
        return 1;
    if (table.match != 0xffffffffc0001073ull)
        return 2;
    if (mask != 0xffffffffull)
        return 3;
    return 0;
}
