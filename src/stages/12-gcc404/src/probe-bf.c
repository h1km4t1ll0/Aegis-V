struct probe_tc {
    struct probe_tc *chain;
    struct probe_tc *type;
    void *ann;
    unsigned int code : 8;
    unsigned side_effects_flag : 1;
    unsigned constant_flag : 1;
    unsigned addressable_flag : 1;
    unsigned volatile_flag : 1;
    unsigned readonly_flag : 1;
    unsigned unsigned_flag : 1;
    unsigned asm_written_flag : 1;
    unsigned nowarning_flag : 1;
    unsigned used_flag : 1;
    unsigned nothrow_flag : 1;
    unsigned static_flag : 1;
    unsigned public_flag : 1;
    unsigned private_flag : 1;
    unsigned protected_flag : 1;
    unsigned deprecated_flag : 1;
    unsigned invariant_flag : 1;
    unsigned lang_flag_0 : 1;
    unsigned lang_flag_1 : 1;
    unsigned lang_flag_2 : 1;
    unsigned lang_flag_3 : 1;
    unsigned lang_flag_4 : 1;
    unsigned lang_flag_5 : 1;
    unsigned lang_flag_6 : 1;
    unsigned visited : 1;
};

int
probe_bf_code(struct probe_tc *n)
{
    return n->code;
}

int
probe_bf_flag(struct probe_tc *n)
{
    return n->constant_flag;
}

int
probe_bf_switch(struct probe_tc *expr)
{
    switch (expr->type->code) {
    case 1:
        return 10;
    case 2:
        return 20;
    default:
        return 0;
    }
}
