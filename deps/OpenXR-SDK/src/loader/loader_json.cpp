// Copyright (c) 2026 libretro contributors
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "loader_json.hpp"

#include <formats/rjson.h>

#include <cstdlib>
#include <sstream>

namespace Json {

static const Value &NullSentinel() {
    static const Value null_value;
    return null_value;
}

bool Value::isUInt() const {
    const char *s;
    char *end;
    unsigned long parsed;
    if (_type != numberValue || _string.empty())
        return false;
    s = _string.c_str();
    if (*s == '-')
        return false;
    parsed = strtoul(s, &end, 10);
    if (*end != '\0')
        return false; /* fractional or exponent spelling */
    return parsed <= 0xffffffffUL;
}

unsigned Value::asUInt() const { return (unsigned)strtoul(_string.c_str(), nullptr, 10); }

const Value &Value::operator[](const char *key) const {
    for (Members::const_iterator it = _children.begin(); it != _children.end(); ++it) {
        if (it->first == key)
            return it->second;
    }
    return NullSentinel();
}

Value &Value::operator[](const char *key) {
    if (_type != objectValue) {
        /* jsoncpp turns a null value into an object here; the loader never
         * indexes a non-null non-object mutably, so do the same for all. */
        _type = objectValue;
        _string.clear();
        _children.clear();
    }
    for (Members::iterator it = _children.begin(); it != _children.end(); ++it) {
        if (it->first == key)
            return it->second;
    }
    return AppendChild(key);
}

static bool BuildValue(rjson_t *parser, enum rjson_type token, Value &out) {
    size_t len;
    const char *str;
    switch (token) {
        case RJSON_OBJECT:
            out = Value(objectValue);
            for (;;) {
                enum rjson_type next = rjson_next(parser);
                std::string key;
                if (next == RJSON_OBJECT_END)
                    return true;
                if (next != RJSON_STRING)
                    return false; /* member name or malformed input */
                str = rjson_get_string(parser, &len);
                key.assign(str, len);
                next = rjson_next(parser);
                if (!BuildValue(parser, next, out.AppendChild(key)))
                    return false;
            }
        case RJSON_ARRAY:
            out = Value(arrayValue);
            for (;;) {
                enum rjson_type next = rjson_next(parser);
                if (next == RJSON_ARRAY_END)
                    return true;
                if (!BuildValue(parser, next, out.AppendChild(std::string())))
                    return false;
            }
        case RJSON_STRING:
            str = rjson_get_string(parser, &len);
            out = Value(std::string(str, len));
            return true;
        case RJSON_NUMBER:
            str = rjson_get_string(parser, &len);
            out.SetNumber(std::string(str, len));
            return true;
        case RJSON_TRUE:
            out.SetBool(true);
            return true;
        case RJSON_FALSE:
            out.SetBool(false);
            return true;
        case RJSON_NULL:
            out = Value();
            return true;
        default:
            return false;
    }
}

bool ParseString(const std::string &content, Value &root, std::string &errors) {
    rjson_t *parser = rjson_open_string(content.c_str(), content.size());
    bool ok = false;
    if (!parser) {
        errors = "out of memory";
        return false;
    }
    /* Match the jsoncpp CharReaderBuilder defaults this replaces: JavaScript
     * comments and data after the document were accepted, and a UTF-8 BOM is
     * harmless, so nothing previously loadable becomes unloadable. */
    rjson_set_options(parser, RJSON_OPTION_ALLOW_COMMENTS | RJSON_OPTION_ALLOW_TRAILING_DATA | RJSON_OPTION_ALLOW_UTF8BOM);
    {
        enum rjson_type first = rjson_next(parser);
        ok = (first != RJSON_DONE && first != RJSON_ERROR) && BuildValue(parser, first, root)
             /* a manifest is one document: anything after it is an error */
             && rjson_next(parser) == RJSON_DONE;
    }
    if (!ok) {
        std::ostringstream error_ss;
        const char *detail = rjson_get_error(parser);
        error_ss << "line " << rjson_get_source_line(parser) << ", column " << rjson_get_source_column(parser) << ": "
                 << ((detail && detail[0]) ? detail : "malformed JSON");
        errors = error_ss.str();
        root = Value();
    }
    rjson_free(parser);
    return ok;
}

}  // namespace Json
