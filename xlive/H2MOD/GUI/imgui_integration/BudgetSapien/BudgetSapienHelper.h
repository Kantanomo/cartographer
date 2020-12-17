#pragma once
#include "H2MOD/Tags/TagInterface.h"

namespace BudgetSapienHelper
{
	enum e_tag_tree_node_type
	{
		e_tag,
		e_folder,
	};
	struct s_tag_tree_node
	{
		e_tag_tree_node_type Type;
		std::string Name;
		tags::tag_instance Instance;
		std::vector<s_tag_tree_node> Children;
		s_tag_tree_node()
		{
			
		}
		s_tag_tree_node(e_tag_tree_node_type type, std::string name)
		{
			Type = type;
			Name = name;
			Instance = tags::tag_instance();
		}
		s_tag_tree_node(e_tag_tree_node_type type, std::string name, tags::tag_instance instance)
		{
			Type = type;
			Name = name;
			Instance = instance;
		}
	};
	void GenerateTreeNodesTagView();
	void SelectTag(s_tag_tree_node node);
	s_tag_tree_node GetSelectedTag();
	std::vector<s_tag_tree_node> getCurrentTreeNodes();
}
