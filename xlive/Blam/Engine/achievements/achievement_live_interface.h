#pragma once
#include "achievement_list.h"

/* classes */

class c_achievement_live_interface
{
public:
	void process_live_signin_state(bool signed_in);

private:
	bool m_queue_achievement[k_achievement_type_count];
	bool m_upload_achievements;
	uint32 m_upload_start_time;
	bool m_upload_in_progress;
	bool m_achievement_uploading[k_achievement_type_count];
	_XUSER_ACHIEVEMENT m_user_achievement[k_achievement_type_count];
	XOVERLAPPED m_overlapped;
};
