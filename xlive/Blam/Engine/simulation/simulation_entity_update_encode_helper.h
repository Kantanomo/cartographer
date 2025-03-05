#pragma once
#include "memory/bitstream.h"

class c_entity_update_encode_helper
{
	c_bitstream* m_bitstream;
	uint32 m_update_component_first_index;
	uint32 m_update_component_count;
	uint32 m_update_mask;
	uint32 m_update_considered_mask;
	uint32 m_update_written_mask;
	uint32 m_update_skipped_mask;
	uint32 m_update_overflowed_mask;
	bool m_able_to_write_update;
	uint32 m_current_update_component_index;
	const char* m_block_name;
	uint32 m_required_leave_space_bits;
	uint32 m_current_leave_space_bits;

	bool new_instance(c_bitstream* bitstream, uint32 required_leave_space_bits, uint32 update_component_first_index, uint32 update_component_count, uint32 update_mask);
	bool set_component_flag(uint32 component_index, const char* block_name);
	void commit();
	void set_update_mask(uint32* out_mask) const;
};
