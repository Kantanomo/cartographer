#pragma once
#include "stdafx.h"

#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "rapidjson/ostreamwrapper.h"
using namespace rapidjson;

class easy_json_writer : public PrettyWriter<OStreamWrapper> {
public:
    explicit easy_json_writer(OStreamWrapper& os) : PrettyWriter(os), buffer{}, lastKey{}, lastValue{} {}
    std::string path;
    char lastKey[256];
    char lastValue[256];
    bool Key(const Ch* str) {
        copy_rj_ch_to_buffer(lastKey, str, std::strlen(str));
        return PrettyWriter::Key(str);
    }

    bool Key(const char* str, SizeType length, bool copy = false)
    {
        copy_rj_ch_to_buffer(lastKey, str, length);
        return PrettyWriter::Key(str, length, copy);
    }

    bool Bool(bool b)
    {
        copy_rj_ch_to_buffer(lastValue, (b ? "1" : "0"), 1);
        return PrettyWriter::Bool(b);
    }

    bool String(const Ch* str, SizeType length, bool copy = false)
    {
        copy_rj_ch_to_buffer(lastValue, str, length);
        return PrettyWriter::String(str, length, copy);
    }

    bool String(const Ch* str)
    {
        copy_rj_ch_to_buffer(lastValue, str, std::strlen(str));
        return PrettyWriter::String(str);
    }

    bool Int(int i)
    {
        int length = snprintf(buffer, sizeof(buffer), "%d", i);
        copy_rj_ch_to_buffer(lastValue, buffer, length);
        return PrettyWriter::Int(i);
    }

    bool Uint(unsigned u)
    {
        int length = snprintf(buffer, sizeof(buffer), "%d", u);
        copy_rj_ch_to_buffer(lastValue, buffer, length);
        return PrettyWriter::Uint(u);
    }

    bool Double(double d)
    {
        int length = snprintf(buffer, sizeof(buffer), "%.17g", d);
        copy_rj_ch_to_buffer(lastValue, buffer, length);
        bool res =  PrettyWriter::Double(d);
        return res;
    }

    bool RawNumber(const Ch* str, SizeType length, bool copy = false)
    {
        copy_rj_ch_to_buffer(lastValue, str, length);
        return PrettyWriter::RawNumber(str, length, copy);
    }

    bool StartObject()
    {
        if (lastKey[0] == '\0')
        {
            path += ".";
            path += lastKey;
        }
        return PrettyWriter::StartObject();
    }

    bool EndObject(SizeType memberCount = 0)
    {
        size_t lastDot = path.rfind('.');
        if (lastDot != std::string::npos) {
            path = path.erase(lastDot);
        }
        return PrettyWriter::EndObject(memberCount);
    }

    std::vector<std::string> get_path_vector() const
    {
        std::vector<std::string> tokens;
        std::istringstream stream(path);
        std::string token;

        while (std::getline(stream, token, '.')) {
            if(!token.empty())
				tokens.push_back(token);
        }

        return tokens;
    }
private:
    static void copy_rj_ch_to_buffer(char* dest, const Ch* src, SizeType length)
    {
        memcpy(dest, src, length);
        dest[length] = '\0';
    }
    
    char buffer[32];

};