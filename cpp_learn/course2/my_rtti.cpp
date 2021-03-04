#include "my_rtti.h"
#include <ehdata_forceinclude.h>
#include <rttidata.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
	https://en.cppreference.com/w/cpp/language/dynamic_cast
	https://zh.cppreference.com/w/cpp/language/dynamic_cast

	dynamic_cast < 新类型 > ( 表达式 )

	运行时检查(runtime check)的条件
		只有表达式是到多态类型的基类的指针或引用，并且新类型是到派生类的指针或引用，才会进行运行时检查

	运行时检查的定义
		a. 检验 表达式 所指向/标识的最终派生对象。若在该对象（最终派生对象）中 表达式 指向/指代 Derived 的公开基类，且只有一个 Derived 类型（新类型）对象从 表达式 所指向/标识的子对象派生，则转型结果指向/指代该 Derived 对象。（此之谓“向下转型（downcast）”。）
		b. 否则，若 表达式 指向/指代最终派生对象的公开基类，而同时最终派生对象拥有 Derived 类型（新类型）的无歧义公开基类，则转型结果指向/指代该 Derived（此之谓“侧向转型（sidecast, crosscast）”。）
		c. 否则，检查失败
*/

/*
	判断类型是否相等
*/
bool typeid_is_equal(const TypeDescriptor* const lhs, const TypeDescriptor* const rhs)
{
	return lhs == rhs || !strcmp(lhs->name, rhs->name);
}

/*
	根据多态对象指针，得到完整对象定位器（CompleteObjectLocator）
*/
static inline _RTTICompleteObjectLocator *get_CompleteObjectLocator_from_object(void *ptr_to_object)
{
	// 虚表的地址存储在对象的顶部，即ptr_to_object[0]
	// CompleteObjectLocator对象的地址存储在虚表的前一个指针大小位置，即vf_ptr[-1]
	return static_cast<_RTTICompleteObjectLocator***>(ptr_to_object)[0][-1];
}

/*
	根据多态对象指针找到完整对象（complete_object）指针
*/
static void* find_complete_object(void *input_ptr)
{
	// 先获取CompleteObjectLocator对象
	const auto complete_locator = get_CompleteObjectLocator_from_object(input_ptr);

	// 根据储存在CompleteObjectLocator的偏移信息，计算得到完整对象的地址
	const auto input_addr = reinterpret_cast<uintptr_t>(input_ptr);
	auto complete_object_addr = input_addr - complete_locator->offset;
	const auto cd_offset = complete_locator->cdOffset;
	if (cd_offset > 0)
	{
		const auto offset_value = *reinterpret_cast<int *>(input_addr - cd_offset);
		complete_object_addr -= offset_value;
	}

	// 最后，将得到的地址转换为指针返回
	return reinterpret_cast<void*>(complete_object_addr);
}

static ptrdiff_t pmd_to_offset(
	void *this_ptr,			// 完整对象地址
	const PMD& pmd)			// 类成员指针结构体
{
	ptrdiff_t ret_offset = 0;

	if (pmd.pdisp >= 0) 
	{
		ret_offset = pmd.pdisp;
		ret_offset += *(__int32*)((char*)*(ptrdiff_t*)((char*)this_ptr + ret_offset) + pmd.vdisp);
	}

	ret_offset += pmd.mdisp;
	return ret_offset;
}

