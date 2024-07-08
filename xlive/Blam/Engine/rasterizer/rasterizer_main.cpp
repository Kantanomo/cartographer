#include "stdafx.h"
#include "rasterizer_main.h"

#include "bitmaps/bitmaps.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"
#include "shell/shell_windows.h"

#include "H2MOD/Modules/Shell/Config.h"

/* prototypes */

void __cdecl rasterizer_present_hook(bitmap_data* bitmap);

/* public code */

void rasterizer_main_apply_patches(void)
{
    // present hooks for the frame limiter
    PatchCall(Memory::GetAddress(0x19073C), rasterizer_present_hook);
    PatchCall(Memory::GetAddress(0x19074C), rasterizer_present_hook);
    return;
}

/* private code */

// rasterizer_present hook
// used to limit framerate using our implementation
void __cdecl rasterizer_present_hook(bitmap_data* bitmap)
{
    rasterizer_present(bitmap);
    shell_windows_throttle_framerate(H2Config_fps_limit);
    return;
}
