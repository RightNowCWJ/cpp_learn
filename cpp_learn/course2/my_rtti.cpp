#include "my_rtti.h"
#include <ehdata_forceinclude.h>
#include <rttidata.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
	https://en.cppreference.com/w/cpp/language/dynamic_cast
	https://zh.cppreference.com/w/cpp/language/dynamic_cast

	dynamic_cast < ������ > ( ���ʽ )

	����ʱ���(runtime check)������
		ֻ�б��ʽ�ǵ���̬���͵Ļ����ָ������ã������������ǵ��������ָ������ã��Ż��������ʱ���

	����ʱ���Ķ���
		a. ���� ���ʽ ��ָ��/��ʶ�����������������ڸö����������������� ���ʽ ָ��/ָ�� Derived �Ĺ������࣬��ֻ��һ�� Derived ���ͣ������ͣ������ ���ʽ ��ָ��/��ʶ���Ӷ�����������ת�ͽ��ָ��/ָ���� Derived ���󡣣���֮ν������ת�ͣ�down-cast��������
		b. ������ ���ʽ ָ��/ָ��������������Ĺ������࣬��ͬʱ������������ӵ�� Derived ���ͣ������ͣ��������幫�����࣬��ת�ͽ��ָ��/ָ���� Derived����֮ν������ת�ͣ�side-cast, cross-cast��������
		c. ���򣬼��ʧ��
*/

/*
	�ж������Ƿ����
*/
bool typeid_is_equal(const TypeDescriptor* const lhs, const TypeDescriptor* const rhs)
{
	return lhs == rhs || !strcmp(lhs->name, rhs->name);
}

/*
	���ݶ�̬����ָ�룬�õ���������λ����CompleteObjectLocator��
*/
static inline _RTTICompleteObjectLocator *get_CompleteObjectLocator_from_object(void *ptr_to_object)
{
	// ���ĵ�ַ�洢�ڶ���Ķ�������ptr_to_object[0]
	// CompleteObjectLocator����ĵ�ַ�洢������ǰһ��ָ���Сλ�ã���vf_ptr[-1]
	return static_cast<_RTTICompleteObjectLocator***>(ptr_to_object)[0][-1];
}

/*
	���ݶ�̬����ָ���ҵ���������complete_object��ָ��
*/
static void* find_complete_object(void *input_ptr)
{
	// �Ȼ�ȡCompleteObjectLocator����
	const auto complete_locator = get_CompleteObjectLocator_from_object(input_ptr);

	// ���ݴ�����CompleteObjectLocator��ƫ����Ϣ������õ���������ĵ�ַ
	const auto input_addr = reinterpret_cast<uintptr_t>(input_ptr);
	auto complete_object_addr = input_addr - complete_locator->offset;
	const auto cd_offset = complete_locator->cdOffset;
	if (cd_offset > 0)
	{
		const auto offset_value = *reinterpret_cast<int *>(input_addr - cd_offset);
		complete_object_addr -= offset_value;
	}

	// ��󣬽��õ��ĵ�ַת��Ϊָ�뷵��
	return reinterpret_cast<void*>(complete_object_addr);
}

static ptrdiff_t pmd_to_offset(
	void *this_ptr,			// ���������ַ
	const PMD& pmd)			// ���Աָ��ṹ��
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
	���̳��£��ҵ�ָ��Ŀ�������Ӷ���ĵ�ַ

	�ο�����ʱ���Ķ���

	�ڵ��̳��£��ڼ̳в���У��κ��ض����Ͷ�����ȷ��ʵ�������ң�������cross-cast�����ֻ����down-cast�ļ�顣
	����ת������Ϊ�����������ʧ�ܣ��Բ���������������ļ̳в���е����ͽ���ת��Ҳ��ʧ��

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
		// �����̳в�Σ����������࿪ʼ
		_RTTIBaseClassDescriptor *bcd = CHD_PBCD(base_class_array->arrayOfBaseClassDescriptors[i]);

		if (typeid_is_equal(BCD_PTD(*bcd), target_type))
		{
			// �ҵ�Ŀ�������ڼ̳в����
			for (unsigned long j = i + 1; j < num_base_classes; ++j)
			{
				// ���������Ļ��࣬�����ҵ�Դ����
				_RTTIBaseClassDescriptor *source_bcd = CHD_PBCD(base_class_array->arrayOfBaseClassDescriptors[j]);
				if (source_bcd->attributes & BCD_PRIVORPROTBASE)
				{
					// Ŀ��������Դ����֮����ڷǹ��м̳У������̳л���˽�м̳У�����ôת��ʧ��
					return nullptr;
				}

				if (typeid_is_equal(BCD_PTD(*source_bcd), src_type))
				{
					// �ҵ���Դ����
					return source_bcd;
				}
			}

			// �Ҳ���Դ������ΪĿ�����͵Ļ��࣬���Կ϶�����down-cast
			// ���ڵ��̳��£�������cross-cast
			// ������������ǲ�����ģ�ת��ʧ��
			return nullptr;
		}
	}

	// Ŀ�����Ͳ��ڼ̳в���У�ת��ʧ��
	return nullptr;
}

