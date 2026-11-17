#pragma once
#include "application/ports/IImageEncoder.h"

namespace uic {

// Запись анимированного WEBP через libwebp (WebPAnimEncoder). Одиночные кадры остаются за Qt.
// Качество 100 = режим без потерь (как и в Qt-плагине).
class WebpAnimEncoder : public IImageEncoder {
public:
    std::string id() const override { return "libwebp-anim"; }
    bool supports(ImageFormat f) const override { return f == ImageFormat::Webp; }
    Status encode(const ImageBuffer&, const fs::path& temporaryPath, const EncoderOptions&,
                  const CancellationToken&, const ProgressSink&) override;
    bool supportsSequence(ImageFormat f) const override { return f == ImageFormat::Webp; }
    Status encodeSequence(const ImageSequence&, const fs::path& temporaryPath, const EncoderOptions&,
                          const CancellationToken&, const ProgressSink&) override;
};

} // namespace uic
