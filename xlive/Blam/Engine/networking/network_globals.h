#pragma once

/* structures */

struct s_network_globals
{
	bool network_initialized;
	bool halt_on_critical_events;
};

/* prototypes */

void network_globals_apply_patches(void);

s_network_globals* network_globals_get(void);

void network_globals_initialize(void);

void network_globals_dispose(void);
