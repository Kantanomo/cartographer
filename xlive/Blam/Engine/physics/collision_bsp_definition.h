#pragma once
#include "tag_files/tag_block.h"

/* constants */

enum
{
	MAXIMUM_PLANES_PER_BSP3D = 65536,
	MAXIMUM_LEAVES_PER_BSP3D = 65536,
	MAXIMUM_BSP2D_REFERENCES_PER_COLLISION_BSP = 131072,
	MAXIMUM_SURFACES_PER_COLLISION_BSP = 131072,
	MAXIMUM_EDGES_PER_COLLISION_BSP = 262144,
	MAXIMUM_VERTICES_PER_COLLISION_BSP = 131072
};

/* enums */

enum e_collision_surface_flags
{
	_collision_surface_two_sided_bit = 0,
	_collision_surface_invisible_bit,
	_collision_surface_climbable_bit,
	_collision_surface_breakable_bit,
	_collision_surface_invalid_bit,
	_collision_surface_conveyor_bit,
	NUMBER_OF_COLLISION_SURFACE_FLAGS
};

enum e_collision_leaf_flags : uint8
{
	_collision_leaf_contains_double_sided_surfaces = FLAG(0)
};

/* structures */

struct bsp3d
{
	s_tag_block nodes;
	s_tag_block planes;
};

// max: MAXIMUM_VERTICES_PER_COLLISION_BSP
struct collision_vertex
{
	real_point3d point;
	uint16 first_edge;
	int16 pad;
};
ASSERT_STRUCT_SIZE(collision_vertex, 16);

// max: MAXIMUM_EDGES_PER_COLLISION_BSP
struct collision_edge
{
	uint16 start_vertex;
	uint16 end_vertex;
	uint16 forward_edge;
	uint16 reverse_edge;
	uint16 left_surface;
	uint16 right_surface;
};
ASSERT_STRUCT_SIZE(collision_edge, 12);

// max: MAXIMUM_SURFACES_PER_COLLISION_BSP
struct collision_surface
{
	uint16 plane;
	uint16 first_edge;
	uint8 /*e_collision_surface_flags*/ flags;
	int8 breakable_surface;
	uint16 material;
};
ASSERT_STRUCT_SIZE(collision_surface, 8);

// max: MAXIMUM_NODES_PER_BSP2D
struct bsp2d_node
{
	real_plane2d plane;
	uint16 left_child;
	uint16 right_child;
};
ASSERT_STRUCT_SIZE(bsp2d_node, 16);

// max: MAXIMUM_BSP2D_REFERENCES_PER_COLLISION_BSP
struct bsp2d_reference
{
	uint16 plane;
	uint16 bsp_2d_node;
};
ASSERT_STRUCT_SIZE(bsp2d_reference, 4);

// max: MAXIMUM_LEAVES_PER_BSP3D
struct collision_leaf
{
	e_collision_leaf_flags flags;
	int8 bsp_2d_reference_count;
	uint16 first_bsp_2d_reference;
};
ASSERT_STRUCT_SIZE(collision_leaf, 4);

// max: MAXIMUM_NODES_PER_BSP3D
struct bsp3d_node
{
	int8 data[8];
};
ASSERT_STRUCT_SIZE(bsp3d_node, 8);

struct collision_bsp
{
	bsp3d bsp3d;

	tag_block<collision_leaf> leaves;
	tag_block<bsp2d_reference> bsp_2d_references;
	tag_block<bsp2d_node> bsp_2d_nodes;
	tag_block<collision_surface> surfaces;
	tag_block<collision_edge> edges;
	tag_block<collision_vertex> vertices;
};
ASSERT_STRUCT_SIZE(collision_bsp, 64);