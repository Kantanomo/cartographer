#pragma once

/* structures */

class c_path_result
{
public:
	bool m_valid;
	bool m_steps_finish_path;
	bool m_obstacle_on_goal;
	real_point3d m_start_point;
	int32 m_pad_16;
	int8 m_end_point[16];
	datum m_ignorable_object_index;
	uint16 m_ignorable_object_flags;
	int8 m_pad_42[3];
	uint8 m_step_count;
	uint8 m_step_index;
	int8 m_steps[112];
	datum m_obstacle_object_index;
	int16 m_obstacle_type;
	int16 m_error_info_error_stage;
	int16 m_error_info_num_thresholds;
	int8 m_error_info_failed_threshold[24];
};
ASSERT_STRUCT_SIZE(c_path_result, 196);
