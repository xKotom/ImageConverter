#pragma once
#include "domain/Models.h"

namespace uic {

// Безопасная запись: результат пишется во временный файл с непредсказуемым именем
// в ЦЕЛЕВОМ каталоге (тот же том), и только commit() даёт ему итоговое имя.
// Деструктор без commit() удаляет временный остаток (RAII) — это и откат при ошибке/отмене.
class AtomicFileWriter {
public:
    static Result<AtomicFileWriter> begin(const fs::path& targetDir);

    AtomicFileWriter(AtomicFileWriter&& o) noexcept;
    AtomicFileWriter& operator=(AtomicFileWriter&& o) noexcept;
    AtomicFileWriter(const AtomicFileWriter&) = delete;
    AtomicFileWriter& operator=(const AtomicFileWriter&) = delete;
    ~AtomicFileWriter();

    const fs::path& tempPath() const { return temp_; }
    // replaceExisting=false: если итоговый файл уже появился, коммит отклоняется.
    Status commit(const fs::path& finalPath, bool replaceExisting);
    void discard() noexcept;

private:
    explicit AtomicFileWriter(fs::path temp) : temp_(std::move(temp)) {}
    fs::path temp_;
};

} // namespace uic
