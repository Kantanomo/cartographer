#include "stdafx.h"
#include "simulation_gamestate_entities.h"

#include "simulation/simulation_entity_database.h"

void simulation_gamestate_index_encode(c_bitstream* bitstream, datum object_index)
{
	ASSERT(bitstream);
	//ASSERT(gamestate_index != NONE)

	bitstream->write_integer("gamestate-index-id", DATUM_INDEX_TO_IDENTIFIER(object_index), 16);
	bitstream->write_integer("gamestate-index-absolute", DATUM_INDEX_TO_ABSOLUTE_INDEX(object_index), 11);
	return;
}

void simulation_gamestate_index_decode(c_bitstream* bitstream, datum* object_index)
{
	ASSERT(bitstream);
	//ASSERT(gamestate_index_out);

	int32 object_index_id = bitstream->read_integer("gamestate-index-id", 16);
	int32 object_absolute_index = bitstream->read_integer("gamestate-index-absolute", 11);

	*object_index = DATUM_INDEX_NEW(object_absolute_index, object_index_id);
	return;
}

void simulation_entity_index_encode(c_bitstream* stream, int32 entity_index)
{
	stream->write_integer("entity-absolute-index", ENTITY_INDEX_TO_ABSOLUTE_INDEX(entity_index), 10);
	stream->write_integer("entity-seed", ENTITY_INDEX_TO_SEED(entity_index), 4);
	return;
}

void simulation_entity_index_decode(c_bitstream* stream, int32* entity_index)
{
	int32 entity_abs_index = stream->read_integer("entity-absolute-index", 10);
	int32 entity_seed = stream->read_integer("entity-seed", 4);

	*entity_index = (entity_seed << 28) | (entity_abs_index & (k_simulation_entity_database_maximum_entities - 1));
	return;
}