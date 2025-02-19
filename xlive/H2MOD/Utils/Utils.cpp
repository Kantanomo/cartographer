#include "stdafx.h"
#include "Utils.h"

#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"

int FindLineStart(FILE* fp, int lineStrLen) {
	int fp_offset_orig = ftell(fp);
	for (int i = lineStrLen; i < 255; i++) {
		if (fp_offset_orig - i < 0) {
			fseek(fp, fp_offset_orig, SEEK_SET);
			return 0;
		}
		fseek(fp, fp_offset_orig - i, SEEK_SET);
		int c = 0;
		if ((c = fgetc(fp)) == '\r' || c == '\n') {
			fseek(fp, fp_offset_orig, SEEK_SET);
			return fp_offset_orig - i + 1;
		}
	}
	fseek(fp, fp_offset_orig, SEEK_SET);
	return fp_offset_orig - lineStrLen;
}

bool GetFileLine(FILE* fp, char** fileLine) {
	bool moreToGo = true;
	int c, fileLineLen = 256, fileLineCursor = 0;
	*fileLine = (char*)malloc(fileLineLen);
	while (c = fgetc(fp)) {
		if (c == EOF) {
			moreToGo = false;
			break;
		}
		else if (c == '\r') {

		}
		else if (c == '\n') {
			break;
		}
		else {
			(*fileLine)[fileLineCursor++] = c;
		}
		if (fileLineCursor >= fileLineLen - 2) {
			fileLineLen += 256;
			*fileLine = (char*)realloc(*fileLine, fileLineLen);
		}
	}
	(*fileLine)[fileLineCursor++] = 0;
	if (strlen(*fileLine) == 0) {
		free(*fileLine);
		*fileLine = 0;
	}
	return moreToGo || *fileLine != 0;
}

bool GetNTStringLine(char* text, int lineNum, char** line) {
	int inc_line_num = 0;
	char* line_begin = text;

	while (inc_line_num++ < lineNum) {
		line_begin = strchr(line_begin, '\n');
		if (line_begin++ == 0) {
			return false;
		}
	}

	size_t line_len = 0;
	char* line_next = strchr(line_begin, '\n');
	if (line_next == 0) {
		line_len = strlen(line_begin);
	}
	else {
		if (*(line_next - 1) == '\r') {
			line_next--;
		}
		line_len = line_next - line_begin;
	}

	if (line_len <= 0) {
		*line = NULL;
		return true;
	}

	*line = (char*)malloc(sizeof(char) * (line_len + 1));

	csmemcpy(*line, line_begin, line_len);
	(*line)[line_len] = 0;

	return true;
}

//if missing versioning parameters, major will always be on leftmost side.
char CmpVersions(const char* version_base, const char* version_alt) {

	if (strcmp(version_base, version_alt) == 0)
		return 0b00000;//same

	int versions[2][4];
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 4; i++) {
			versions[j][i] = 0;
		}
	}

	if (sscanf_s(version_base, "%d.%d.%d.%d", &versions[0][0], &versions[0][1], &versions[0][2], &versions[0][3]) <= 0) {
		return 0b10000;//invalid
	}

	if (sscanf_s(version_alt, "%d.%d.%d.%d", &versions[1][0], &versions[1][1], &versions[1][2], &versions[1][3]) <= 0) {
		return 0b10000;//invalid
	}

	for (int i = 0; i < 4; i++) {
		if (versions[0][i] == versions[1][i]) {
			continue;
		}
		else if (versions[0][i] > versions[1][i]) {//alt is old
			return 0b10000 | (0b1000 >> i);
		}
		else {//alt is new
			return 0b00000 | (0b1000 >> i);
		}
	}

	return 0b00000;//same
	//return 0b01000;//new major
	//return 0b00100;//new minor
	//return 0b00010;//new revision
	//return 0b00001;//new build
	//return 0b11000;//old major
	//return 0b10100;//old minor
	//return 0b10010;//old revision
	//return 0b10001;//old build
}

