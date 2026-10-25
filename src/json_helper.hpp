#pragma once

#include <string>
#include <vector>
#include <map>
#include <string_view>
#include <cmath>
#include <sstream>
#include <cctype>

namespace kjd {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue {
    JsonType type{JsonType::Null};
    bool boolVal{false};
    double numVal{0.0};
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::map<std::string, JsonValue> objVal;

    bool isNull() const { return type == JsonType::Null; }
    bool isBool() const { return type == JsonType::Bool; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isString() const { return type == JsonType::String; }
    bool isArray() const { return type == JsonType::Array; }
    bool isObject() const { return type == JsonType::Object; }

    bool asBool(bool def = false) const { return isBool() ? boolVal : def; }
    double asDouble(double def = 0.0) const { return isNumber() ? numVal : def; }
    int64_t asInt64(int64_t def = 0) const { return isNumber() ? static_cast<int64_t>(numVal) : def; }
    const std::string& asString() const { return strVal; }

    bool has(const std::string& key) const {
        return isObject() && (objVal.find(key) != objVal.end());
    }

    const JsonValue* get(const std::string& key) const {
        if (!isObject()) return nullptr;
        auto it = objVal.find(key);
        return (it != objVal.end()) ? &it->second : nullptr;
    }
};

class SimpleJson {
public:
    static bool parse(std::string_view text, JsonValue& out, std::string& err) {
        size_t idx = 0;
        skipWhitespace(text, idx);
        if (!parseValue(text, idx, out, err)) {
            return false;
        }
        skipWhitespace(text, idx);
        if (idx != text.size()) {
            err = "Trailing characters after JSON value";
            return false;
        }
        return true;
    }

private:
    static void skipWhitespace(std::string_view s, size_t& idx) {
        while (idx < s.size() && (s[idx] == ' ' || s[idx] == '\t' || s[idx] == '\r' || s[idx] == '\n')) {
            idx++;
        }
    }

    static bool parseValue(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        skipWhitespace(s, idx);
        if (idx >= s.size()) {
            err = "Unexpected end of input";
            return false;
        }

        char c = s[idx];
        if (c == '{') return parseObject(s, idx, val, err);
        if (c == '[') return parseArray(s, idx, val, err);
        if (c == '"') return parseString(s, idx, val, err);
        if (c == 't' || c == 'f') return parseBool(s, idx, val, err);
        if (c == 'n') return parseNull(s, idx, val, err);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(s, idx, val, err);

        err = std::string("Unexpected token '") + c + "'";
        return false;
    }

    static bool parseObject(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        idx++; // skip '{'
        val.type = JsonType::Object;
        val.objVal.clear();

        skipWhitespace(s, idx);
        if (idx < s.size() && s[idx] == '}') {
            idx++;
            return true;
        }

        while (idx < s.size()) {
            skipWhitespace(s, idx);
            if (idx >= s.size() || s[idx] != '"') {
                err = "Expected string key in object";
                return false;
            }

            JsonValue keyVal;
            if (!parseString(s, idx, keyVal, err)) return false;

            skipWhitespace(s, idx);
            if (idx >= s.size() || s[idx] != ':') {
                err = "Expected ':' after object key";
                return false;
            }
            idx++; // skip ':'

            JsonValue childVal;
            if (!parseValue(s, idx, childVal, err)) return false;

            val.objVal[keyVal.strVal] = std::move(childVal);

            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                idx++;
                continue;
            } else if (idx < s.size() && s[idx] == '}') {
                idx++;
                return true;
            } else {
                err = "Expected ',' or '}' in object";
                return false;
            }
        }

        err = "Unterminated object";
        return false;
    }

    static bool parseArray(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        idx++; // skip '['
        val.type = JsonType::Array;
        val.arrVal.clear();

        skipWhitespace(s, idx);
        if (idx < s.size() && s[idx] == ']') {
            idx++;
            return true;
        }

        while (idx < s.size()) {
            JsonValue elem;
            if (!parseValue(s, idx, elem, err)) return false;
            val.arrVal.push_back(std::move(elem));

            skipWhitespace(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                idx++;
                continue;
            } else if (idx < s.size() && s[idx] == ']') {
                idx++;
                return true;
            } else {
                err = "Expected ',' or ']' in array";
                return false;
            }
        }

        err = "Unterminated array";
        return false;
    }

    static bool parseString(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        idx++; // skip opening '"'
        val.type = JsonType::String;
        val.strVal.clear();

        while (idx < s.size()) {
            char c = s[idx++];
            if (c == '"') {
                return true;
            }
            if (c == '\\') {
                if (idx >= s.size()) {
                    err = "Unexpected end in string escape";
                    return false;
                }
                char esc = s[idx++];
                if (esc == '"') val.strVal += '"';
                else if (esc == '\\') val.strVal += '\\';
                else if (esc == '/') val.strVal += '/';
                else if (esc == 'b') val.strVal += '\b';
                else if (esc == 'f') val.strVal += '\f';
                else if (esc == 'n') val.strVal += '\n';
                else if (esc == 'r') val.strVal += '\r';
                else if (esc == 't') val.strVal += '\t';
                else {
                    val.strVal += esc;
                }
            } else {
                val.strVal += c;
            }
        }

        err = "Unterminated string";
        return false;
    }

    static bool parseNumber(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        size_t start = idx;
        if (s[idx] == '-') idx++;

        while (idx < s.size() && std::isdigit(static_cast<unsigned char>(s[idx]))) {
            idx++;
        }

        if (idx < s.size() && s[idx] == '.') {
            idx++;
            while (idx < s.size() && std::isdigit(static_cast<unsigned char>(s[idx]))) {
                idx++;
            }
        }

        if (idx < s.size() && (s[idx] == 'e' || s[idx] == 'E')) {
            idx++;
            if (idx < s.size() && (s[idx] == '+' || s[idx] == '-')) idx++;
            while (idx < s.size() && std::isdigit(static_cast<unsigned char>(s[idx]))) {
                idx++;
            }
        }

        std::string numStr(s.substr(start, idx - start));
        char* endPtr = nullptr;
        double d = std::strtod(numStr.c_str(), &endPtr);
        if (endPtr != numStr.c_str() + numStr.size() || !std::isfinite(d)) {
            err = "Invalid number: " + numStr;
            return false;
        }

        val.type = JsonType::Number;
        val.numVal = d;
        return true;
    }

    static bool parseBool(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        if (s.substr(idx, 4) == "true") {
            idx += 4;
            val.type = JsonType::Bool;
            val.boolVal = true;
            return true;
        }
        if (s.substr(idx, 5) == "false") {
            idx += 5;
            val.type = JsonType::Bool;
            val.boolVal = false;
            return true;
        }
        err = "Invalid boolean literal";
        return false;
    }

    static bool parseNull(std::string_view s, size_t& idx, JsonValue& val, std::string& err) {
        if (s.substr(idx, 4) == "null") {
            idx += 4;
            val.type = JsonType::Null;
            return true;
        }
        err = "Invalid null literal";
        return false;
    }
};

} // namespace kjd
