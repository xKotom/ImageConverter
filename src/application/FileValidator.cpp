#include "FileValidator.h"
#include <fstream>

namespace uic {

Status FileValidator::validate(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return makeError(ErrorCode::NotFound, "Файл не найден");
    if (!fs::is_regular_file(path, ec)) return makeError(ErrorCode::InvalidInput, "Это не обычный файл");
    auto size = fs::file_size(path, ec);
    if (ec) return makeError(ErrorCode::IoError, "Не удалось получить размер файла");
    if (size == 0) return makeError(ErrorCode::CorruptedFile, "Файл пустой");
    if (size > kMaxFileSize) return makeError(ErrorCode::TooLarge, "Файл слишком большой (более 2 ГиБ)");
    std::ifstream in(path, std::ios::binary);
    if (!in) return makeError(ErrorCode::AccessDenied, "Нет доступа к файлу на чтение");
    return Ok{};
}

} // namespace uic
