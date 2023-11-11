#pragma once
#include "stdafx.h"
#include "easy_json_writer.h"

#include "Blam/Engine/cseries/cseries_strings.h"
#include "Blam/Math/real_math.h"
#include "H2MOD/Utils/Utils.h"

#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/pointer.h"
#include "rapidjson/error/en.h"

#define k_max_json_depth 5
#define k_max_path_item_length 15

using namespace rapidjson;
#define easy_json_log(fmt, ...) \
	if (log_function != nullptr) \
		log_function(fmt, ##__VA_ARGS__)

#define easy_json_logw(fmt, ...) \
	if (log_w_function != nullptr) \
		log_w_function(fmt, ##__VA_ARGS__)




enum e_easy_json_error
{
	_easy_json_error_success,
	_easy_json_error_file_open,
	_easy_json_error_parse_failed
};

template<typename struct_type>
class c_easy_json {
private:
	c_static_wchar_string260 m_file_name;
	Document m_doc;
	int m_current_depth = 0;
	char m_key_path[k_max_json_depth][k_max_path_item_length];
	struct_type* m_object;
	struct_type m_backup_object;

	static char* trim_tabs_from_str(const char* input) {
		size_t length = strlen(input);
		char* output = (char*)calloc(length + 1, sizeof(char)); // +1 for null terminator
		size_t j = 0;

		for (size_t i = 0; i < length; ++i) {
			if (input[i] != '\t') {
				output[j++] = input[i];
			}
		}

		output[j] = '\0'; // Null-terminate the output string
		return output;
	}
public:
	typedef void t_log_function(const char* format, ...);
	typedef void t_log_w_function(const wchar_t* format, ...);
	t_log_function* log_function;
	t_log_w_function* log_w_function;
	e_easy_json_error error_code;
	char error_message[1024];

	c_easy_json(const wchar_t* file_name_in, struct_type* object_pointer)
	{
		m_file_name.set(file_name_in);
		m_object = object_pointer;
		m_doc.SetObject();
		log_function = nullptr;
		log_w_function = nullptr;
	}

	e_easy_json_error load() {
		easy_json_logw(L"[easy_json] attempting to load file \"%ws\"", m_file_name.get_string());
		std::ifstream ifs(m_file_name.get_string());
		if (!ifs) {
			easy_json_log("[easy_json] file does not exist, creating new file");
			// The file does not exist, so create it.
			std::ofstream ofs(m_file_name.get_string());
			if (!ofs) {
				easy_json_log("[easy_json] failed to create file");
				throw std::runtime_error("Failed to create file");
			}
			ofs << "{}";  // Initialize with an empty JSON object.
			ofs.close();
			ifs.open(m_file_name.get_string());
		}
		IStreamWrapper isw(ifs);
		m_doc.SetObject();
		ParseResult parse_result = m_doc.ParseStream(isw);
		if (m_doc.HasParseError())
		{
			ParseErrorCode errorCode = parse_result.Code();
			size_t errorOffset = parse_result.Offset();
			easy_json_log("[easy_json] json parsing error code: %d at offset %d", errorCode, errorOffset);
			
			size_t currentPosition = ifs.tellg();
			size_t lineStart = 0;
			size_t lineCount = 1;
			ifs.seekg(0);
			std::string line;
			char* trimmed_line;
			while (std::getline(ifs, line))
			{
				if (lineStart <= errorOffset && errorOffset <= lineStart + line.length())
				{
					trimmed_line = trim_tabs_from_str(line.c_str());
					easy_json_log("[easy_json] offending line in the json file: %s", trimmed_line);
					easy_json_log("[easy_json] error message: %s", GetParseError_En(errorCode));
					break;
				}
				lineStart += line.length() + 1;  // +1 for the newline character
				lineCount++;
			}
			size_t oFileBufSize = m_file_name.length() + 1 * sizeof(char);
			char* oFile = (char*)malloc(oFileBufSize);
			wcstombs2(m_file_name.get_buffer(), oFile, oFileBufSize);

			error_code = _easy_json_error_parse_failed;
			sprintf(error_message, "Failed to load file:\n\t%s\n\nError reason:\n\t%s\n\nFailed to parse line:\n\t%s", oFile, GetParseError_En(errorCode), trimmed_line);
			free(trimmed_line);
			free(oFile);
			return _easy_json_error_parse_failed;
		}

		m_object->load(*this);
		//Store a copy of the loaded object for data-integrity when saving if an error occurs.
		m_backup_object = struct_type(*m_object);
		return _easy_json_error_success;
	}

	e_easy_json_error save(bool save_object = true) {

		easy_json_logw(L"[easy_json] attempting to save file \"%ws\"", m_file_name.get_string());
		std::ofstream ofs(m_file_name.get_string());
		if (!ofs.is_open()) {
			easy_json_log("[easy_json] json file could not be opened, perhaps it is open in another program or you do not have permissions to write to the location.");
			return _easy_json_error_file_open;
		}

		OStreamWrapper osw(ofs);
		easy_json_writer writer(osw);
		writer.SetIndent('\t', 1);  // Set indentation to use tabs.

		if (save_object)
			m_object->save(*this);

		// Serialize the document and handle any errors during serialization.
		if (!m_doc.Accept(writer)) 
		{
			easy_json_log("[easy_json] json writing failed on key: \"%s\" with value \"%s\"", writer.lastKey, writer.lastValue);
			easy_json_log("[easy_json] attempting to resave the config without the failed key...");
			easy_json_log("[easy_json] key path: %s", writer.path);
			for (const auto& token : writer.get_path_vector())
				operator[](token.c_str());

			Value* member = get_current_pointer();
			clear_key_path();
			if(member != nullptr)
			{
				if (member->HasMember(writer.lastKey))
				{
					easy_json_log("[easy_json] removing found key: %s at path: %s", writer.lastKey, writer.path);
					member->RemoveMember(writer.lastKey);
					ofs.close();
					//save while passing false to restart the saving process without re-processing the actual object.
					return save(false);
				}
			}

			//reset the ofstream and re-save the first loaded settings.
			ofs.close();
			ofs.open(m_file_name.get_string(), std::ofstream::out | std::ofstream::trunc);
			m_backup_object.save(*this);
			ofs.close();

			easy_json_log("[easy_json] failed to resolve the save conflict settings will not be saved.");
			return _easy_json_error_parse_failed;
		}
		easy_json_log("[easy_json] json file successfully saved.");
		// If everything is successful, return success.
		return _easy_json_error_success;
	}

	/// <summary>
	/// Gets a pointer to the JSON Value associated with the current key path in the document.
	/// If the key path is empty, returns a pointer to the root document object.
	/// If any key in the path does not exist, it creates the necessary nested structure in the document.
	/// </summary>
	/// <returns>A pointer to the JSON Value associated with the current key path, or nullptr if the path is invalid.</returns>
	__declspec(noinline) Value* get_current_pointer()
	{
		// Check if path is empty, if so, return a pointer to the root document object
		if (m_current_depth == 0) {
			return Pointer("").Get(m_doc);
		}

		// Obtain a pointer to the root document object
		Value* value = Pointer("").Get(m_doc);
		// Iterate through each element in the path vector
		for (int i = 0; i < m_current_depth; ++i) 
		{
			char* current_key = m_key_path[i];
			// Check if the current value is an object
			if (value->IsObject()) {
				// If the current element is not a member of the object, add a new member with the given name
				if (!value->HasMember(current_key)) {
					Value key;
					key.SetString(current_key, m_doc.GetAllocator());
					value->AddMember(key, rapidjson::Value(rapidjson::kObjectType), m_doc.GetAllocator());
				}
				// Update the value pointer to point to the member with the given name
				value = &((*value)[current_key]);
			}
			else {
				// If the current value is not an object, return a null pointer
				return nullptr;
			}
		}
		// Return a pointer to the final value object in the path
		return value;
	}

	c_easy_json& operator[](const char* key)
	{
		strncpy(m_key_path[m_current_depth], key, k_max_path_item_length - 1);
		m_key_path[m_current_depth][k_max_path_item_length - 1] = '\0';
		m_current_depth++;
		return *this;
	}

	void clear_key_path() {
		memset(m_key_path, 0, sizeof(m_key_path));
		m_current_depth = 0;
	}

	template<typename T>
	inline void value_format(T input, char* out_buffer, size_t buffer_size)
	{
		if constexpr (std::is_same_v<T, int>)
			snprintf(out_buffer, buffer_size, "%d", input);
		else if constexpr (std::is_same_v<T, unsigned int>)
			snprintf(out_buffer, buffer_size, "%u", input);
		else if constexpr (std::is_same_v<T, long>)
			snprintf(out_buffer, buffer_size, "%ld", input);
		else if constexpr (std::is_same_v<T, unsigned long>)
			snprintf(out_buffer, buffer_size, "%lu", input);
		else if constexpr (std::is_same_v<T, long long>)
			snprintf(out_buffer, buffer_size, "%lld", input);
		else if constexpr (std::is_same_v<T, unsigned long long>)
			snprintf(out_buffer, buffer_size, "%llu", input);
		else if constexpr (std::is_same_v<T, float>)
			snprintf(out_buffer, buffer_size, "%f", input);
		else if constexpr (std::is_same_v<T, double>)
			snprintf(out_buffer, buffer_size, "%lf", input);
		else if constexpr (std::is_same_v<T, char*> || std::is_same_v<T, const char*>)
			snprintf(out_buffer, buffer_size, "%s", input);
		else if constexpr (std::is_same_v<T, std::string>)
			snprintf(out_buffer, buffer_size, "%s", input.c_str());
		else if constexpr (std::is_same_v<T, bool>)
			snprintf(out_buffer, buffer_size, "%s", input ? "true" : "false");
		else if constexpr (std::is_same_v<T, short>)
			snprintf(out_buffer, buffer_size, "%hd", input);
		else if constexpr (std::is_same_v<T, unsigned short>)
			snprintf(out_buffer, buffer_size, "%hu", input);
		else if constexpr (std::is_same_v<T, real_point3d>)
			snprintf(out_buffer, buffer_size, "%f, %f, %f", input.x, input.y, input.z);
		else
			static_assert(sizeof(T) == 0, "Unsupported type in easy_json::value_format()");
		out_buffer[strlen(out_buffer)] = '\0';
	}

	template<typename T>
	T get(const char* key, T defaultValue = T{}) {
		// Check if the key exists in the document

		Value* current_object = get_current_pointer();

		if(current_object == nullptr)
		{
			size_t total_length = m_current_depth;
			for (int i = 0; i < m_current_depth; ++i) {
				total_length += strlen(m_key_path[i]);
			}
			char* combined_path = (char*)malloc(total_length + 1);
			strcpy(combined_path, m_key_path[0]);
			for (int i = 1; i < m_current_depth; ++i) {
				strcat(combined_path, "\\");
				strcat(combined_path, m_key_path[i]);
			}
			
			clear_key_path();
			char value_buffer[64];
			value_format(defaultValue, value_buffer, 64);
			easy_json_log("[easy_json] the provided keypath was unable to be accessed.. path: %s", combined_path);
			easy_json_log("[easy_json] get: %s value(default): %s", key, value_buffer);

			free(combined_path);
			return defaultValue;
		}
		
		clear_key_path();
		Value v;

		if (!current_object->HasMember(key)) {
			char value_buffer[64];
			value_format(defaultValue, value_buffer, 64);
			easy_json_log("[easy_json] get: %s value(default): %s", key, value_buffer);
			return defaultValue;
		}

		v = (*current_object)[key];
		constexpr bool is_rapidjson_type_v =
			std::is_same_v<T, bool> ||
			std::is_same_v<T, int> ||
			std::is_same_v<T, long> ||
			std::is_same_v<T, unsigned> ||
			std::is_same_v<T, int64_t> ||
			std::is_same_v<T, uint64_t> ||
			std::is_same_v<T, double> ||
			std::is_same_v<T, float> ||
			std::is_same_v<T, const char*>;

		//Test for all default supported rapidjson types
		
		if constexpr (is_rapidjson_type_v) {
			char value_buffer[64];
			if (v.Is<T>())
			{
				value_format(v.Get<T>(), value_buffer, 64);
				easy_json_log("[easy_json] get: %s value: %s", key, value_buffer);
				return v.Get<T>();
			}
			
			value_format(defaultValue, value_buffer, 64);
			easy_json_log("[easy_json] get: %s value(default): %s", key, value_buffer);
			return defaultValue;
		}
		//specializations for non supported data types below
		else if constexpr (std::is_same_v<T, short>) {
			if (v.Is<int>())
			{
				easy_json_log("[easy_json] get: %s value: %d", key, v.GetInt());
				return static_cast<short>(v.GetInt());
			}

			easy_json_log("[easy_json] get: %s value(default): %d", key, defaultValue);
			return defaultValue;
		}
		else if constexpr (std::is_same_v<T, unsigned short>) {
			if (v.Is<unsigned int>())
			{
				easy_json_log("[easy_json] get: %s value: %d", key, v.GetUint());
				return static_cast<unsigned short>(v.GetUint());
			}

			easy_json_log("[easy_json] get: %s value(default): %d", key, defaultValue);
			return defaultValue;
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			if (v.Is<const char*>())
			{
				easy_json_log("[easy_json] get: %s value: %s", key, v.GetString());
				return v.GetString();
			}
			char value_buffer[64];
			value_format(defaultValue, value_buffer, 64);
			easy_json_log("[easy_json] get: %s value(default): %s", key, value_buffer);
			return defaultValue;
		}
		else if constexpr (std::is_same_v<T, real_point3d>) {
			char value_buffer[4 + (sizeof(float) * CHAR_BIT * 3)];
			if (v[0].Is<float>() && v[1].Is<float>() && v[2].Is<float>()) {
				real_point3d point(
					v[0].GetFloat(),
					v[1].GetFloat(),
					v[2].GetFloat());
				value_format(point, value_buffer, 100);
				easy_json_log("[easy_json] get: %s value: %s", key, value_buffer);
				return point;
			}

			value_format(defaultValue, value_buffer, 100);
			easy_json_log("[easy_json] get: %s value(default): %s", key, value_buffer);
			return defaultValue;
		}
		else {
			// Unsupported type
			static_assert(sizeof(T) == 0, "Unsupported type in easy_json::get()");
		}
	}
	/// <summary>
	/// Retrieves the value associated with the specified key from the JSON document and assigns it to the provided output variable. 
	/// If the key is not found, the provided default value is assigned to the output variable.
	/// </summary>
	/// <typeparam name="T">Type of the value to retrieve and assign.</typeparam>
	/// <param name="key">The key to search for in the JSON document.</param>
	/// <param name="out_variable">Pointer to the variable where the retrieved value will be stored.</param>
	/// <param name="defaultValue">The value to assign if the key is not found (default is a default-constructed instance of the type).</param>
	/// <returns>None.</returns>
	template<typename T>
	void get(const char* key, T* out_variable, T defaultValue = T{})
	{
		T val = get(key, defaultValue);
		*out_variable = val;
	}

	/// <summary>
	/// Retrieves the value associated with the specified key from the JSON document and assigns it to the provided output variable. 
	/// If the key is not found, the function will return the already existing value of the give variable
	/// </summary>
	/// <typeparam name="T">Type of the value to retrieve and assign.</typeparam>
	/// <param name="key">The key to search for in the JSON document.</param>
	/// <param name="out_variable">Pointer to the variable where the retrieved value will be stored.</param>
	/// <returns>None.</returns>
	template<typename T>
	void get_ds(const char* key, T* out_variable)
	{
		T val = get(key, *out_variable);
		*out_variable = val;
	}

	/// <summary>
	/// Sets the specified key in the JSON document to the provided value. If the key already exists, its value is updated;
	/// if not, a new key-value pair is created.
	/// Specialization for a custom data type must be added to this function
	/// </summary>
	/// <typeparam name="T">Type of the value to be set.</typeparam>
	/// <param name="key">The key to set in the JSON document.</param>
	/// <param name="value">The value to be set for the specified key.</param>
	/// <param name="auto_save">Optional parameter indicating whether to save the document automatically after setting the value (default is false).</param>
	/// <returns>None.</returns>
	template<typename T>
	void set(const char* key, T value, bool auto_save = false) {

		Value* current_object = get_current_pointer();

		if (current_object == nullptr)
		{
			size_t total_length = m_current_depth;
			for (int i = 0; i < m_current_depth; ++i) {
				total_length += strlen(m_key_path[i]);
			}
			char* combined_path = (char*)malloc(total_length + 1);
			strcpy(combined_path, m_key_path[0]);
			for (int i = 1; i < m_current_depth; ++i) {
				strcat(combined_path, "\\");
				strcat(combined_path, m_key_path[i]);
			}

			clear_key_path();
			char value_buffer[64];
			value_format(value, value_buffer, 64);
			easy_json_log("[easy_json] the provided keypath was unable to be accessed.. path: %s", combined_path);
			easy_json_log("[easy_json] failed to set: %s value: %s", key, value_buffer);
			return;
		}

		clear_key_path();

		bool is_new = !current_object->HasMember(key);
		Value k(key, m_doc.GetAllocator());
		
		constexpr bool is_rapidjson_type_v =
			std::is_same_v<T, bool> ||
			std::is_same_v<T, int> ||
			std::is_same_v<T, long> ||
			std::is_same_v<T, unsigned> ||
			std::is_same_v<T, int64_t> ||
			std::is_same_v<T, uint64_t> ||
			std::is_same_v<T, double>;
			//Not included as native support doesn't support NAN values
			//std::is_same_v<T, float>;
			//Not included due to the requirement of creating a String Reference for the string.
			//Removing the native support and using a specialization support to make them StringRefs
			//Makes it less complicated to use
			//std::is_same_v<T, const char*>;

		//Test for all default supported rapidjson types
		if constexpr (is_rapidjson_type_v) {
			if (is_new)
				current_object->AddMember(k, value, m_doc.GetAllocator());
			else
				(*current_object)[key].Set<T>(value);
		}
		//specializations for non supported data types below
		else if constexpr (std::is_same_v<T, float>) {
			if (!FloatIsNaN(value))
			{
				if (is_new)
					current_object->AddMember(k, value, m_doc.GetAllocator());
				else
					(*current_object)[key].SetFloat(value);
			}
		}
		else if constexpr (std::is_same_v<T, short>) {
			if (is_new)
				current_object->AddMember(k, value, m_doc.GetAllocator());
			else
				(*current_object)[key].SetInt(value);
		}
		else if constexpr (std::is_same_v<T, unsigned short>) {
			if (is_new)
				current_object->AddMember(k, value, m_doc.GetAllocator());
			else
				(*current_object)[key].SetUint(value);
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			if (is_new)
			{
				Value v(value.c_str(), value.size(), m_doc.GetAllocator());
				current_object->AddMember(k, v, m_doc.GetAllocator());
			}
			else
				(*current_object)[key].SetString(value.c_str(), m_doc.GetAllocator());
		}
		else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
			if (is_new)
				current_object->AddMember(k, StringRef(value), m_doc.GetAllocator());
			else
				(*current_object)[key].SetString(StringRef(value));
		}
		else if constexpr (std::is_same_v<T, real_point3d>) {
			if (is_new)
			{
				Value vals(rapidjson::kArrayType);
				vals.PushBack(value.x, m_doc.GetAllocator());
				vals.PushBack(value.y, m_doc.GetAllocator());
				vals.PushBack(value.z, m_doc.GetAllocator());
				current_object->AddMember(k, vals, m_doc.GetAllocator());
			}
			else
			{
				(*current_object)[key].Clear();
				(*current_object)[key].PushBack(value.x, m_doc.GetAllocator());
				(*current_object)[key].PushBack(value.y, m_doc.GetAllocator());
				(*current_object)[key].PushBack(value.z, m_doc.GetAllocator());
			}
		}
		else {
			// Unsupported type
			static_assert(sizeof(T) == 0, "Unsupported type in easy_json::set()");
		}

		if (auto_save)
			save();
	}
};

template<typename type>
class c_base_easy_json_struct
{
public:
	virtual void load(c_easy_json<type>& json) = 0;
	virtual void save(c_easy_json<type>& json) = 0;
};

#undef easy_json_log
#undef easy_json_logw