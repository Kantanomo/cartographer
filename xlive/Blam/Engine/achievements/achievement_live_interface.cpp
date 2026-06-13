#include "stdafx.h"
#include "achievement_live_interface.h"

/* public code */

void c_achievement_live_interface::process_live_signin_state(
	bool signed_in)
{
	error(_error_delayed, "Achievements: process_live_signin_state: %d", signed_in);
	csmemset(m_queue_achievement, false, sizeof(m_queue_achievement));

	m_upload_achievements = false;
	
	if (!m_upload_in_progress && m_overlapped.InternalLow == ERROR_IO_PENDING)
	{
		error(_error_delayed, "Achievements: cancel upload");
		XCancelOverlapped(&m_overlapped);
	}

	csmemset(m_achievement_uploading, 0, sizeof(m_achievement_uploading));
	
	m_upload_in_progress = false;

	csmemset(&m_overlapped, 0, sizeof(m_overlapped));

	return;
}
