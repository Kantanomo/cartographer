#pragma once
#include "stdafx.h"

#include "easy_json_writer.h"
#include "H2MOD/Utils/Utils.h"
#include "rapidjson/prettywriter.h"
#include "Blam/Math/real_math.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/pointer.h"
#include "rapidjson/error/en.h"

using namespace rapidjson;
#define EASY_JSON_LOG(fmt, ...) \
    if (log_function != nullptr) \
        log_function(fmt, ##__VA_ARGS__)

#define EASY_JSON_LOGW(fmt, ...) \
    if (log_w_function != nullptr) \
        log_w_function(fmt, ##__VA_ARGS__)




template<typename struct_type>
class easy_json {
private:
    wchar_t file_name[255];
    Document doc_;
    std::vector<std::string> key_path;
    struct_type* object;

public:
    enum e_easy_json_error : byte {
        success,
        file_open_error,
        json_parse_error
    };
    typedef void t_log_function(const char* format, ...);
    typedef void t_log_w_function(const wchar_t* format, ...);
    t_log_function* log_function;
    t_log_w_function* log_w_function;

    easy_json(const wchar_t* file_name_in, struct_type* object_pointer)
    {
        wcsncpy(file_name, file_name_in, 255);
        object = object_pointer;
        doc_.SetObject();
    }

    e_easy_json_error load() {
        EASY_JSON_LOGW(L"[easy_json] attempting to load file \"%ws\"", file_name);
        std::ifstream ifs(file_name);
        if (!ifs) {
            EASY_JSON_LOG("[easy_json] file does not exist, creating new file");
            // The file does not exist, so create it.
            std::ofstream ofs(file_name);
            if (!ofs) {
                EASY_JSON_LOG("[easy_json] failed to create file");
                throw std::runtime_error("Failed to create file");
            }
            ofs << "{}";  // Initialize with an empty JSON object.
            ofs.close();
            ifs.open(file_name);
        }
        IStreamWrapper isw(ifs);
        doc_.SetObject();
        ParseResult parse_result = doc_.ParseStream(isw);
        if (doc_.HasParseError())
        {
            ParseErrorCode errorCode = parse_result.Code();
            size_t errorOffset = parse_result.Offset();
            EASY_JSON_LOG("[easy_json] json parsing error code: %d at offset %d", errorCode, errorOffset);
            
            size_t currentPosition = ifs.tellg();
            size_t lineStart = 0;
            size_t lineCount = 1;
            ifs.seekg(0);
            std::string line;
            while (std::getline(ifs, line))
            {
                if (lineStart <= errorOffset && errorOffset <= lineStart + line.length())
                {
                    EASY_JSON_LOG("[easy_json] offending line in the json file: %s", line.c_str());
                    EASY_JSON_LOG("[easy_json] error message: %s", GetParseError_En(errorCode));
                    break;
                }
                lineStart += line.length() + 1;  // +1 for the newline character
                lineCount++;
            }

            return e_easy_json_error::json_parse_error;
        }

        object->load(*this);
        return e_easy_json_error::success;
    }

    e_easy_json_error save(bool save_object = true) {
        EASY_JSON_LOGW(L"[easy_json] attempting to save file \"%ws\"", file_name);
        std::ofstream ofs(file_name);
        if (!ofs.is_open()) {
            EASY_JSON_LOG("[easy_json] json file could not be opened, perhaps it is open in another program or you do not have permissions to write to the location.");
            return e_easy_json_error::file_open_error;
        }

        OStreamWrapper osw(ofs);
        easy_json_writer writer(osw);
        writer.SetIndent('\t', 1);  // Set indentation to use tabs.

        if (save_object)
            object->save(*this);

        // Serialize the document and handle any errors during serialization.
        if (!doc_.Accept(writer)) 
        {
            EASY_JSON_LOG("[easy_json] json writing failed on key: %s with value %s", writer.getLastKey(), writer.getLastValue());
            return e_easy_json_error::json_parse_error;
        }

        // If everything is successful, return success.
        return e_easy_json_error::success;
    }

    /// <summary>
   /// Gets a pointer to the JSON Value associated with the current key path in the document.
   /// If the key path is empty, returns a pointer to the root document object.
   /// If any key in the path does not exist, it creates the necessary nested structure in the document.
   /// </summary>
   /// <returns>A pointer to the JSON Value associated with the current key path, or nullptr if the path is invalid.</returns>
    Value* get_current_pointer()
    {
        // Check if path is empty, if so, return a pointer to the root document object
        if (key_path.empty()) {
            return Pointer("").Get(doc_);
        }

        // Obtain a pointer to the root document object
        Value* value = Pointer("").Get(doc_);

        // Iterate through each element in the path vector
        for (const auto& element : key_path)
        {
            // Check if the current value is an object
            if (value->IsObject()) {
                // If the current element is not a member of the object, add a new member with the given name
                if (!value->HasMember(element.c_str())) {
                    Value key;
                    key.SetString(element.c_str(), doc_.GetAllocator());
                    value->AddMember(key, rapidjson::Value(rapidjson::kObjectType), doc_.GetAllocator());
                }
                // Update the value pointer to point to the member with the given name
                value = &((*value)[element.c_str()]);
            }
            else {
                // If the current value is not an object, return a null pointer
                return nullptr;
            }
        }

        // Return a pointer to the final value object in the path
        return value;
    }
    easy_json& operator[](const char* key)
    {
        key_path.emplace_back(key);
        return *this;
    }

    template<typename T>
    T get(const char* key, T defaultValue = T{}) {
        // Check if the key exists in the document

        Value* current_object = get_current_pointer();

        if(current_object == nullptr)
        {
            std::string key_path_str = "\\";
            for (const auto& str : key_path)
                key_path_str += str + "\\";
            key_path.clear();
            std::stringstream ss;
            ss << defaultValue;
        	EASY_JSON_LOG("[easy_json] the provided keypath was unable to be accessed.. path: %s", key_path_str.c_str());
            EASY_JSON_LOG("[easy_json] get: %s value(default): %s", key, ss.str().c_str());
            return defaultValue;
        }
        
        key_path.clear();
        Value v;

        if (!current_object->HasMember(key)) {
            std::stringstream ss;
            ss << defaultValue;
            EASY_JSON_LOG("[easy_json] get: %s value(default): %s", key, ss.str().c_str());
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
            std::stringstream ss;
            if (v.Is<T>())
            {
                ss << v.Get<T>();
                EASY_JSON_LOG("[easy_json] get: %s value: %s", key, ss.str().c_str());
                return v.Get<T>();
            }
            
            ss << defaultValue;
            EASY_JSON_LOG("[easy_json] get: %s value(default): %s", key, ss.str().c_str());
            return defaultValue;
        }
        //specializations for non supported data types below
        else if constexpr (std::is_same_v<T, short>) {
            if (v.Is<int>())
            {
                EASY_JSON_LOG("[easy_json] get: %s value: %d", key, v.GetInt());
                return static_cast<short>(v.GetInt());
            }

            EASY_JSON_LOG("[easy_json] get: %s value(default): %d", key, defaultValue);
            return defaultValue;
        }
        else if constexpr (std::is_same_v<T, unsigned short>) {
            if (v.Is<unsigned int>())
            {
                EASY_JSON_LOG("[easy_json] get: %s value: %d", key, v.GetUint());
                return static_cast<unsigned short>(v.GetUint());
            }

            EASY_JSON_LOG("[easy_json] get: %s value(default): %d", key, defaultValue);
            return defaultValue;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
        	if (v.Is<const char*>())
            {
                EASY_JSON_LOG("[easy_json] get: %s value: %s", key, v.GetString());
                return v.GetString();
            }
            //push the default value onto a stringstream due to ambiguous variadic templating.
            std::stringstream ss;
            ss << defaultValue;
            EASY_JSON_LOG("[easy_json] get: %s value(default): %s", key, ss.str().c_str());
            return defaultValue;
        }
        else if constexpr (std::is_same_v<T, real_point3d>) {
            std::stringstream ss;
            if (v[0].Is<float>() && v[1].Is<float>() && v[2].Is<float>()) {
                auto x = v[0].GetFloat();
                auto y = v[1].GetFloat();
                auto z = v[2].GetFloat();
                ss << x << ", " << y << ", " << z;
                EASY_JSON_LOG("[easy_json] get: %s value: %s", key, ss.str().c_str());
                return real_point3d(x, y, z);
            }

            ss << defaultValue.x << ", " << defaultValue.y << ", " << defaultValue.z;
            EASY_JSON_LOG("[easy_json] get: %s value(default): %s", key, ss.str().c_str());
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
            std::string key_path_str = "\\";
            for (const auto& str : key_path)
                key_path_str += str + "\\";
            key_path.clear();
            std::stringstream ss;
            ss << value;
            EASY_JSON_LOG("[easy_json] the provided keypath was unable to be accessed.. path: %s", key_path_str.c_str());
            EASY_JSON_LOG("[easy_json] failed to set: %s value: %s", key, ss.str().c_str());
            return;
        }

        key_path.clear();

        bool is_new = !current_object->HasMember(key);
        Value k(key, doc_.GetAllocator());
        
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
        //Removing the native support and using a specialization support to make them into std::strings
        //is a lot less complicated and works better.
        //std::is_same_v<T, const char*>;

    //Test for all default supported rapidjson types
        if constexpr (is_rapidjson_type_v) {
            if (is_new)
                current_object->AddMember(k, value, doc_.GetAllocator());
            else
                (*current_object)[key].Set<T>(value);
        }
        //specializations for non supported data types below
        else if constexpr (std::is_same_v<T, float>) {
            if (!FloatIsNaN(value))
            {
                if (is_new)
                    current_object->AddMember(k, value, doc_.GetAllocator());
                else
                    (*current_object)[key].SetFloat(value);
            }
        }
        else if constexpr (std::is_same_v<T, short>) {
            if (is_new)
                current_object->AddMember(k, value, doc_.GetAllocator());
            else
                (*current_object)[key].SetInt(value);
        }
        else if constexpr (std::is_same_v<T, unsigned short>) {
            if (is_new)
                current_object->AddMember(k, value, doc_.GetAllocator());
            else
                (*current_object)[key].SetUint(value);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            if (is_new)
            {
                Value v(value.c_str(), value.size(), doc_.GetAllocator());
                current_object->AddMember(k, v, doc_.GetAllocator());
            }
            else
                (*current_object)[key].SetString(value.c_str(), doc_.GetAllocator());
        }
        else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
            if (is_new)
                current_object->AddMember(k, StringRef(value), doc_.GetAllocator());
            else
                (*current_object)[key].SetString(StringRef(value));
        }
        else if constexpr (std::is_same_v<T, real_point3d>) {
            if (is_new)
            {
                Value vals(rapidjson::kArrayType);
                vals.PushBack(value.x, doc_.GetAllocator());
                vals.PushBack(value.y, doc_.GetAllocator());
                vals.PushBack(value.z, doc_.GetAllocator());
                current_object->AddMember(k, vals, doc_.GetAllocator());
            }
            else
            {
                (*current_object)[key].Clear();
                (*current_object)[key].PushBack(value.x, doc_.GetAllocator());
                (*current_object)[key].PushBack(value.y, doc_.GetAllocator());
                (*current_object)[key].PushBack(value.z, doc_.GetAllocator());
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
    virtual void load(easy_json<type>& json) = 0;
    virtual void save(easy_json<type>& json) = 0;
};

#undef EASY_JSON_LOG
#undef EASY_JSON_LOGW