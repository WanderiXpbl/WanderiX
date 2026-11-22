#ifndef JSON_H
#define JSON_H

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
using namespace std;

// Tiny helpers to build JSON text by hand, so the server needs no extra library.
// Example:  JsonObject o;  o.addText("name", "FRI").addInt("cost", 200);  o.str()  ->  {"name":"FRI","cost":200}

// "Kalsang "Corner"" -> "\"Kalsang \\\"Corner\\\"\""  (quotes and special characters escaped)
string jsonText(string s) {
    string out = "\"";
    for (int i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 32) out += " ";
        else out += c;
    }
    return out + "\"";
}

string jsonNumber(double x, int decimals) {
    ostringstream out;
    out << fixed << setprecision(decimals) << x;
    return out.str();
}

string jsonNumber(int x) {
    return to_string(x);
}

string jsonBool(bool b) {
    return b ? "true" : "false";
}

class JsonObject {
private:
    vector<string> fields;

public:
    // value is already JSON (a number, another object, an array...)
    JsonObject& add(string key, string jsonValue) {
        fields.push_back(jsonText(key) + ":" + jsonValue);
        return *this;
    }
    JsonObject& addText(string key, string value) { return add(key, jsonText(value)); }
    JsonObject& addInt(string key, int value) { return add(key, jsonNumber(value)); }
    JsonObject& addNumber(string key, double value, int decimals) { return add(key, jsonNumber(value, decimals)); }
    JsonObject& addBool(string key, bool value) { return add(key, jsonBool(value)); }

    string str() {
        string out = "{";
        for (int i = 0; i < fields.size(); i++) {
            if (i > 0) out += ",";
            out += fields[i];
        }
        return out + "}";
    }
};

class JsonArray {
private:
    vector<string> items;

public:
    JsonArray& add(string jsonValue) {
        items.push_back(jsonValue);
        return *this;
    }

    string str() {
        string out = "[";
        for (int i = 0; i < items.size(); i++) {
            if (i > 0) out += ",";
            out += items[i];
        }
        return out + "]";
    }
};

#endif
