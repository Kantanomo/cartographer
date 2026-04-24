#pragma once

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

enum e_collision_leaf_flags
{
	_collision_leaf_contains_two_sided_bit = 0,
	NUMBER_OF_COLLISION_LEAF_FLAGS
};

/* structures */

struct bsp3d
{
	s_tag_block nodes;
	s_tag_block planes;
};

struct bsp2d
{
	s_tag_block nodes;
};

// max: MAXIMUM_VERTICES_PER_COLLISION_BSP
struct collision_vertex
{
	real_point3d point;
	uint16 first_edge_index;
	int16 sink;
};
ASSERT_STRUCT_SIZE(collision_vertex, 16);

// max: MAXIMUM_EDGES_PER_COLLISION_BSP
struct collision_edge
{
	uint16 vertex_indices[2];
	uint16 edge_indices[2];
	int16 surface_indices[2];
};
ASSERT_STRUCT_SIZE(collision_edge, 12);

// max: MAXIMUM_SURFACES_PER_COLLISION_BSP
struct collision_surface
{
	int16 plane_designator;
	uint16 first_edge_index;
	uint8 /*e_collision_surface_flags*/ flags;
	int8 breakable_surface_index;
	int16 material_index;
};
ASSERT_STRUCT_SIZE(collision_surface, 8);

// max: MAXIMUM_NODES_PER_BSP2D
struct bsp2d_node
{
	real_plane2d plane;
	int16 child_indices[2];
};
ASSERT_STRUCT_SIZE(bsp2d_node, 16);

// max: MAXIMUM_BSP2D_REFERENCES_PER_COLLISION_BSP
struct bsp2d_reference
{
	int16 plane_designator;
	int16 root_index;
};
ASSERT_STRUCT_SIZE(bsp2d_reference, 4);

// max: MAXIMUM_LEAVES_PER_BSP3D
struct collision_leaf
{
	uint8 /*e_collision_leaf_flags*/ flags;
	int8 bsp_2d_reference_count;
	uint16 first_bsp2d_reference_index;
};
ASSERT_STRUCT_SIZE(collision_leaf, 4);

// max: MAXIMUM_NODES_PER_BSP3D
struct bsp3d_node
{
	int64 plane_index : 16;
	int64 below_child_index : 24;
	int64 above_child_index : 24;
};
ASSERT_STRUCT_SIZE(bsp3d_node, 8);

struct collision_bsp
{
	bsp3d bsp3d;

	s_tag_block leaves;
	s_tag_block bsp_2d_references;
	
	bsp2d bsp2d;

	s_tag_block surfaces;
	s_tag_block edges;
	s_tag_block vertices;
};
ASSERT_STRUCT_SIZE(collision_bsp, 64);
