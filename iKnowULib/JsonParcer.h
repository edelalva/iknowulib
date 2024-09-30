#ifndef JSONPARSER_H
#define JSONPARSER_H

#include <string>
#include <map>
#include <sstream>
#include <cctype>

// Simple JSON Parser for { "key": "value" } pairs
class JsonParser {
public:
    std::map<std::string, std::string> parse(const std::string& json) {
        std::map<std::string, std::string> result;
        std::istringstream stream(json);
        char c;

        // Find the opening '{'
        while (stream >> c) {
            if (c == '{') break;
        }

        std::string key, value;
        bool inKey = true;

        while (stream >> c) {
            if (c == '}' || c == EOF) break; // End of JSON object

            if (c == '\"') {  // Start of a key or value
                std::string temp;
                while (stream.get(c) && c != '\"') {
                    temp += c;
                }

                if (inKey) {
                    key = temp;
                    inKey = false;
                }
                else {
                    value = temp;
                    result[key] = value;
                    inKey = true;
                }
            }
        }

        return result;
    }

    std::string toJson(const std::map<std::string, std::string>& jsonMap) {
        std::ostringstream out;
        out << "{";
        for (auto it = jsonMap.begin(); it != jsonMap.end(); ++it) {
            if (it != jsonMap.begin()) {
                out << ",";
            }
            out << "\"" << it->first << "\":\"" << it->second << "\"";
        }
        out << "}";
        return out.str();
    }
}; 

#endif

