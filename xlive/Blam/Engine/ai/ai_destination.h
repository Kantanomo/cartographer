#pragma once

/* structures */

struct s_destination_orders_raw
{
	real_point3d point;
	int16 reference_frame;
	int16 bsp_index;
	int32 sector_ref;
};
ASSERT_STRUCT_SIZE(s_destination_orders_raw, 20);

union s_destination_orders_data
{
	datum prop_index;
	s_destination_orders_raw raw;
};
ASSERT_STRUCT_SIZE(s_destination_orders_data, 20);

class c_destination_orders
{
public:
	int16 m_destination_type;
	uint8 m_destination_flags;
	real32 m_scripted_speed;
	real32 m_scripted_goal_throttle;
	s_destination_orders_data m_destination;
	real32 m_destination_tolerance;
	real32 m_raw_destination_tolerance;
	uint32 m_destination_facing_flags;
	real_vector3d m_destination_facing;
	int32 m_destination_action[8];
};
ASSERT_STRUCT_SIZE(c_destination_orders, 88);