void ReadIniFile(void* fileConfig, bool configIsFILE, const char* header, const char* headerVersion, int(interpretSettingFunc)(char* fileLine, char* version, int lineNumber)) {
	bool foundFirstHeader = false;
	char version[30] = "0";
	bool keepReading = true;
	int lineNumber = 0;
	char* fileLine;
	while (keepReading && ((configIsFILE && GetFileLine((FILE*)fileConfig, &fileLine)) || (!configIsFILE && GetNTStringLine((char*)fileConfig, lineNumber, &fileLine)))) {
		lineNumber++;
		if (fileLine) {
			if (fileLine[0] == header[0] && sscanf_s(fileLine, header, &version, 30)) {
				foundFirstHeader = true;
				addDebugText("Found header on line %d asserting version: %s", lineNumber, version);
				if (CmpVersions(headerVersion, version) == 0) {//does not send this line to interpreter.
					free(fileLine);
					continue;
				}
				else {
					addDebugText("Incorrect version! Continue searching!");
				}
			}
			int rtnCode = interpretSettingFunc(fileLine, foundFirstHeader ? version : 0, lineNumber);
			if (!(rtnCode & 0b10)) {
				free(fileLine);
				if (rtnCode & 0b1)
					keepReading = false;
			}
		}
	}
}

std::string GetVKeyCodeString(int vkey) {
	std::ostringstream strStream;
	strStream << "0x" << std::hex << vkey;
	if (vkey >= 0x70 && vkey <= 0x87) {
		int func_num = vkey - 0x70 + 1;
		strStream << " - VK_F" << std::dec << func_num;
	}
	else if (vkey == 0x24) {
		strStream << " - VK_Home";
	}
	return strStream.str();
}

int GetWidePathFromFullWideFilename(const wchar_t* filepath, wchar_t* rtnpath) {
	const wchar_t* backslash_offset = wcsrchr(filepath, L'\\');
	const wchar_t* forward_slash_offset = wcsrchr(filepath, L'/');
	const wchar_t* most_right_slash_offset = 
		backslash_offset == NULL ? forward_slash_offset : ((forward_slash_offset != NULL && backslash_offset < forward_slash_offset) ? forward_slash_offset : backslash_offset);

	if (most_right_slash_offset != NULL) {
		wcsncpy(rtnpath, filepath, (most_right_slash_offset - filepath) + 1);
		return 0;
	}

	return -1;
}

LONG GetDWORDRegKey(HKEY hKey, const wchar_t* strValueName, DWORD* nValue) {
	DWORD dwBufferSize(sizeof(DWORD));
	DWORD nResult(0);
	LONG nError = ::RegQueryValueExW(hKey,
		strValueName,
		0,
		NULL,
		reinterpret_cast<LPBYTE>(&nResult),
		&dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		*nValue = nResult;
	}
	return nError;
}

char* custom_label_escape(char* label_literal) {
	int label_literal_length = strlen(label_literal);
	int length_shift = 0;
	int label_escaped_length = label_literal_length + length_shift + 2;
	char* label_escaped = (char*)malloc(label_escaped_length);

	for (int i = 0; i < label_literal_length; i++) {
		if (label_escaped_length < i + length_shift + 2) {
			label_escaped = (char*)realloc(label_escaped, label_literal_length + length_shift + 2);
		}
		if (label_literal[i] == '\\') {
			label_escaped[i + length_shift] = '\\';
			length_shift++;
			label_escaped[i + length_shift] = '\\';
		}
		else if (label_literal[i] == '\n') {
			label_escaped[i + length_shift] = '\\';
			length_shift++;
			label_escaped[i + length_shift] = 'n';
		}
		else if (label_literal[i] == '\r') {
			label_escaped[i + length_shift] = '\\';
			length_shift++;
			label_escaped[i + length_shift] = 'r';
		}
		else {
			label_escaped[i + length_shift] = label_literal[i];
		}
	}

	label_escaped[label_literal_length + length_shift] = 0;
	return label_escaped;
}

