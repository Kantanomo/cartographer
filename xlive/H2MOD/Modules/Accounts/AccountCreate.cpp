#include "stdafx.h"
#include "AccountCreate.h"

#include "cartographer/config/endpoints.h"
#include "H2MOD/Modules/OnScreenDebug/OnscreenDebug.h"
#include "H2MOD/Utils/Utils.h"

#define ERROR_CODE_INVALID_PARAM -1
#define ERROR_CODE_INVALID_EMAIL -2
#define ERROR_CODE_INVALID_USERNAME -3
#define ERROR_CODE_INVALID_PASSWORD -4
#define ERROR_CODE_TAKEN_EMAIL -5
#define ERROR_CODE_TAKEN_USERNAME -6
#define ERROR_CODE_BANNED_EMAIL_DOMAIN -7

const char k_user_name_string[] = "username=";

const char k_cartographer_create_url[] = k_cartographer_url_https"/create1";

// TODO (Carefully) Cleanup and move

static int InterpretMasterCreate(char* response_content) {
	int result = 0;//will stay as 0 when master only returns "return_code=xxx<br>"

	char* index_next = NULL;
	char* index_current = response_content;
	while ((index_next = strstr(index_current, "<br>")))
	{
		char* fileLine = (char*)malloc((index_next - index_current + 1) * sizeof(char));
		memcpy(fileLine, index_current, index_next - index_current);
		fileLine[index_next - index_current] = 0;

		int tempint1;
		if (sscanf_s(fileLine, "return_code=%d", &tempint1) == 1) 
		{
			addDebugText("Return code is: %d", tempint1);
			result = tempint1;
		}
		else if (strstr(fileLine, k_user_name_string))
		{
			char* tempName = fileLine + NUMBEROF(k_user_name_string) - 1;
			while (isspace(*tempName))
			{
				tempName++;
			}

			char tempstr1[512] = { "" };
			snprintf(tempstr1, 32, tempName);
			for (int j = strlen(tempstr1) - 1; j > 0; j--)
			{
				if (isspace(tempstr1[j]))
				{
					tempstr1[j] = 0;
				}
				else
				{
					break;
				}
			}
		}

		free(fileLine);
		index_current = index_next + 4;
	}

	return result;
}


bool HandleGuiAccountCreate(char* username, char* email, char* password, e_cartographer_error_id* out_account_create_cartographer_error_id) {
	bool result = false;
	char* rtn_result = 0;

	char http_request_body[] = "username=%s&email=%s&password=%s&request_version=%s";
	char http_request_body_build[1024] = { "" };

	char* escaped_user_username = encode_rfc3986(username);
	char* escaped_user_email = encode_rfc3986(email);
	char* escaped_user_password = encode_rfc3986(password);
	sprintf_s(http_request_body_build, http_request_body, escaped_user_username, escaped_user_email, escaped_user_password, DLL_VERSION_STR);
	free(escaped_user_username);
	free(escaped_user_email);
	free(escaped_user_password);

#ifndef LC4
	int rtn_code = MasterHttpResponse(k_cartographer_create_url, http_request_body_build, &rtn_result);
#else
	TEST_N_DEF(LC4);
#endif
	
	if (rtn_code == 0)
	{
		rtn_code = InterpretMasterCreate(rtn_result);
		result = rtn_code > 0 ? true : result;
		free(rtn_result);
	}
	
	if (rtn_code <= 0)
	{
		addDebugText("ERROR Account Create: %d", rtn_code);
		// ### TODO FIXME move this handling trash out of here
		if (rtn_code == 0 
			|| rtn_code == ERROR_CODE_CURL_SOCKET_FAILED 
			|| rtn_code == ERROR_CODE_CURL_HANDLE 
			|| rtn_code == ERROR_CODE_CURL_EASY_PERF
			|| rtn_code == ERROR_CODE_INVALID_PARAM) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_internal_error;
		}
		else if (rtn_code == ERROR_CODE_INVALID_EMAIL) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_invalid_email;
		}
		else if (rtn_code == ERROR_CODE_INVALID_USERNAME) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_invalid_username;
		}
		else if (rtn_code == ERROR_CODE_INVALID_PASSWORD) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_invalid_password;
		}
		else if (rtn_code == ERROR_CODE_TAKEN_EMAIL) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_email_already_used;
		}
		else if (rtn_code == ERROR_CODE_TAKEN_USERNAME) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_username_taken;
		}
		else if (rtn_code == ERROR_CODE_BANNED_EMAIL_DOMAIN) {
			*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_blacklisted_email_provider;
		}
		else {
			//unknown error!
			*out_account_create_cartographer_error_id = _cartographer_error_id_unknown_unhandled_error;
		}
	}
	else {
		*out_account_create_cartographer_error_id = _cartographer_error_id_account_create_success;
	}
	return result;
}
