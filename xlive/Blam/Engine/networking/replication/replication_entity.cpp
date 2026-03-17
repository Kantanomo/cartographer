#include "stdafx.h"
#include "replication_entity.h"

#include "memory/bitstream.h"

/* public code */

void replication_entity_index_decode(
	c_bitstream* packet,
	int32* entity_index)
{
	ASSERT(packet);
	ASSERT(entity_index);

	uint32 entity_abs_index = packet->read_integer("entity-absolute-index", 10);
	uint8 seed = (uint8)packet->read_integer("entity-seed", 4);

	*entity_index = entity_abs_index | (seed << 28);
	return;
}

void replication_entity_index_decode_get_abs_entity_index(
	c_bitstream* bitstream,
	int32* entity_index,
	uint32* entity_abs_index)
{
	*entity_abs_index = bitstream->read_integer("entity-absolute-index", 10);
	uint8 seed = (uint8)bitstream->read_integer("entity-seed", 4);

	*entity_index = *entity_abs_index | (seed << 28);
	return;
}
