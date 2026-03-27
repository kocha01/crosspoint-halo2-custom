// Desktop logging — printf to stdout
#include <Logging.h>
#include <cstdio>
#include <cstdarg>
#include <string>

void logPrintf(const char* level, const char* origin, const char* format, ...) {
    fprintf(stdout, "%s [%s] ", level, origin);
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fflush(stdout);
}

std::string getLastLogs() { return ""; }
void clearLastLogs() {}
bool sanitizeLogHead() { return false; }

// MySerialImpl
MySerialImpl MySerialImpl::instance;

size_t MySerialImpl::printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stdout, format, args);
    va_end(args);
    return ret > 0 ? ret : 0;
}

size_t MySerialImpl::write(uint8_t b) {
    return fputc(b, stdout) != EOF ? 1 : 0;
}

size_t MySerialImpl::write(const uint8_t* buffer, size_t size) {
    return fwrite(buffer, 1, size, stdout);
}

void MySerialImpl::flush() {
    fflush(stdout);
}
