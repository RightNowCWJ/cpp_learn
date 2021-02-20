/*
test for Member Function
*/

#pragma once

#include <stdio.h>


class Calc {
public:
	virtual int sub(int l, int r) {
		return 0;
	}
	virtual int div(int l, int r) {
		return l / r;
	}
};

class CheckCalc : public Calc {

public:

	int div(int l, int r) {
		if (r == 0) {
			++error_count;
			return 0;
		}
		return l / r;
	}

	int error_count = 0;
};

typedef int (Calc::* PtrToOperator) (int, int);

int doCalc(Calc& ins, PtrToOperator op, int l, int r)
{
	return (ins.*(op))(l, r);
}

void test_virtual_function()
{
	CheckCalc check_calc;
	Calc *pc = &check_calc;

	// 间接调用。vcall --> vptr + offset
	PtrToOperator op = &Calc::div;

	// vptr + offset
	pc->div(50, 0);

	// vcall
	(pc->*(op))(50, 0);

	// vcall
	doCalc(*pc, op, 50, 0);

	printf("[%s] error_count:%d \n", __FUNCTION__, check_calc.error_count);
}

