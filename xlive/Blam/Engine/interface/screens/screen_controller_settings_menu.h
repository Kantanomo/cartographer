#pragma once

class c_list_item_widget;

class c_controller_settings_edit_list // : public c_list_widget
{
public:
	void update_list_items(c_list_item_widget* item, int32 skin_index);
	static void apply_instance_patches();
};
