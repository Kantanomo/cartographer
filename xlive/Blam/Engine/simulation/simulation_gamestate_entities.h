#pragma once

/* prototypes */

void simulation_gamestate_entities_notify_simulation_world_reset(void);

void simulation_gamestate_index_encode(class c_bitstream* bitstream, datum object_index);
void simulation_gamestate_index_decode(class c_bitstream* bitstream, datum* object_index);

void simulation_entity_index_encode(class c_bitstream* bitstream, int32 entity_index);
void simulation_entity_index_decode(class c_bitstream* bitstream, int32* entity_index);
