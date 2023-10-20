#include "stdafx.h"
#include "KantTesting.h"
#include <H2MOD/Modules/Shell/Startup/Startup.h>
#include "Blam/Engine/cartographer/settings/settings.h"


namespace KantTesting
{
	void MapLoad()
	{
	}
	
	template<typename T>
	void tttt(T s)
	{
		Document doc_;
		static_assert(std::is_base_of<c_base_easy_json_struct, T>::value,
			"T must be a derived class of Base in Context<T>.");
		s.load(doc_);
	}
	
	void Initialize()
	{
		
		//easy_json_struct test(L"banana.json", &cartographer_settings);
		//test.load();
		
		//test.save();
		//tttt(a);
		if (ENABLEKANTTEST) {
		//	if (!Memory::isDedicatedServer())
			//{
			//tags::on_map_load(MapLoad);
		//	}
		}
	}
}


