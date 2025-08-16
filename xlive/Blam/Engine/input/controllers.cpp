#include "stdafx.h"
#include "controllers.h"


e_controller_index first_controller()
{
	return _controller_index_0;
}

e_controller_index next_controller(e_controller_index controller)
{
	switch (controller)
	{
	case _controller_index_0:
		return _controller_index_1;
	case _controller_index_1:
		return _controller_index_2;
	case _controller_index_2:
		return _controller_index_3;
	}
	return k_no_controller;
}