#pragma once
#include "CodecRegistry.h"

namespace uic {

// Оркестрация одной конвертации: decode -> ImageBuffer -> (фон вместо alpha) -> encode
// во временный файл -> проверка результата -> фиксация имени. Не бросает исключений наружу.
// Вызывается из рабочих потоков; сам состояния не хранит.
class ConversionEngine {
public:
    explicit ConversionEngine(const CodecRegistry& registry) : registry_(registry) {}
    ConversionResult convert(const FileInfo& info, const ConversionSettings& settings,
                             const CancellationToken& token, const ProgressSink& progress) const;

    // Заменяет прозрачность заливкой фоном (0xRRGGBB). Публично для тестов.
    static void flattenAlpha(ImageBuffer& img, std::uint32_t backgroundRgb);

private:
    ConversionResult run(const FileInfo&, const ConversionSettings&, const CancellationToken&,
                         const ProgressSink&) const;
    const CodecRegistry& registry_;
};

} // namespace uic
