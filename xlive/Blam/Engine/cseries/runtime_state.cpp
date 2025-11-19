#include "stdafx.h"
#include "runtime_state.h"

/* public code */

void __cdecl runtime_state_initialize(void)
{
	INVOKE(0x37ED5, 0x2F79C, runtime_state_initialize);
	return;
}

uintptr_t address_from_pointer(const void* pointer)
{
	return (uintptr_t)pointer;
}

void* pointer_from_address(uintptr_t address)
{
	return (void*)address;
}

uintptr_t align_address(uintptr_t address, int32 alignment_bits)
{
	return ~((1 << alignment_bits) - 1) & ((1 << alignment_bits) - 1 + address);
}

void* align_pointer(void* pointer, int32 alignment_bits)
{
	const uintptr_t address = address_from_pointer(pointer);
	const uintptr_t aligned_address = align_address(address, alignment_bits);
	return pointer_from_address(aligned_address);
}
