#pragma once
#include "3rdparty/detours/include/detours.h"

/* macros */

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


// Invokes a function
// ADDR_CLIENT: file offset in halo2.exe
// ADDR_SERVER: file offset in h2server.exe
// TYPE: function
// __VA_ARGS__: arguments for the function we want to invoke
// NOTE:
// if server or client is equal to 0 then we pass the same address for both (ONLY ON RELEASE BUILDS)
// REASON:
// optimizes out a cmov or related instruction when building release whenever we call a function that doesn't have a dedi address specified
#ifdef NDEBUG
#define INVOKE_BY_TYPE(_addr_client, _addr_server, _type, ...)	\
Memory::GetAddress<_type>(_addr_client == 0 ? _addr_server : _addr_client, _addr_server == 0 ? _addr_client : _addr_server)(__VA_ARGS__)
#else
#define INVOKE_BY_TYPE(_addr_client, _addr_server, _type, ...)	\
Memory::GetAddress<_type>(_addr_client, _addr_server)(__VA_ARGS__)
#endif

#define INVOKE(_addr_client, _addr_server, _fn_decl, ...) INVOKE_BY_TYPE(_addr_client, _addr_server, decltype(_fn_decl)*, __VA_ARGS__)
// ### TODO find better name
#define INVOKE_TYPE(_addr_client, _addr_server, _type, ...) INVOKE_BY_TYPE(_addr_client, _addr_server, _type, __VA_ARGS__)

#define INVOKE_VFPTR_FN(_get_table_fn, _index, _type, ...) \
	(this->**_get_table_fn<_type>(_index))(__VA_ARGS__)

#define INVOKE_CLASS_FN(classobj, functionPtr)  ((classobj)->*(functionPtr))

/* prototypes */

void *DetourFunc(BYTE *src, const BYTE *dst, const unsigned int len);
void RetourFunc(BYTE *src, BYTE *restore, const unsigned int len);
void *DetourClassFunc(BYTE *src, const BYTE *dst, const unsigned int len);
void RetourClassFunc(BYTE *src, BYTE *restore, const unsigned int len);

void Codecave(uintptr_t destAddress, VOID(*func)(VOID), BYTE nopCount);
void WriteBytes(uintptr_t destAddress, LPVOID bytesToWrite, const unsigned int numBytes);
void PatchCall(uintptr_t call_addr, uintptr_t new_function_ptr);
void WritePointer(uintptr_t offset, const void *ptr);
void PatchWinAPICall(uintptr_t call_addr, uintptr_t new_function_ptr);
void NopFill(uintptr_t address, const unsigned int length);
void ReadBytesProtected(uintptr_t address, BYTE* buf, BYTE count);

inline void PatchCall(uintptr_t call_addr, void *new_function_ptr)
{
	PatchCall(call_addr, (uintptr_t)(new_function_ptr));
}

inline void PatchCall(void *call_addr, void *new_function_ptr)
{
	PatchCall((uintptr_t)call_addr, (uintptr_t)new_function_ptr);
}

inline void PatchWinAPICall(uintptr_t call_addr, void *new_function_ptr)
{
	PatchWinAPICall(call_addr, (uintptr_t)(new_function_ptr));
}

inline void PatchWinAPICall(void *call_addr, void *new_function_ptr)
{
	PatchWinAPICall((uintptr_t)(call_addr), (uintptr_t)(new_function_ptr));
}

template <typename T>
inline void WriteValue(uintptr_t offset, T data)
{
	WriteBytes(offset, &data, sizeof(T));
}

inline void WriteJmpTo(uintptr_t call_addr, uintptr_t new_function_ptr)
{
	BYTE call_patch[1] = { 0xE9 };
	WriteBytes(call_addr, call_patch, 1);
	PatchCall(call_addr, new_function_ptr);
}

inline void WriteJmpTo(uintptr_t call_addr, void *new_function_ptr)
{
	WriteJmpTo(call_addr, (uintptr_t)(new_function_ptr));
}

inline void WriteJmpTo(void *call_addr, void *new_function_ptr)
{
	WriteJmpTo((uintptr_t)(call_addr), (uintptr_t)(new_function_ptr));
}
