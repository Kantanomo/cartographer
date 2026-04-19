#pragma once
#include "user_interface_widget.h"
#include "saved_games/player_profile.h"


/* classes */

class c_model_widget : public c_user_interface_widget
{
protected:
	struct s_ui_model_scene_reference* m_tag_block;

public:
	void apply_appearance_and_character(struct s_player_appearance* appearance, e_character_type character);

	// c_model_widget virtual functions

	virtual ~c_model_widget(void) = default;
	virtual void setup_children(void) override;
	virtual void render_widget(union rectangle2d* viewport_bounds) override;
	virtual int32 get_intro_delay(void) override;
	virtual class c_user_interface_text* get_interface() override;
};
ASSERT_STRUCT_SIZE(c_model_widget, 0x74);