#pragma once

/* structures */

struct s_network_globals
{
	bool initialized;
	bool halt_on_critical_events;
};

/* prototypes */

void network_globals_apply_patches(void);

s_network_globals* network_globals_get(void);

bool network_initialized(void);

void network_initialize(void);

void network_dispose(void);

void __cdecl network_receive(void);

void __cdecl network_send(void);

void __cdecl network_update(void);
