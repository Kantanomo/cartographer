#include "stdafx.h"
#include "hs.h"

#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"

/* prototypes */

void __cdecl hs_print(const char* text);

/* public code */

void hs_apply_patches(void)
{
	// hook the print command to redirect the text to our console
	PatchCall(Memory::GetAddress(0xE9E50), hs_print);
	return;
}

void __cdecl hs_update(void)
{
	INVOKE(0xA37A9, 0x95A09, hs_update);
	return;
}

/* private code */

// TODO: properly implement this
void __cdecl hs_print(const char* text)
{
	std::string finalOutput("[HSC Print] "); finalOutput += text;
	addDebugText(finalOutput.c_str());
	return;
}
