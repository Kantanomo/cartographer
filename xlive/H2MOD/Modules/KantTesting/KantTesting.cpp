#include "KantTesting.h"
#include "Blam\Cache\DataTypes\BlamPrimitiveType.h"
#include "Blam\Cache\TagGroups\biped_definition.hpp"
#include "Blam\Cache\TagGroups\globals_definition.hpp"
#include "Blam\Cache\TagGroups\model_definition.hpp"
#include "Blam\Cache\TagGroups\scenario_definition.hpp"
#include "Blam\Cache\TagGroups\scenario_lightmap_definition.hpp"
#include "Blam\Cache\TagGroups\scenario_structure_bsp_definition.hpp"
#include "Blam\Cache\TagGroups\weapon_definition.hpp"
#include "Blam\Engine\Game\GameEngineGlobals.h"
#include "Blam\Engine\Game\GameGlobals.h"
#include "Blam\Engine\Players\Players.h"
#include "Blam\LazyBlam\LazyBlam.hpp"
#include "H2MOD\EngineCalls\EngineCalls.h"
#include "H2MOD\Modules\Config\Config.h"
#include "H2MOD\Modules\Console\ConsoleCommands.h"
#include "H2MOD\Modules\EventHandler\EventHandler.hpp"
#include "H2MOD\Modules\Networking\CustomPackets\CustomPackets.h"
#include "H2MOD\Modules\Networking\Memory\bitstream.h"
#include "H2MOD\Modules\PlayerRepresentation\PlayerRepresentation.h"
#include "H2MOD\Tags\MetaExtender.h"
#include "H2MOD\Tags\MetaLoader\tag_loader.h"
#include "Util\Hooks\Hook.h"


namespace KantTesting
{
	typedef void(__cdecl t_asdf)(datum a1, real_vector3d* a2, real_vector3d* a3, real_vector3d* a4);
	t_asdf* p_asdf;

	void __cdecl asdf(datum a1, real_vector3d* a2, real_vector3d* a3, real_vector3d* a4)
	{
		LOG_INFO_GAME("[{}] {:x}", __FUNCTION__, a1);
		p_asdf(a1, a2, a3, a4);
	}
	void Initialize()
	{
		if (ENABLEKANTTEST) {
			if (!Memory::isDedicatedServer()) {

				p_asdf = (t_asdf*)DetourFunc(Memory::GetAddress<BYTE*>(0x186355), (BYTE*)asdf, 6);
				//tags::on_map_load(mapLoad);
			/*	register_player_packets_method = (register_player_packets)DetourFunc(Memory::GetAddress<BYTE*>(0x1F0A55, 0x1D140E), (BYTE*)registerPlayerPackets, 5);

				p_encode_player_add = Memory::GetAddress<t_encode_player_add*>(0x1F06B6);
				p_decode_player_add = Memory::GetAddress<t_decode_player_add*>(0x1F0752);
				p_encode_player_properties = Memory::GetAddress<t_encode_player_properties*>(0x1F0935);
				p_decode_player_properties = Memory::GetAddress<t_decode_player_properties*>(0x1F09AC);

				EventHandler::register_callback<EventHandler::GameLoopEvent>(testApply);
				p_object_new = Memory::GetAddress<t_object_new*>(0x136CA7);
				PatchCall(Memory::GetAddress(0x55C06), object_new_impl);*/

				//imp_object_placement_data = Memory::GetAddress<object_placement_new_def*>(0x132163);
				//PatchCall(Memory::GetAddress(0x55B43), object_placement_new);
				//NopFill(Memory::GetAddress(0x55B5A), 7);



				//p_get_model_variant = Memory::GetAddress<t_get_model_variant*>(0x12FE84);
				//PatchCall(Memory::GetAddress(0x12FEF1), get_model_variant);
				//tags::on_map_load(fix_elite_model_variant);
				//tags::on_map_load(add_elite_variants);

				
				//Stop the game from overriding the player biped
				//NopFill(Memory::GetAddress(0x52fc5), 3);
				//Stop  the game from overriding the player biped
				//NopFill(Memory::GetAddress(0x52fF5), 3);
				//PatchCall(Memory::GetAddress(0x5509E), network_ession_player_profile_recieve);
				//tags::on_map_load(player_representation_testing);
			}
		}
	}
}
