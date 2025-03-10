#pragma once

#include "3rdparty/detours/include/detours.h"

#define WIN32_LEAN_AND_MEAN
#define JMP_OP_CODE 0xEB
#define JNZ_OP_CODE 0x75

#define DETOUR_BEGIN() \
do \
{ \
	DetourTransactionBegin(); \
	DetourUpdateThread(GetCurrentThread()); \
} while (0)

#define DETOUR_ATTACH(_ptr_func, _address, _target_ptr) \
do \
{ \
	(_ptr_func) = (_address); \
	DetourAttach(&(PVOID&)(_ptr_func), _target_ptr); \
} while (0)

#define DETOUR_COMMIT() \
	DetourTransactionCommit();

#ifdef LLVM
#define CLASS_HOOK_DECLARE_LABEL(name, member) static auto name = &member

#else
#define CLASS_HOOK_DECLARE_LABEL(name, member)
#endif

#ifdef LLVM
#define CLASS_HOOK_JMP(name, member) __asm jmp (name)
#else
#define CLASS_HOOK_JMP(name, member) __asm jmp (member)
#endif

void *DetourFunc(BYTE *src, const BYTE *dst, const unsigned int len);
void RetourFunc(BYTE *src, BYTE *restore, const unsigned int len);
void *DetourClassFunc(BYTE *src, const BYTE *dst, const unsigned int len);
void RetourClassFunc(BYTE *src, BYTE *restore, const unsigned int len);

void *VTableFunction(void *ClassPtr, size_t index);
void Codecave(size_t destAddress, VOID(*func)(VOID), BYTE nopCount);
void WriteBytes(size_t destAddress, LPVOID bytesToWrite, const unsigned int numBytes);
void PatchCall(size_t call_addr, size_t new_function_ptr);
void WritePointer(size_t offset, const void *ptr);
void PatchWinAPICall(size_t call_addr, size_t new_function_ptr);
void NopFill(size_t address, const unsigned int length);
void ReadBytesProtected(size_t address, BYTE* buf, BYTE count);

inline void PatchCall(size_t call_addr, void *new_function_ptr)
{
	PatchCall(call_addr, (size_t)(new_function_ptr));
}

inline void PatchCall(void *call_addr, void *new_function_ptr)
{
	PatchCall((size_t)call_addr, (size_t)new_function_ptr);
}

inline void PatchWinAPICall(size_t call_addr, void *new_function_ptr)
{
	PatchWinAPICall(call_addr, (size_t)(new_function_ptr));
}

inline void PatchWinAPICall(void *call_addr, void *new_function_ptr)
{
	PatchWinAPICall((size_t)(call_addr), (size_t)(new_function_ptr));
}

template <typename T>
inline void WriteValue(size_t offset, T data)
{
	WriteBytes(offset, &data, sizeof(T));
}

inline void WriteJmpTo(size_t call_addr, size_t new_function_ptr)
{
	BYTE call_patch[1] = { 0xE9 };
	WriteBytes(call_addr, call_patch, 1);
	PatchCall(call_addr, new_function_ptr);
}

inline void WriteJmpTo(size_t call_addr, void *new_function_ptr)
{
	WriteJmpTo(call_addr, (size_t)(new_function_ptr));
}

inline void WriteJmpTo(void *call_addr, void *new_function_ptr)
{
	WriteJmpTo((size_t)(call_addr), (size_t)(new_function_ptr));
}
