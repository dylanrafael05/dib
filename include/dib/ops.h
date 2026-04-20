#pragma once

#include "dib/lambda.h"

/// This namespace contains helpers which invoke each of the builtin
/// operators on their provided arguments.
namespace dib::ops
{
#define op1(name, tname, op) \
		constexpr auto name = DIB_LMB_1_C((), op); \
		using tname = decltype(name)
#define op2(name, tname, op) \
		constexpr auto name = DIB_LMB_2_C((), _1 op _2); \
		using tname = decltype(name)

	op2(less, Less, <);
	op2(less_equal, LessEqual, <=);

	op2(greater, Greater, >);
	op2(greater_equal, GreaterEqual, >=);

	op2(equal, Equal, ==);
	op2(not_equal, NotEqual, !=);

	op2(compare, Compare, <=>);

	op2(plus, Plus, +);
	op2(minus, Minus, -);
	op2(multiply, Multiply, *);
	op2(divide, Divide, /);
	op2(modulo, Modulo, %);

	op2(assign, Assign, =);
	op2(plus_assign, PlusAssign, +=);
	op2(minus_assign, MinusAssign, -=);
	op2(multiply_assign, MultiplyAssign, *=);
	op2(divide_assign, DivideAssign, /=);
	op2(modulo_assign, ModuloAssign, %=);

	op2(bool_and, BoolAnd, &);
	op2(bool_or, BoolOr, |);
	op2(bool_xor, BoolXor, ^);
	op2(left_shift, LeftShift, <<);
	op2(right_shift, RightShift, >>);

	op2(and_assign, AndAssign, &=);
	op2(or_assign, OrAssign, |=);
	op2(xor_assign, XorAssign, ^=);
	op2(left_shift_assign, LeftShiftAssign, <<=);
	op2(right_shift_assign, RightShiftAssign, >>=);

	op1(identity, Identity, _1);

	op1(posit, Posit, +_1);
	op1(negate, Negate, -_1);

	op1(deref, Deref, *_1);
	op1(addrof, AddrOf, &_1);

#undef op1
#undef op2
}