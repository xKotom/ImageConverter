#pragma once
#include "domain/Models.h"

namespace uic {

// Проверка пути: существование, тип элемента, доступ на чтение, пустой/слишком большой файл.
class FileValidator {
public:
    static constexpr std::uintmax_t kMaxFileSize = 2ull * 1024 * 1024 * 1024; // 2 ГиБ
    static Status validate(const fs::path& path);
};

} // namespace uic
