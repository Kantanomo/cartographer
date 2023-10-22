#pragma once
#include "stdafx.h"

#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
using namespace rapidjson;

class easy_json_writer : public PrettyWriter<OStreamWrapper> {
public:
    explicit easy_json_writer(OStreamWrapper& os) : PrettyWriter(os), buffer{}, lastKey{}, lastValue{} {}

    bool Key(const Ch* str) {
        copyToString(lastKey, str, std::strlen(str));
        return PrettyWriter::Key(str);
    }

    bool Key(const char* str, SizeType length, bool copy = false)
    {
        copyToString(lastKey, str, length);
        return PrettyWriter::Key(str, length, copy);
    }

    bool Bool(bool b)
    {
        copyToString(lastValue, (b ? "1" : "0"), 1);
        return PrettyWriter::Bool(b);
    }

    bool String(const Ch* str, SizeType length, bool copy = false)
    {
        copyToString(lastValue, str, length);
        return PrettyWriter::String(str, length, copy);
    }

    bool String(const Ch* str)
    {
        copyToString(lastValue, str, std::strlen(str));
        return PrettyWriter::String(str);
    }

    bool Int(int i)
    {
        int length = snprintf(buffer, sizeof(buffer), "%d", i);
        copyToString(lastValue, buffer, length);
        return PrettyWriter::Int(i);
    }

    bool Uint(unsigned u)
    {
        int length = snprintf(buffer, sizeof(buffer), "%d", u);
        copyToString(lastValue, buffer, length);
        return PrettyWriter::Uint(u);
    }

    bool Double(double d)
    {
        int length = snprintf(buffer, sizeof(buffer), "%.17g", d);
        copyToString(lastValue, buffer, length);
        return PrettyWriter::Double(d);
    }

    bool RawNumber(const Ch* str, SizeType length, bool copy = false)
    {
        copyToString(lastValue, str, length);
        return PrettyWriter::RawNumber(str, length, copy);
    }

    bool StartObject()
    {
        return PrettyWriter::StartObject();
    }

    bool EndObject(SizeType memberCount = 0)
    {
        return PrettyWriter::EndObject(memberCount);
    }

    const char* getLastKey() const
    {
        return lastKey;
    }

    const char* getLastValue() const
    {
        return lastValue;
    }

private:
    static void copyToString(char* dest, const Ch* src, SizeType length)
    {
        memcpy(dest, src, length);
        dest[length] = '\0';
    }
    char buffer[32];
    char lastKey[256];
    char lastValue[256];
};