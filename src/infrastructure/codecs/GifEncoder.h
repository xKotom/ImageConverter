#pragma once
#include "application/ports/IImageEncoder.h"

namespace uic {

// Запись GIF (одиночный кадр и анимация) на giflib. Qt GIF писать не умеет.
// Ограничения формата: глобальная палитра до 256 цветов (медианное сечение, без дизеринга),
// прозрачность однобитная (альфа < 128 -> прозрачный пиксель).
class GifEncoder : public IImageEncoder {
public:
    std::string id() const override { return "giflib"; }
    bool supports(ImageFormat f) const override { return f == ImageFormat::Gif; }
    Status encode(const ImageBuffer&, const fs::path& temporaryPath, const EncoderOptions&,
                  const CancellationToken&, const ProgressSink&) override;
    bool supportsSequence(ImageFormat f) const override { return f == ImageFormat::Gif; }
    Status encodeSequence(const ImageSequence&, const fs::path& temporaryPath, const EncoderOptions&,
                          const CancellationToken&, const ProgressSink&) override;
};

} // namespace uic
