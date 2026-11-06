/* Returning a struct member must put the second register's bytes
   in the result. tcc used to step a spilled pointer onto the next
   stack slot and return that local instead. */
struct cset {
    void *func;
    int cd;
    int width;
};

struct box {
    struct cset narrow;
};

static struct cset pick(struct box *p, int type)
{
    switch (type) {
    default:
        return p->narrow;
    }
}

int main(void)
{
    struct box b;
    struct cset c;

    b.narrow.func = (void *)0x1111;
    b.narrow.cd = 7;
    b.narrow.width = 8;
    c = pick(&b, 61);
    if (c.func != (void *)0x1111)
        return 1;
    if (c.cd != 7)
        return 2;
    if (c.width != 8)
        return 3;
    return 0;
}
