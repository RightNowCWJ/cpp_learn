/*
test for Data Member
*/

#pragma once

#include <stdio.h>

class Vector3 {
public:
	float x = 0.0f, y = 0.0f, z = 0.0f;

	void move_z(float delta) {
		z += delta;
	}

	void move(float delta, float Vector3::* dest)
	{
		this->*dest += delta;
	}
};

class Normal {
public:
	float nx = 0.0f, ny = 1.0f, nz = 0.0f;
};

class Vertex : public Vector3, public Normal {
	float u, v;
};

void test_data_ptr()
{
	Vector3 v;
	auto p_x = &v.x;
	auto ptr_Vector_x = &Vector3::x;

	// &v + offset_x
	v.*ptr_Vector_x = 3.0;
	printf("[%s] ptr_x:%p\n", __FUNCTION__, ptr_Vector_x);
}

void test_data_mi_ptr()
{
	Vertex vtx;
	float Normal::*ptr_ny = &Normal::ny;
	auto ptr_Vector_y = &Vector3::y;

	// &v + offset_ny + sizeof(Normal)
	vtx.*ptr_ny = 0.5;

	// &v + offset_y
	vtx.*ptr_Vector_y = 0.5;

	printf("[%s] ptr_y:%p, ptr_ny:%p ny:%.3f \n", __FUNCTION__, ptr_Vector_y, ptr_ny, vtx.ny);
}

void test_data_move()
{
	Vector3 v;
	v.move(1.0f, &Vector3::z);
}


