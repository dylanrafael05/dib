#ifndef __DIB_OPS_H
#define __DIB_OPS_H

#include "dib/lambda.h"

namespace dib::ops
{
#define op2(name, op) \
		constexpr auto name = DIB_LMB_2(_1 op _2); \
		using name##_t = decltype(name)

	op2(less, <);
	op2(less_equal, <=);

	op2(great, >);
	op2(great_equal, >=);

	op2(equal, ==);
	op2(not_equal, !=);

	op2(compare, <=>);

	op2(plus, +);
	op2(minus, -);
	op2(multiply, *);
	op2(divide, /);
	op2(modulo, %);

	op2(assign, =);
	op2(plus_assign, +=);
	op2(minus_assign, -=);
	op2(multiply_assign, *=);
	op2(divide_assign, /=);
	op2(modulo_assign, %=);

	op2(bool_and, &);
	op2(bool_or, |);
	op2(bool_xor, ^);
	op2(left_shift, <<);
	op2(right_shift, >>);

	op2(and_assign, &=);
	op2(or_assign, |=);
	op2(xor_assign, ^=);
	op2(left_shift_assign, <<=);
	op2(right_shift_assign, >>=);

#undef op2
}

#endif