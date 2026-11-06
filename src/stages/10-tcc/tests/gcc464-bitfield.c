/* The last bit-field of line_map is stored as a full unsigned int.
   The struct must be large enough that this store does not smash
   the following object. */
struct line_map {
    const char *to_file;
    unsigned int to_line;
    unsigned int start_location;
    int included_from;
    unsigned int reason : 8;
    unsigned char sysp;
    unsigned int column_bits : 8;
};

struct wrap {
    struct line_map m;
    unsigned long guard;
};

int main(void)
{
    struct wrap w;

    w.guard = 0x1122334455667788UL;
    w.m.to_file = 0;
    w.m.to_line = 1;
    w.m.start_location = 2;
    w.m.included_from = -1;
    w.m.reason = 1;
    w.m.sysp = 2;
    w.m.column_bits = 8;
    if (w.guard != 0x1122334455667788UL)
        return 1;
    if (w.m.column_bits != 8 || w.m.reason != 1 || w.m.sysp != 2)
        return 2;
    return 0;
}
