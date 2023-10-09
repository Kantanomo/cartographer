#include "stdafx.h"
#include "KantTesting.h"
#include <H2MOD/Modules/Shell/Startup/Startup.h>
#include <Blam/Cartographer/Settings/Setting.h>



namespace KantTesting
{
	void MapLoad()
	{
	}
	
	template<typename T>
	void tttt(T s)
	{
		Document doc_;
		static_assert(std::is_base_of<s_base_easy_json_struct, T>::value,
			"T must be a derived class of Base in Context<T>.");
		s.load(doc_);
	}
	
	void Initialize()
	{
		s_cartographer_settings a;
		easy_json_struct<s_cartographer_settings> b(L"banana.json", &a);
		b.load();
		
		b.save();
		//tttt(a);
		if (ENABLEKANTTEST) {
		//	if (!Memory::isDedicatedServer())
			//{
			//tags::on_map_load(MapLoad);
		//	}
		}
	}
}


