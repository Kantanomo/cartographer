#pragma once
namespace PlaylistLoader
{
	enum e_playlist_reader_seek_mode
	{
		newLine = 0x0,
		headerStart = 0x2,
		headerRead = 0x3,
		propertyNameRead = 0x4,
		propertyDeliminatorScan = 0x5,
		propertyValueRead = 0x6,
		seekToNextLine = 0x7,
	};
	struct __declspec(align(4)) VariantMatch
	{
		DWORD unk;
		wchar_t variant_name[16];
		DWORD Data[302];
	};
	struct __declspec(align(4)) VariantMatches
	{
		DWORD Shuffle;
		DWORD Pregame_Team_Selection_Delay;
		DWORD Pregame_Delay;
		DWORD Postgame_Delay;
		VariantMatch Matches[100];
	};
	struct playlist_match
	{
		DWORD unk;
		wchar_t Map[16];
		DWORD data1[9];
		wchar_t Variant[17];
		DWORD unk1;
		DWORD Weight;
		__int16 MinimumPlayers;
		__int16 MaximumPlayers;
	};

	struct setting_buffer_item
	{
		wchar_t Name[16];
		wchar_t Value[16];
		__int16 unk1;
		__int16 unk2;
	};
	static_assert(sizeof(setting_buffer_item) == 0x44, "Size Incorrect");

	struct playlist_entry
	{
		VariantMatches *VariantMatches;
		playlist_match PlaylistMatches[100];
		DWORD MatchCount;
		//setting_buffer_item section_buffer[224];
		wchar_t section_buffer[7616];
		DWORD section_buffer_current_index;
		wchar_t header_buffer[32];
		DWORD unk;
		DWORD current_section_type;
		e_playlist_reader_seek_mode reader_current_mode;
		DWORD reader_current_char_index;
		DWORD reader_current_line;
		DWORD data3[24407];
	};
	static_assert(sizeof(playlist_entry) == 0x1E81C, "Size incorrect");
	void ApplyHooks();
	void Initialize();
}