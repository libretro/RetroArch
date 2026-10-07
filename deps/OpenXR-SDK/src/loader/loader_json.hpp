// Copyright (c) 2026 libretro contributors
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Minimal JSON value tree for the loader, parsed with libretro-common's
// rjson instead of the vendored jsoncpp.  It deliberately keeps jsoncpp's
// names (Json::Value and the small API subset the loader uses) so the
// manifest and broker code stays byte-for-byte familiar.

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Json {

enum ValueType { nullValue = 0, stringValue, numberValue, booleanValue, arrayValue, objectValue };

class Value {
   public:
    typedef std::vector<std::pair<std::string, Value>> Members;

    class ValueConstIterator {
       public:
        explicit ValueConstIterator(Members::const_iterator it) : _it(it) {}
        // Member name as a Value, matching jsoncpp's iterator (empty for arrays).
        Value key() const { return Value(_it->first); }
        const Value &operator*() const { return _it->second; }
        const Value *operator->() const { return &_it->second; }
        ValueConstIterator &operator++() {
            ++_it;
            return *this;
        }
        bool operator==(const ValueConstIterator &o) const { return _it == o._it; }
        bool operator!=(const ValueConstIterator &o) const { return _it != o._it; }

       private:
        Members::const_iterator _it;
    };

    Value() : _type(nullValue), _bool(false) {}
    Value(ValueType type) : _type(type), _bool(false) {}
    Value(const char *s) : _type(stringValue), _bool(false), _string(s ? s : "") {}
    Value(const std::string &s) : _type(stringValue), _bool(false), _string(s) {}

    bool isNull() const { return _type == nullValue; }
    bool isString() const { return _type == stringValue; }
    bool isBool() const { return _type == booleanValue; }
    bool isObject() const { return _type == objectValue; }
    bool isArray() const { return _type == arrayValue; }
    // A number whose spelling is a non-negative integer that fits unsigned,
    // matching how the manifest code uses jsoncpp's isUInt().
    bool isUInt() const;
    // Null, or an array/object with no elements (string/number never queried).
    bool empty() const { return _children.empty(); }

    std::string asString() const { return _string; }
    unsigned asUInt() const;

    // Missing members read as a shared null value, like jsoncpp.
    const Value &operator[](const char *key) const;
    const Value &operator[](const std::string &key) const { return (*this)[key.c_str()]; }
    // Creates the member (turning a null value into an object) on first use.
    Value &operator[](const char *key);
    Value &operator[](const std::string &key) { return (*this)[key.c_str()]; }

    ValueConstIterator begin() const { return ValueConstIterator(_children.begin()); }
    ValueConstIterator end() const { return ValueConstIterator(_children.end()); }

    // Tree building (used by the parser; harmless elsewhere).
    void SetNumber(const std::string &spelling) {
        _type = numberValue;
        _string = spelling;
        _children.clear();
    }
    void SetBool(bool v) {
        _type = booleanValue;
        _bool = v;
        _string.clear();
        _children.clear();
    }
    Value &AppendChild(const std::string &key) {
        _children.push_back(std::make_pair(key, Value()));
        return _children.back().second;
    }

   private:
    ValueType _type;
    bool _bool;
    std::string _string; /* string value, or a number's spelling */
    Members _children;
};

typedef Value::ValueConstIterator ValueConstIterator;

// Parses a whole JSON document with rjson.  On failure returns false and
// describes the problem (with line/column) in errors.
bool ParseString(const std::string &content, Value &root, std::string &errors);

}  // namespace Json
