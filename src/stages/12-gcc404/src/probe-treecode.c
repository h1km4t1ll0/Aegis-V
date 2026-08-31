#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"

tree
probe_treecode(tree type, tree expr)
{
    if (integer_zerop(expr))
        return build_int_cst(type, 0);

    switch (TREE_CODE(TREE_TYPE(expr))) {
    case POINTER_TYPE:
    case REFERENCE_TYPE:
        return build1(NOP_EXPR, type, expr);
    case INTEGER_TYPE:
        return expr;
    default:
        return expr;
    }
}