bool isInteger(std::wstring myString)
{
	return myString.find_first_not_of(L"0123456789") == std::wstring::npos;
}

bool isFloat(std::wstring myString)
{
	std::wistringstream iss(myString);
	float f;
	iss >> std::noskipws >> f; // noskipws considers leading whitespace invalid
	// Check the entire string was consumed and if either failbit or badbit is set
	return iss.eof() && !iss.fail();
}

DWORD crc32buf(const char* buf, size_t len)
{
	DWORD oldcrc32;

	oldcrc32 = 0xFFFFFFFF;

	for (; len; --len, ++buf)
	{
		oldcrc32 = UPDC32(*buf, oldcrc32);
	}

	return ~oldcrc32;
}

bool ComputeFileCrc32Hash(wchar_t* filepath, DWORD &rtncrc32) {

	DWORD oldcrc32;

	oldcrc32 = 0xFFFFFFFF;

	const int BUFSIZE = 1024;
	BOOL bResult = FALSE;
	HANDLE hFile = NULL;
	BYTE rgbFile[BUFSIZE];
	DWORD cbRead = 0;

	hFile = CreateFile(filepath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);

	if (INVALID_HANDLE_VALUE == hFile)
	{
		return false;
	}

	while (bResult = ReadFile(hFile, rgbFile, BUFSIZE, &cbRead, NULL))
	{
		if (cbRead == 0) {
			break;
		}

		for (unsigned int i = 0; i < cbRead; i++) {
			oldcrc32 = UPDC32(rgbFile[i], oldcrc32);
		}
	}

	CloseHandle(hFile);

	if (!bResult) {
		return false;
	}

	rtncrc32 = ~oldcrc32;

	return true;
}

static bool rfc3986_allow(char i) {
	//isalnum(i)//PoS crashes
	if ((i >= '0' && i <= '9') || 
		(i >= 'a' && i <= 'z') ||
		(i >= 'A' && i <= 'Z') ||
		i == '~' || i == '-' || i == '.' || i == '_')
		return true;
	return false;
}

char* encode_rfc3986(const char* label_literal, size_t label_literal_length) {
	if (label_literal_length == 0)
		label_literal_length = strlen(label_literal);
	int escaped_buflen = (label_literal_length * 3) + 1;
	char* label_escaped = (char*)malloc(escaped_buflen * sizeof(char));
	int escaped_buff_i = 0;

	for (size_t i = 0; i < label_literal_length; i++) {
		unsigned char uletter = label_literal[i];
		if (!rfc3986_allow(uletter)) {
			sprintf_s(label_escaped + escaped_buff_i, 4, "%%%02X", uletter);
			escaped_buff_i += 2;
		}
		else {
			label_escaped[escaped_buff_i] = label_literal[i];
		}
		escaped_buff_i++;
	}
	label_escaped[escaped_buff_i] = 0;
	return label_escaped;
}

struct stringMe {
	char *ptr;
	size_t len;
};

