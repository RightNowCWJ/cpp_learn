#include "test1.h"
#include "test2.h"
#include "my_rtti.h"

int main() {
	test_data_ptr();
	test_data_mi_ptr();
	test_data_move();
	test_virtual_function();
	test_dyn();
	test_sidecast();
	return 0;
}

