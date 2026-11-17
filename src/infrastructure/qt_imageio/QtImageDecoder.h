#pragma once
#include "application/ports/IImageDecoder.h"

namespace uic {

// Декодер на базе QImageReader. Реально доступные форматы зависят от плагинов в поставке.
class QtImageDecoder : public IImageDecoder {
public:
    std::string id() const override { return "qt-imageio"; }
    std::string version() const override;
    bool supports(ImageFormat f) const override;
    Result<ImageProbe> probe(const fs::path& path) override;
    Result<ImageBuffer> decode(const fs::path& path, const DecodeOptions&,
                               const CancellationToken&, const ProgressSink&) override;
};

} // namespace uic