static void init_string(struct stringMe *s) {
	s->len = 0;
	s->ptr = (char*)malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct stringMe *s)
{
	size_t new_len = s->len + size*nmemb;
	s->ptr = (char*)realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	csmemcpy(s->ptr + s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmemb;
}

int MasterHttpResponse(const char* url, const char* http_request, char** rtn_response) {
	TEST_N_DEF(LC1);
	int result = ERROR_CODE_CURL_SOCKET_FAILED;//Socket failed to connect to server

	CURL *curl;
	CURLcode res;

	/* get a curl handle */
	curl = curl_interface_init_no_verify();
	if (curl) {
		/* First set the URL that is about to receive our POST. This URL can
		just as well be a https:// URL if that is what should receive the
		data. */
		curl_easy_setopt(curl, CURLOPT_URL, url);

		struct stringMe s;
		init_string(&s);

		//FIXME: <Insert Pinned Public Key Here>

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

		/* Now specify the POST data */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, http_request);

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK) {
			result = ERROR_CODE_CURL_EASY_PERF;//curl_easy_perform() issue
			addDebugText("curl_easy_perform() failed: %s", curl_easy_strerror(res));
			free(s.ptr);
		}
		else {
			*rtn_response = s.ptr;
			result = 0;//successful connection.
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	else {
		result = ERROR_CODE_CURL_HANDLE;//curl handle fail
	}

	return result;
}

void CreateDirTree(const wchar_t* path) {
	size_t buflen = wcslen(path) + 1;
	wchar_t* temp_path = (wchar_t*)malloc(sizeof(wchar_t) * buflen);
	wcscpy_s(temp_path, buflen, path);

	for (size_t i = 1; i < buflen; i++) {
		if (temp_path[i] == L'/' || temp_path[i] == L'\\') {
			wchar_t temp_cut = 0;
			if (temp_path[i + 1] != 0) {
				temp_cut = temp_path[i + 1];
				temp_path[i + 1] = 0;
			}
			CreateDirectoryW(temp_path, NULL);
			if (temp_cut) {
				temp_path[i + 1] = temp_cut;
			}
		}
	}

	free(temp_path);
}

// possible format: '0Xx01234556789ABCDEFabcdefHh'
bool HexStrToBytes(const char* hexStr, size_t hexStrLen, uint8_t* outByteBuf, size_t outBufLen)
{
	// ASCII character index map to hex value
	static const uint8_t lutStrToHex[23] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0,
		0, 0, 0, 10, 11, 12, 13, 14, 15
	};

	if (hexStr == NULL
		|| outByteBuf == NULL)
	{
		return false;
	}

	const uint8_t* byteBufEnd = outByteBuf + outBufLen;
	hexStrLen = hexStrLen != 0u ? hexStrLen : strlen(hexStr);

	// handle prefixes
	if (hexStrLen > 2
		&& hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X'))
	{
		hexStr += 2;
		hexStrLen -= 2;
	}

	// handle suffixes
	if (hexStrLen > 1
		&& (hexStr[hexStrLen - 1] == 'h' || (hexStr[hexStrLen - 1] == 'H')))
	{
		hexStrLen -= 1;
	}

	// continue only if string is at least 1
	if (!(hexStrLen >= 1u))
	{
		return false;
	}

	bool hexStrOddLen = (hexStrLen % 2) != 0;
	const char tmpBuf1[3] = { '0', hexStr[0], '\0' };
	const char* hexStrBeg = &hexStr[0];
	if (hexStrOddLen)
	{
		// if we're dealing with a odd hex string bytes number
		// just add the 0 in front
		hexStrLen += 1;
		hexStrBeg = tmpBuf1;
	}

	// check if we have enough byte buffer
	if (hexStrLen / 2 > outBufLen)
	{
		return false;
	}

	bool result_success = true;
	for (size_t i = 0; i < hexStrLen; i += 2)
	{
		int upper = toupper(hexStrBeg[i]);
		int lower = toupper(hexStrBeg[i + 1]);

		if (!isxdigit(upper) || !isxdigit(lower))
		{
			result_success = false;
			break;
		}

		*outByteBuf++ = (uint8_t)((lutStrToHex[upper - '0'] << 4) | (lutStrToHex[lower - '0']));
		hexStrBeg = hexStr; // get to the next byte, remember this might be on tmpBuf1 ptr if we're dealing with hexStrOddLen
	}

	return result_success;
}

// TODO: this function checks the input hexStr before converting it to bytes
// thus being slower if the input is validated
bool HexStrToBytes(const std::string& hexStr, uint8_t* byteBuf, size_t bufLen) {
	return HexStrToBytes(hexStr.c_str(), hexStr.length(), byteBuf, bufLen);
}

std::string ByteToHexStr(const uint8_t* buffer, size_t size) {
	std::stringstream str;
	str.fill('0');
	str.setf(std::ios_base::uppercase);
	str.setf(std::ios_base::hex, std::ios::basefield);
	for (size_t i = 0; i < size; i++) {
		str << std::setw(2) << (uint16_t)buffer[i];
	}
	return str.str();
}
