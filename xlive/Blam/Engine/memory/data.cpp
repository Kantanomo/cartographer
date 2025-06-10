#include "stdafx.h"
#include "data.h"

/* prototypes */

int32 data_next_absolute_index(data_array* data, int32 index);

/* public code */

void* datum_get(const data_array* data_array, datum datum_index)
{
	return (char*)data_array->data + data_array->size * DATUM_INDEX_TO_ABSOLUTE_INDEX(datum_index);
}

void* datum_try_and_get(const data_array* data_array, datum datum_index)
{
	return INVOKE(0x6639B, 0x32087, datum_try_and_get, data_array, datum_index);
}

void* datum_get_absolute(const data_array* data_array, int32 index)
{
	return (char*)data_array->data + data_array->size * index;
}

void __cdecl datum_delete(data_array* data_array, datum datum_index)
{
	INVOKE(0x6693E, 0x3262A, datum_delete, data_array, datum_index);
	return;
}

size_t align_address(size_t size, int32 alignment_bits)
{
	int32 bit = FLAG(alignment_bits);
	return ~(bit - 1) & (bit - 1 + size);
}

int32 data_allocation_size(int32 maximum_count, int32 size, int32 alignment_bits)
{
	ASSERT(maximum_count > 0 && maximum_count <= k_unsigned_short_max);
	ASSERT(size > 0);
	ASSERT(alignment_bits >= 0);

	ASSERT((size_t)size == align_address(size, alignment_bits));


	int32 alignment = (1 << alignment_bits);
	return size * maximum_count + BIT_VECTOR_SIZE_IN_BYTES(maximum_count) + alignment + 75;
}

void __cdecl data_initialize(
	data_array* data,
	const char* data_name,
	int32 maximum_count,
	int32 size,
	int32 alignment_bits,
	c_allocation_base* allocator)
{
	return INVOKE(0x665AD, 0x32299, data_initialize, data, data_name, maximum_count, size, alignment_bits, allocator);
}

#ifdef _DEBUG
data_array* data_new(
	const char* data_name,
	int32 maximum_count,
	int32 size,
	int32 alignment_bits,
	const char* filename,
	int32 line_number,
	c_allocation_base* allocator)
#else
data_array* data_new(
	const char* data_name,
	int32 maximum_count,
	int32 size,
	int32 alignment_bits,
	c_allocation_base* allocator)
#endif
{
	int32 alloc_size = data_allocation_size(maximum_count, size, alignment_bits);
	data_array* result = (data_array*)(allocator)->alloc(alloc_size);

	if (result)
	{
		data_initialize(result, data_name, maximum_count, size, alignment_bits, allocator);
		SET_FLAG(result->flags, 2, true);
	}

	return result;
}


#ifdef _DEBUG
void data_dispose(data_array* data, const char* filename, int32 line_number)
#else
void data_dispose(data_array* data)
#endif
{
	csmemset(data, 0, 76u);
	if (data->allocator)
	{
		data->allocator->free_block(data);
	}
	return;
}

void __cdecl data_delete_all(data_array* data)
{
	INVOKE(0x66715, 0x32401, data_delete_all, data);
	return;
}

datum __cdecl datum_new(data_array* data_array)
{
	return INVOKE(0x667A0, 0x3248C, datum_new, data_array);
}

datum __cdecl datum_new_at_index(data_array* data_array, datum datum_index)
{
	return INVOKE(0x66858, 0x32544, datum_new_at_index, data_array, datum_index);
}

datum __cdecl datum_new_at_absolute_index(data_array* data, int32 absolute_index)
{
	ASSERT(DATUM_INDEX_TO_ABSOLUTE_INDEX(absolute_index) == absolute_index);
	ASSERT(DATUM_INDEX_TO_IDENTIFIER(absolute_index) == 0);

	return INVOKE(0x668D5, 0x325C1, datum_new_at_absolute_index, data, absolute_index);
}


uint32 __cdecl datum_header_allocate(uint32 total_size, uint32 alignment_bits)
{
	return INVOKE(0x37E69, 0x2B4E6, datum_header_allocate, total_size, alignment_bits);
}

bool __cdecl datum_header_deallocate(void* object)
{
	// todo: server offset
	return INVOKE(0x37EC3, 0, datum_header_deallocate, object);
}

void _cdecl data_make_valid(data_array* data_array)
{
	return INVOKE(0x66B33, 0x3281F, data_make_valid, data_array);
}

int32 data_next_index(data_array* data, datum index)
{
	index = (index == NONE ? 0 : DATUM_INDEX_TO_ABSOLUTE_INDEX(index) + 1);
	int32 absolute_index = data_next_absolute_index(data, index);

	void* salt = (char*)data->data + absolute_index * data->size;
	return (absolute_index != NONE ? DATUM_INDEX_NEW(absolute_index, *(int16*)salt) : NONE);
}

void data_make_invalid(data_array* data)
{
	data->valid = false;
	return;
}

datum __cdecl datum_absolute_index_to_index(data_array* data, int32 absolute_index)
{
	return INVOKE(0x664C3, 0x0, datum_absolute_index_to_index, data, absolute_index);
}

void iterator_new(data_iterator* iterator, data_array* data)
{
	ASSERT(data->valid);

	iterator->data = data;
	iterator->absolute_index = NONE;
	iterator->index = NONE;
	return;
}

void* iterator_next(data_iterator* iterator)
{
	void* result;
	const data_array* data = iterator->data;
	const int32 absolute_index = data_next_absolute_index(iterator->data, iterator->absolute_index + 1);
	if (absolute_index == NONE)
	{
		result = NULL;
		iterator->absolute_index = data->maximum_count;
		iterator->index = NONE;
	}
	else
	{
		result = (char*)data->data + absolute_index * data->size;
		iterator->absolute_index = absolute_index;
		iterator->index = DATUM_INDEX_NEW(absolute_index, *(int16*)result);
	}
	return result;
}

/* private code */

int32 data_next_absolute_index(data_array* data, int32 index)
{
	if (index < 0 || index >= data->first_free_absolute_index)
	{
		index = NONE;
	}
	else
	{
		while (!BIT_VECTOR_TEST_FLAG(data->in_use_bit_vector, index))
		{
			if (++index >= data->first_free_absolute_index)
			{
				index = NONE;
				break;
			}
		}
	}

	return index;
}
