#pragma once

int FindLineStart(FILE* fp, int lineStrLen);
///FREE MEMOERY in fileLine
bool GetFileLine(FILE* fp, char** fileLine);
char CmpVersions(const char* version_base, const char* version_alt);
void ReadIniFile(void* fileConfig, bool configIsFILE, const char* header, const char* headerVersion, int(interpretSettingFunc)(char* fileLine, char* version, int lineNumber));
void GetVKeyCodeString(int vkey, c_static_string<64>* string);
int GetWidePathFromFullWideFilename(const wchar_t* filepath, wchar_t* rtnpath);

///FREE MEMOERY in returned char*
char* custom_label_escape(char* label_literal);

bool isInteger(std::wstring myString);

bool HexStrToBytes(const std::string& hexStr, uint8_t* byteBuf, size_t bufLen);
std::string ByteToHexStr(const uint8_t* buffer, size_t size);

char* encode_rfc3986(const char* label_literal, size_t label_literal_length = 0u);

///FREE MEMOERY in rtn_response if returned 0 (success).
int MasterHttpResponse(const char* url, const char* http_request, char** rtn_response);

void CreateDirTree(const wchar_t* path);

inline std::string str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return std::tolower(c); }
	);
	return s;
}

DWORD crc32buf(const char* buf, size_t len);
bool ComputeFileCrc32Hash(wchar_t* filepath, DWORD& rtncrc32);
