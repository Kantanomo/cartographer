#pragma once

/* constants */

enum
{
	MAXIMUM_REGIONS_PER_MODEL = 16,
	MAXIMUM_PERMUTATIONS_PER_MODEL_REGION = 32,
	MAXIMUM_STATES_PER_MODEL_PERMUTATION = 10,
	MAXIMUM_MATERIALS_PER_MODEL = 32,
	MAXIMUM_DAMAGE_SECTIONS_PER_MODEL = 16,
	MAXIMUM_RESPONSES_PER_DAMAGE_SECTION = 16,
	MAXIMUM_DAMAGE_SEAT_INFOS_PER_MODEL = 16,
	MAXIMUM_DAMAGE_CONSTRAINT_INFOS_PER_MODEL = 16,
	MAXIMUM_MODEL_TARGETS_PER_MODEL = 32,
};

enum
{
    _model_state_default = 0,
    _model_state_minor_damage,
    _model_state_medium_damage,
    _model_state_major_damage,
    _model_state_destroyed,
    k_number_of_model_states,

    k_maximum_number_of_model_states = 12
};