/*
	单继承下，找到指向目标类型子对象的地址

	参考运行时检查的定义

	在单继承下，在继承层次中，任何特定类型都有明确的实例。并且，不存在cross-cast，因此只存在down-cast的检查。
	向上转换会因为访问性问题而失败，对不存在于最派生类的继承层次中的类型进行转换也会失败

*/
static _RTTIBaseClassDescriptor* find_signle_interit_target_type_instance(
	_RTTICompleteObjectLocator* const complete_locator,
	TypeDescriptor* const src_type,
	TypeDescriptor* const target_type
)
{
	auto class_descriptor = COL_PCHD(*complete_locator);
	_RTTIBaseClassArray *base_class_array = CHD_PBCA(*class_descriptor);
	auto num_base_classes = class_descriptor->numBaseClasses;

	for (unsigned long i = 0; i < num_base_classes; ++i)
	{
		// 遍历继承层次，从最派生类开始
		_RTTIBaseClassDescriptor *bcd = CHD_PBCD(base_class_array->arrayOfBaseClassDescriptors[i]);

		if (typeid_is_equal(BCD_PTD(*bcd), target_type))
		{
			// 找到目标类型在继承层次中
			for (unsigned long j = i + 1; j < num_base_classes; ++j)
			{
				// 遍历后续的基类，尝试找到源类型
				_RTTIBaseClassDescriptor *source_bcd = CHD_PBCD(base_class_array->arrayOfBaseClassDescriptors[j]);
				if (source_bcd->attributes & BCD_PRIVORPROTBASE)
				{
					// 目标类型与源类型之间存在非公有继承（保护继承或者私有继承），那么转换失败
					return nullptr;
				}

				if (typeid_is_equal(BCD_PTD(*source_bcd), src_type))
				{
					// 找到了源类型
					return source_bcd;
				}
			}

			// 找不到源类型作为目标类型的基类，所以肯定不是down-cast
			// 而在单继承下，不存在cross-cast
			// 其他的情况都是不合理的，转换失败
			return nullptr;
		}
	}

	// 目标类型不在继承层次中，转换失败
	return nullptr;
}

/*
	多继承非虚拟继承
*/
static _RTTIBaseClassDescriptor* find_multi_interit_target_type_instance(
	void *complete_object,
	_RTTICompleteObjectLocator *complete_locator,
	TypeDescriptor *src_type,
	ptrdiff_t src_offset,
	TypeDescriptor *target_type
)
{
	_RTTIBaseClassDescriptor *bcd, *target_bcd = nullptr,
		*source_bcd = nullptr, *source_in_target_cbd;

	_RTTIBaseClassArray *base_class_array = CHD_PBCA(*COL_PCHD(*complete_locator));
	_RTTIBaseClassArray *target_base_class_array;

	unsigned long idx = 0;
	unsigned long num_complete_object_bases = COL_PCHD(*complete_locator)->numBaseClasses;
	unsigned long num_target_bases = 0;
	unsigned long idx_target = -1;

	for (idx = 0; idx < num_complete_object_bases; idx++)
	{
		bcd = CHD_PBCD(base_class_array->arrayOfBaseClassDescriptors[idx]);

		if (idx - idx_target > num_target_bases && typeid_is_equal(BCD_PTD(*bcd), target_type))
		{
			if (source_bcd)
			{
				if ((bcd->attributes & (BCD_NOTVISIBLE | BCD_AMBIGUOUS)) ||
					(source_bcd->attributes & BCD_NOTVISIBLE))
				{
					return nullptr;
				}
				else
				{
					return bcd;
				}
			}

			target_bcd = bcd;
			idx_target = idx;
			num_target_bases = bcd->numContainedBases;
		}

	}

}

void* dynamic_cast_imp(
	void *input_ptr,			// 多态对象的指针
	long vf_delta,				// 对象地址到虚表指针的偏移量
	void *src_void,				// 多态对象指针指向的静态类型
	void *target_void			// 希望转换到的目标类型
)
{
	if (input_ptr == nullptr)
	{
		return nullptr;
	}

	const auto src_type = static_cast<TypeDescriptor*>(src_void);
	const auto target_type = static_cast<TypeDescriptor*>(target_void);

	void* complete_object = find_complete_object(input_ptr);
	const auto complete_allocator = get_CompleteObjectLocator_from_object(input_ptr);

	_RTTIBaseClassDescriptor *base_class = nullptr;
	if (!((COL_PCHD(*complete_allocator))->attributes & CHD_MULTINH))
	{
		// 非多重继承
		base_class = find_signle_interit_target_type_instance(complete_allocator, src_type, target_type);
	}
	else
	{
		if (!(COL_PCHD(*complete_allocator)->attributes & CHD_VIRTINH))
		{
			// 多重非虚拟继承
		}
		else
		{
			// 多重虚拟继承
		}
	}

	if (base_class == nullptr)
	{
		return nullptr;
	}

	return reinterpret_cast<void *>(
		reinterpret_cast<uintptr_t>(complete_object) + pmd_to_offset(complete_object, base_class->where));
	
}


