#pragma once

enum e_controller_index : int32
{
	_controller_index_0 = 0,
	_controller_index_1,
	_controller_index_2,
	_controller_index_3,
	k_number_of_controllers,
	k_no_controller = NONE
};

e_controller_index first_controller();
e_controller_index next_controller(e_controller_index controller);