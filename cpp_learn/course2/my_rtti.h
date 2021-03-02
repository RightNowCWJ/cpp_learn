#ifndef MY_RTTI_H
#define MY_RTTI_H
#pragma once

#include <assert.h>
#include <typeinfo>
#include <ehdata_forceinclude.h>
#include <stdio.h>

typedef TypeDescriptor TypeDescriptor;


class A {
public:
	virtual ~A()
	{}
};

class B: public A {
};

class C: public B {};

struct EE {
public:
	virtual void func() {};
};

struct AA: virtual EE {
};

struct BB: virtual EE {
};

struct CC: public AA, public BB {
};


void test_cast()
{

}

void test_dyn()
{
	B *p1 = new B();
	A* p2 = p1;
	auto& type = typeid(A);
	auto& type1 = typeid(p1);
	auto& type2 = typeid(p2);
	auto& type3 = typeid(*p2);
	auto p3 = dynamic_cast<void*>(p2);

	printf("%s %s %s %s %s \n", type.name(), type1.name(), type2.name(), type3.name(), typeid(C).name());

	assert(p1 == p2);
	assert(dynamic_cast<B*>(p1) != nullptr);
}

void test_sidecast()
{
	CC *p1 = new CC();
	AA *p2 = p1;
	BB *p3 = dynamic_cast<BB*>(p2);
	BB* p4 = p1;
	BB* p5 = static_cast<BB*>(p1);
	EE* p6 = dynamic_cast<EE*>(p2);
	BB* p7 = dynamic_cast<BB*>(p6);

	assert(p3 != nullptr);
	assert(p4 != nullptr);
}

#endif