/*
	��̳з�����̳�
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
		*source_bcd = nullptr, *source_in_target_bcd;

	_RTTIBaseClassArray *base_class_array = CHD_PBCA(*COL_PCHD(*complete_locator));
	_RTTIBaseClassArray *target_base_class_array;

	unsigned long idx = 0;
	unsigned long num_complete_object_bases = COL_PCHD(*complete_locator)->numBaseClasses;
		// Calculate offset of source object in complete object
	unsigned long num_target_bases = 0;
	unsigned long idx_target = -1;

	// base_class_array����ȫ����ļ̳в�νṹ���������飬��������ȣ��������ҵ�����ʽ������������λ������ĵ�0λ
	// ͨ������һ��base_class_array���ҵ�down-cast��cross-cast
	for (idx = 0; idx < num_complete_object_bases; idx++)
	{
		bcd = CHD_PBCD(base_class_array->arrayOfBaseClassDescriptors[idx]);

		// ����Ƿ��ҵ�Ŀ������
		if (idx - idx_target > num_target_bases && typeid_is_equal(BCD_PTD(*bcd), target_type))
		{
			// ����������ҵ���ԭ���ͣ���ô����ֻ����cross-cast����up-cast
			// Ŀ�����ͱ��������������͵Ĺ��к���ȷ�Ļ��࣬����Դ���ͱ��������������͵Ĺ��л��࣬����ת��ʧ��
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

			// ��¼�ҵ���Ŀ�����ͣ����䴦����������̳в�ε��±꣬�Լ����Ļ������
			target_bcd = bcd;
			idx_target = idx;
			num_target_bases = bcd->numContainedBases;
		}

		if (typeid_is_equal(BCD_PTD(*bcd), src_type) && 
			pmd_to_offset(complete_object, bcd->where) == src_offset)
		{
			if (target_bcd)
			{
				if (idx - idx_target <= num_target_bases)
				{
					// ԭ������Ŀ�����͵ĸ��࣬��������down-cast��ԭ���ͱ�����Ŀ�����͵Ĺ��л������ת���ɹ�

					// ���Ŀ�����͵ļ̳в��������������ԭ�����Ƿ��Ŀ�����Ϳɼ�
					// Ŀ�����͵�BaseClassArray�����������BaseClassArray���Ӽ���(base_class_array[target_idx, target_idx + num_target_bases])������ͬ�Ľṹ
					// ͨ���±�(idx - idx_target)�ҵ�ԭ������Ŀ�����ͼ̳в���µ�������
					target_base_class_array = CHD_PBCA(*BCD_PCHD(*target_bcd));
					source_in_target_bcd = target_base_class_array->arrayOfBaseClassDescriptors[idx - idx_target];

					if (source_in_target_bcd->attributes & BCD_NOTVISIBLE)
					{
						return nullptr;
					}
					else
					{
						return target_bcd;
					}
					
				}
				else
				{
					// ������cross-cast
					// Ŀ�����ͱ��������������͵Ĺ��еĺ���ȷ�Ļ��࣬����ԭ���ͱ��������������͵Ĺ��л���

					if ((target_bcd->attributes & (BCD_NOTVISIBLE | BCD_AMBIGUOUS)) ||
						(bcd->attributes & BCD_NOTVISIBLE))
					{
						return nullptr;
					}
					else
					{
						return target_bcd;
					}
				}
			}

			// ��¼�ҵ���ԭ����
			source_bcd = bcd;
		}

	}

	// �ڼ̳в����û���ҵ�ԭ���ͻ���û���ҵ�Ŀ�����ͣ��������ת��ʧ��
	return nullptr;

}

void* dynamic_cast_imp(
	void *input_ptr,			// ��̬�����ָ��
	long vf_delta,				// �����ַ�����ָ���ƫ����
	void *src_void,				// ��̬����ָ��ָ��ľ�̬����
	void *target_void			// ϣ��ת������Ŀ������
)
{
	if (input_ptr == nullptr)
	{
		return nullptr;
	}

	const auto src_type = static_cast<TypeDescriptor*>(src_void);
	const auto target_type = static_cast<TypeDescriptor*>(target_void);

	// ��������ָ��
	void *complete_object = find_complete_object(input_ptr);
	// ��������λ��
	const auto complete_locator = get_CompleteObjectLocator_from_object(input_ptr);

	_RTTIBaseClassDescriptor *base_class = nullptr;
	if (!((COL_PCHD(*complete_locator))->attributes & CHD_MULTINH))
	{
		// �Ƕ��ؼ̳�
		base_class = find_signle_interit_target_type_instance(
			complete_locator, src_type, target_type);
	}
	else
	{

		input_ptr = ((char *)input_ptr - vf_delta);

		ptrdiff_t inptr_delta = (char *)input_ptr - (char *)complete_object;

		if (!(COL_PCHD(*complete_locator)->attributes & CHD_VIRTINH))
		{
			// ���ط�����̳�
			base_class = find_multi_interit_target_type_instance(
				complete_object, complete_locator, src_type, inptr_delta, target_type);
		}
		else
		{
			// ��������̳�
		}
	}

	if (base_class == nullptr)
	{
		return nullptr;
	}

	return reinterpret_cast<void *>(
		reinterpret_cast<uintptr_t>(complete_object) + pmd_to_offset(complete_object, base_class->where));
	
}


