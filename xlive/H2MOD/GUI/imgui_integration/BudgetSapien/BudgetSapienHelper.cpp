#include "BudgetSapienHelper.h"
#include "H2MOD/Tags/TagInterface.h"


namespace BudgetSapienHelper
{
	std::vector<s_tag_tree_node> currentTreeNodes;
	s_tag_tree_node currentTag = s_tag_tree_node();
	void GenerateTreeNodesTagView()
	{
		currentTreeNodes.clear();
		auto nameMap = tags::get_tag_datum_name_map();
		for(auto &it : nameMap)
		{
			tags::tag_instance c_tag = tags::get_tag_instances()[it.first];
			auto treeIt = std::find_if(
				currentTreeNodes.begin(), 
				currentTreeNodes.end(),
				[&c_tag](const s_tag_tree_node &node)
				{
					return c_tag.type.as_string() == node.Name;
				}
			);
			if(treeIt != currentTreeNodes.end())
			{
				treeIt->Children.emplace_back(e_tag_tree_node_type::e_tag, it.second, c_tag);
			}
			else
			{
				currentTreeNodes.emplace_back(e_tag_tree_node_type::e_folder, c_tag.type.as_string());
				treeIt = std::find_if(
					currentTreeNodes.begin(),
					currentTreeNodes.end(),
					[&c_tag](const s_tag_tree_node &node)
				{
					return c_tag.type.as_string() == node.Name;
				}
				);
				treeIt->Children.emplace_back(e_tag_tree_node_type::e_tag, it.second, c_tag);
			}
		}
		return;
	}

	void SelectTag(s_tag_tree_node node)
	{
		currentTag = node;
	}

	s_tag_tree_node GetSelectedTag()
	{
		return currentTag;
	}

	std::vector<s_tag_tree_node> getCurrentTreeNodes()
	{
		return currentTreeNodes;
	}

}
