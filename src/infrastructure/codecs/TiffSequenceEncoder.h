#pragma once
#include "application/ports/IImageEncoder.h"

namespace uic {

// Запись многостраничного TIFF через libtiff (Deflate, ICC-профиль первой страницы).
// Одиночные кадры по умолчанию пишет Qt; этот кодировщик нужен для последовательностей.
class TiffSequenceEncoder : public IImageEncoder {
public:
    std::string id() const override { return "libtiff"; }
    bool supports(ImageFormat f) const override { return f == ImageFormat::Tiff; }
    Status encode(const ImageBuffer&, const fs::path& temporaryPath, const EncoderOptions&,
                  const CancellationToken&, const ProgressSink&) override;
    bool supportsSequence(ImageFormat f) const override { return f == ImageFormat::Tiff; }
    Status encodeSequence(const ImageSequence&, const fs::path& temporaryPath, const EncoderOptions&,
                          const CancellationToken&, const ProgressSink&) override;
};

} // namespace uic
