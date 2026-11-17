#include "AtomicFileWriter.h"
#include <cstdio>
#include <fstream>
#include <random>

namespace uic {

namespace {
std::string randomHex() {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<std::uint64_t>(rd()) << 32) ^ rd());
    char buf[24];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(gen()));
    return buf;
}
} // namespace

Result<AtomicFileWriter> AtomicFileWriter::begin(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec))
        return makeError(ErrorCode::IoError, "Не удалось создать или открыть папку назначения");

    for (int attempt = 0; attempt < 8; ++attempt) {
        fs::path tmp = dir / (".uic-" + randomHex() + ".tmp");
        if (fs::exists(tmp, ec)) continue;
        std::ofstream f(tmp, std::ios::binary | std::ios::out);
        if (!f) return makeError(ErrorCode::AccessDenied, "Нет прав на запись в папку назначения");
        return AtomicFileWriter(tmp);
    }
    return makeError(ErrorCode::IoError, "Не удалось подобрать имя временного файла");
}

AtomicFileWriter::AtomicFileWriter(AtomicFileWriter&& o) noexcept : temp_(std::move(o.temp_)) { o.temp_.clear(); }

AtomicFileWriter& AtomicFileWriter::operator=(AtomicFileWriter&& o) noexcept {
    if (this != &o) { discard(); temp_ = std::move(o.temp_); o.temp_.clear(); }
    return *this;
}

AtomicFileWriter::~AtomicFileWriter() { discard(); }

void AtomicFileWriter::discard() noexcept {
    if (temp_.empty()) return;
    std::error_code ec;
    fs::remove(temp_, ec);
    temp_.clear();
}

Status AtomicFileWriter::commit(const fs::path& finalPath, bool replaceExisting) {
    if (temp_.empty()) return makeError(ErrorCode::Internal, "Нет временного файла для фиксации");
    std::error_code ec;
    if (!replaceExisting && fs::exists(finalPath, ec))
        return makeError(ErrorCode::AlreadyExists, "Файл с таким именем уже существует");
    fs::rename(temp_, finalPath, ec); // на Windows заменяет существующий файл
    if (ec) return makeError(ErrorCode::IoError, "Не удалось зафиксировать результат: " + ec.message());
    temp_.clear();
    return Ok{};
}

} // namespace uic
