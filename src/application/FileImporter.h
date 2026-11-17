#pragma once
#include "domain/Models.h"
#include <string>
#include <vector>

namespace uic {

// Разворачивает список файлов/папок, нормализует пути и убирает повторы.
class FileImporter {
public:
    // Ключ сравнения путей (Windows нечувствительна к регистру).
    static std::string pathKey(const fs::path& p);
    // Папки обходятся рекурсивно; из них берутся только файлы с известным расширением изображения.
    // Явно указанные файлы берутся любые — формат определяется по содержимому.
    static std::vector<fs::path> collect(const std::vector<fs::path>& inputs);
};

} // namespace uic
