#pragma once
#include "application/ports/IImageEncoder.h"

namespace uic {

// Кодировщик на базе QImageWriter. Качество — шкала конкретного плагина
// (для WEBP значение 100 включает режим без потерь).
class QtImageEncoder : public IImageEncoder {
public:
    std::string id() const override { return "qt-imageio"; }
    bool supports(ImageFormat f) const override;
    Status encode(const ImageBuffer&, const fs::path& temporaryPath, const EncoderOptions&,
                  const CancellationToken&, const ProgressSink&) override;
};

} // namespace uic
