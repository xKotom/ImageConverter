#pragma once
#include "application/ports/IImageDecoder.h"
#include "application/ports/IImageEncoder.h"

namespace uic {

// HEIC/HEIF и AVIF на libheif. Что доступно, определяется при запуске по фактически
// загруженным кодекам libheif (HEVC/AV1), а не по расширению файла.
//
// Лицензии: libheif и libde265 — LGPL; кодировщик HEVC (x265) — GPL, поэтому запись HEIC
// отключена, пока сборка не выполнена с -DUIC_ENABLE_HEIC_ENCODE=ON. Запись AVIF идёт через aom (BSD).
class HeifDecoder : public IImageDecoder {
public:
    std::string id() const override { return "libheif"; }
    std::string version() const override;
    bool supports(ImageFormat f) const override;
    Result<ImageProbe> probe(const fs::path& path) override;
    Result<ImageBuffer> decode(const fs::path& path, const DecodeOptions&,
                               const CancellationToken&, const ProgressSink&) override;
};

class HeifEncoder : public IImageEncoder {
public:
    std::string id() const override { return "libheif"; }
    bool supports(ImageFormat f) const override;
    Status encode(const ImageBuffer&, const fs::path& temporaryPath, const EncoderOptions&,
                  const CancellationToken&, const ProgressSink&) override;
};

} // namespace uic
