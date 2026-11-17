#pragma once
#include "domain/Models.h"

namespace uic {

struct NameDecision {
    fs::path path;
    bool skip = false;    // политика Skip и файл уже существует
    bool replace = false; // итоговый файл существует и будет заменён
};

// Формирует имя результата и разрешает конфликты. Никогда не выбирает путь исходника.
class OutputNamer {
public:
    using ExistsFn = std::function<bool(const fs::path&)>;
    static NameDecision resolve(const fs::path& source, const fs::path& outputDir,
                                ImageFormat target, ConflictPolicy policy,
                                const ExistsFn& exists = nullptr);
};

} // namespace uic
