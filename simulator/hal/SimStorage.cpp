// Desktop HalStorage implementation — std::fstream backed
#define HAL_STORAGE_IMPL
#include <HalStorage.h>
#include <Logging.h>

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

// ── Configuration ──
static std::string sdRootPath = "./sd_data";

void simStorageSetRoot(const std::string& path) {
    sdRootPath = path;
}

static std::string resolvePath(const char* path) {
    if (path[0] == '/') return sdRootPath + path;
    return sdRootPath + "/" + path;
}

// ── HalFile::Impl ──
class HalFile::Impl {
public:
    std::fstream stream;
    std::string filePath;
    bool isDir = false;
    bool opened = false;
    bool writeMode = false;  // Track if opened for writing (use tellp instead of tellg)

    // For directory iteration
    std::vector<std::string> dirEntries;
    size_t dirIndex = 0;
};

// ── HalFile ──
HalFile::HalFile() : impl(std::make_unique<Impl>()) {}
HalFile::~HalFile() { close(); }
HalFile::HalFile(HalFile&& o) = default;
HalFile& HalFile::operator=(HalFile&& o) = default;
HalFile::HalFile(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

void HalFile::flush() { if (impl && impl->stream.is_open()) impl->stream.flush(); }

size_t HalFile::getName(char* name, size_t len) {
    if (!impl) return 0;
    auto fname = fs::path(impl->filePath).filename().string();
    strncpy(name, fname.c_str(), len);
    return fname.size();
}

size_t HalFile::size() { return fileSize(); }

size_t HalFile::fileSize() {
    if (!impl || !impl->opened) return 0;
    if (impl->isDir) return 0;
    // Use tellp/seekp for write-only files (tellg/seekg would corrupt on macOS)
    if (impl->writeMode) {
        auto pos = impl->stream.tellp();
        impl->stream.seekp(0, std::ios::end);
        auto sz = impl->stream.tellp();
        impl->stream.seekp(pos);
        return static_cast<size_t>(sz);
    }
    auto pos = impl->stream.tellg();
    impl->stream.seekg(0, std::ios::end);
    auto sz = impl->stream.tellg();
    impl->stream.seekg(pos);
    return static_cast<size_t>(sz);
}

bool HalFile::seek(size_t pos) { return seekSet(pos); }

bool HalFile::seekCur(int64_t offset) {
    if (!impl || !impl->stream.is_open()) return false;
    impl->stream.clear();  // Clear any prior eof/fail flags
    // Use seekp for write-only files, seekg for read files (macOS fstream bug)
    if (impl->writeMode) {
        impl->stream.seekp(offset, std::ios::cur);
    } else {
        impl->stream.seekg(offset, std::ios::cur);
    }
    return !impl->stream.fail();
}

bool HalFile::seekSet(size_t offset) {
    if (!impl || !impl->stream.is_open()) return false;
    impl->stream.clear();  // Clear any prior eof/fail flags
    if (impl->writeMode) {
        impl->stream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    } else {
        impl->stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    }
    return !impl->stream.fail();
}

int HalFile::available() const {
    if (!impl || !impl->stream.is_open()) return 0;
    auto pos = impl->stream.tellg();
    auto& s = const_cast<std::fstream&>(impl->stream);
    s.seekg(0, std::ios::end);
    auto end = s.tellg();
    s.seekg(pos);
    return static_cast<int>(end - pos);
}

size_t HalFile::position() const {
    if (!impl || !impl->stream.is_open()) return 0;
    auto& s = const_cast<std::fstream&>(impl->stream);
    // CRITICAL: On macOS, calling tellg() on a write-only fstream corrupts the
    // put pointer (same class of bug as seekp() corrupting tellg() on read files).
    // Use tellp() for write-mode files, tellg() for read-mode files.
    if (impl->writeMode) {
        return static_cast<size_t>(s.tellp());
    }
    return static_cast<size_t>(s.tellg());
}

int HalFile::read(void* buf, size_t count) {
    if (!impl || !impl->stream.is_open()) return -1;
    impl->stream.clear();  // Clear eof/fail from prior operations
    impl->stream.read(static_cast<char*>(buf), count);
    return static_cast<int>(impl->stream.gcount());
}

int HalFile::read() {
    uint8_t b;
    if (read(&b, 1) == 1) return b;
    return -1;
}

size_t HalFile::write(const void* buf, size_t count) {
    if (!impl || !impl->stream.is_open()) return 0;
    impl->stream.write(static_cast<const char*>(buf), count);
    return impl->stream.fail() ? 0 : count;
}

size_t HalFile::write(uint8_t b) {
    return write(&b, 1);
}

bool HalFile::rename(const char*) { return false; /* not needed for simulator */ }

bool HalFile::isDirectory() const {
    return impl && impl->isDir;
}

void HalFile::rewindDirectory() {
    if (impl) impl->dirIndex = 0;
}

bool HalFile::close() {
    if (impl && impl->stream.is_open()) {
        impl->stream.close();
    }
    if (impl) impl->opened = false;
    return true;
}

HalFile HalFile::openNextFile() {
    if (!impl || !impl->isDir || impl->dirIndex >= impl->dirEntries.size()) {
        return HalFile();
    }
    auto entryPath = impl->dirEntries[impl->dirIndex++];
    auto fileImpl = std::make_unique<Impl>();
    fileImpl->filePath = entryPath;
    if (fs::is_directory(entryPath)) {
        fileImpl->isDir = true;
        fileImpl->opened = true;
    } else {
        fileImpl->stream.open(entryPath, std::ios::in | std::ios::binary);
        fileImpl->opened = fileImpl->stream.is_open();
    }
    return HalFile(std::move(fileImpl));
}

bool HalFile::isOpen() const { return impl && impl->opened; }
HalFile::operator bool() const { return isOpen(); }

// ── HalStorage ──
HalStorage HalStorage::instance;

HalStorage::HalStorage() {}

bool HalStorage::begin() {
    if (!fs::exists(sdRootPath)) {
        fs::create_directories(sdRootPath);
    }
    initialized = true;
    return true;
}

bool HalStorage::ready() const { return initialized; }

std::vector<String> HalStorage::listFiles(const char* path, int maxFiles) {
    std::vector<String> result;
    auto fullPath = resolvePath(path);
    if (!fs::exists(fullPath) || !fs::is_directory(fullPath)) return result;
    for (auto& entry : fs::directory_iterator(fullPath)) {
        if (static_cast<int>(result.size()) >= maxFiles) break;
        result.push_back(String(entry.path().filename().string().c_str()));
    }
    return result;
}

String HalStorage::readFile(const char* path) {
    auto fullPath = resolvePath(path);
    std::ifstream f(fullPath);
    if (!f.is_open()) return String();
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return String(content.c_str());
}

bool HalStorage::readFileToStream(const char*, Print&, size_t) { return false; }

size_t HalStorage::readFileToBuffer(const char* path, char* buffer, size_t bufferSize, size_t maxBytes) {
    auto fullPath = resolvePath(path);
    std::ifstream f(fullPath, std::ios::binary);
    if (!f.is_open()) return 0;
    size_t toRead = maxBytes > 0 ? std::min(maxBytes, bufferSize) : bufferSize;
    f.read(buffer, toRead);
    return static_cast<size_t>(f.gcount());
}

bool HalStorage::writeFile(const char* path, const String& content) {
    auto fullPath = resolvePath(path);
    // Ensure parent directory exists
    fs::create_directories(fs::path(fullPath).parent_path());
    std::ofstream f(fullPath);
    if (!f.is_open()) return false;
    f << content.c_str();
    return true;
}

bool HalStorage::ensureDirectoryExists(const char* path) {
    return mkdir(path, true);
}

HalFile HalStorage::open(const char* path, const oflag_t oflag) {
    auto fullPath = resolvePath(path);
    auto fileImpl = std::make_unique<HalFile::Impl>();
    fileImpl->filePath = fullPath;

    if (fs::is_directory(fullPath)) {
        fileImpl->isDir = true;
        fileImpl->opened = true;
        for (auto& entry : fs::directory_iterator(fullPath)) {
            fileImpl->dirEntries.push_back(entry.path().string());
        }
    } else {
        std::ios_base::openmode mode = std::ios::binary;
        if (oflag & O_WRONLY) {
            mode |= std::ios::out;
            if (oflag & O_TRUNC) mode |= std::ios::trunc;
            if (oflag & O_APPEND) mode |= std::ios::app;
            fileImpl->writeMode = true;
            // Ensure parent directory exists
            fs::create_directories(fs::path(fullPath).parent_path());
        } else {
            mode |= std::ios::in;
        }
        if (oflag & O_RDWR) mode |= std::ios::in | std::ios::out;
        fileImpl->stream.open(fullPath, mode);
        fileImpl->opened = fileImpl->stream.is_open();
    }

    return HalFile(std::move(fileImpl));
}

bool HalStorage::mkdir(const char* path, const bool) {
    auto fullPath = resolvePath(path);
    return fs::create_directories(fullPath);
}

bool HalStorage::exists(const char* path) {
    return fs::exists(resolvePath(path));
}

bool HalStorage::remove(const char* path) {
    return fs::remove(resolvePath(path));
}

bool HalStorage::rename(const char* oldPath, const char* newPath) {
    try {
        fs::rename(resolvePath(oldPath), resolvePath(newPath));
        return true;
    } catch (...) {
        return false;
    }
}

bool HalStorage::rmdir(const char* path) {
    return fs::remove_all(resolvePath(path)) > 0;
}

bool HalStorage::openFileForRead(const char*, const char* path, HalFile& file) {
    file = open(path, O_RDONLY);
    return file.isOpen();
}

bool HalStorage::openFileForRead(const char* mod, const std::string& path, HalFile& file) {
    return openFileForRead(mod, path.c_str(), file);
}

bool HalStorage::openFileForRead(const char* mod, const String& path, HalFile& file) {
    return openFileForRead(mod, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char*, const char* path, HalFile& file) {
    file = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    return file.isOpen();
}

bool HalStorage::openFileForWrite(const char* mod, const std::string& path, HalFile& file) {
    return openFileForWrite(mod, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* mod, const String& path, HalFile& file) {
    return openFileForWrite(mod, path.c_str(), file);
}

bool HalStorage::removeDir(const char* path) {
    return rmdir(path);
}
