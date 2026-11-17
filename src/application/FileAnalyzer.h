#pragma once
#include "CodecRegistry.h"

namespace uic {

// Объединяет проверку пути, сигнатуру и зондирование декодером в FileInfo.
// Формат берётся из содержимого; расширение хранится отдельно как extensionHint.
class FileAnalyzer {
public:
    explicit FileAnalyzer(const CodecRegistry& registry) : registry_(registry) {}
    Result<FileInfo> analyze(const fs::path& path) const;
private:
    const CodecRegistry& registry_;
};

} // namespace uic
