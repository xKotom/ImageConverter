#pragma once
#include "CodecRegistry.h"

namespace uic {

// Проверяет совместимость и раскрывает потери ДО запуска конвертации.
class ConversionPlanner {
public:
    static constexpr long long kMaxPixels = 150'000'000; // защита памяти
    static constexpr long long kMaxSequenceBytes = 1536ll * 1024 * 1024; // все кадры анимации в памяти
    explicit ConversionPlanner(const CodecRegistry& registry) : registry_(registry) {}
    ConversionPlan plan(const FileInfo& info, const ConversionSettings& settings) const;
private:
    const CodecRegistry& registry_;
};

} // namespace uic
