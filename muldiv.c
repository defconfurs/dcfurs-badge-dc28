#include "badge.h"

/* Multiply two single-register values together. */
unsigned int
__mulsi3(unsigned int a, unsigned int b)
{
    MISC->mul_ctrl = 0;
    MISC->mul_op_a = (a >> 16);
    MISC->mul_op_b = b;
    MISC->mul_accum = MISC->mul_result;
    MISC->mul_op_a = a;
    MISC->mul_op_b = (b >> 16);
    MISC->mul_accum = MISC->mul_result << 16;
    MISC->mul_op_b = b;
    return MISC->mul_result;
}
