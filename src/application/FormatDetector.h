#pragma once
#include "domain/Models.h"
#include <cstdint>
#include <span>

namespace uic {

// Определяет формат по сигнатуре содержимого; расширение файла не используется.
class FormatDetector {
public:
    static ImageFormat detect(std::span<const std::uint8_t> header);
    static Result<ImageFormat> detectFile(const fs::path& path); // читает первые байты
};

} // namespace uic
