#include "stdafx.h"
#include "controllers.h"

/* public code */

e_controller_index first_controller(void)
{
	return _controller0;
}

e_controller_index next_controller(
	e_controller_index controller)
{
	e_controller_index result = k_no_controller;

	switch (controller)
	{
	case _controller0:
		result = _controller1;
		break;
	case _controller1:
		result = _controller2;
		break;
	case _controller2:
		result = _controller3;
		break;
	}

	return result;
}
