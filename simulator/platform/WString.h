#pragma once
// Minimal Arduino String class wrapping std::string

#include <string>
#include <cstring>
#include <cstdio>

class String {
    std::string buf;
public:
    String() = default;
    String(const char* s) : buf(s ? s : "") {}
    explicit String(const std::string& s) : buf(s) {}
    String(char c) : buf(1, c) {}
    String(int val) : buf(std::to_string(val)) {}
    String(unsigned int val) : buf(std::to_string(val)) {}
    String(long val) : buf(std::to_string(val)) {}
    String(unsigned long val) : buf(std::to_string(val)) {}

    const char* c_str() const { return buf.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(buf.length()); }
    bool isEmpty() const { return buf.empty(); }

    bool startsWith(const char* prefix) const {
        return buf.compare(0, strlen(prefix), prefix) == 0;
    }
    bool startsWith(const String& prefix) const {
        return buf.compare(0, prefix.buf.size(), prefix.buf) == 0;
    }

    String substring(unsigned int from) const {
        if (from >= buf.size()) return String();
        return String(buf.substr(from));
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from >= buf.size()) return String();
        return String(buf.substr(from, to - from));
    }

    void trim() {
        auto start = buf.find_first_not_of(" \t\r\n");
        auto end = buf.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) { buf.clear(); return; }
        buf = buf.substr(start, end - start + 1);
    }

    int indexOf(char c) const {
        auto pos = buf.find(c);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    char charAt(unsigned int index) const { return buf[index]; }
    char operator[](unsigned int index) const { return buf[index]; }

    // ArduinoJson compatibility
    size_t write(uint8_t c) { buf += static_cast<char>(c); return 1; }
    size_t write(const uint8_t* data, size_t len) { buf.append(reinterpret_cast<const char*>(data), len); return len; }

    String& operator+=(const char* s) { buf += s; return *this; }
    String& operator+=(const String& s) { buf += s.buf; return *this; }
    String& operator+=(char c) { buf += c; return *this; }

    friend String operator+(const String& a, const String& b) { return String(a.buf + b.buf); }
    friend bool operator==(const String& a, const String& b) { return a.buf == b.buf; }
    friend bool operator==(const String& a, const char* b) { return a.buf == b; }
    friend bool operator!=(const String& a, const String& b) { return a.buf != b.buf; }

    // Explicit conversion to std::string to avoid ambiguity with std::string_view overloads
    explicit operator std::string() const { return buf; }
};
