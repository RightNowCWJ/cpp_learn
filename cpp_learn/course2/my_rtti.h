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

class B: protected A {
};

class C: public B {};

struct MA {
	virtual void func() {};
};

struct MB {
	virtual void func_ex() {};
};

struct MC : public MA, public MB {
};

struct VA {
public:
	virtual void func() {};
};

struct VB: virtual VA {
};

struct VC: virtual VA {
};

struct VD: public VB, public VC {
};


void test_cast()
{

}

void test_dyn()
{
	C *p1 = new C();
	A* p2 = new A();
	C* p3 = dynamic_cast<C*>(p2);
	// A* p4 = dynamic_cast<A*>(p3);
}


template<typename target_type, typename src_type>
target_type* my_dynamic_cast(src_type* input_ptr)
{
	
}

void* dynamic_cast_imp(
	void *input_ptr,
	long vf_delta,
	void *src_void,
	void *target_void
);


void test_mcast()
{
	MC *p1 = new MC();

	// up_cast
	MA* p2 = p1;

	// side_cast
	MB* p3 = dynamic_cast<MB*>(p2);

	// down_cast
	MC* p4 = dynamic_cast<MC*>(p3);

}


void test_vcast()
{
	VD *p1 = new VD();

	// up_cast
	VB* p2 = p1;

	// side_cast
	VC* p3 = dynamic_cast<VC*>(p2);

	// down_cast
	VD* p4 = dynamic_cast<VD*>(p3);
}


#endif

