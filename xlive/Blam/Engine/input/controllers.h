#pragma once

/* enum */

enum e_controller_index
{
	_controller0 = 0,
	_controller1,
	_controller2,
	_controller3,
	k_number_of_controllers,
	k_no_controller = NONE,
	k_any_controller = 255
};

/* prototypes */

e_controller_index first_controller(void);

e_controller_index next_controller(e_controller_index controller);
