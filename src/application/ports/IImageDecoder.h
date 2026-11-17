#pragma once
#include "domain/Models.h"
#include <string>

namespace uic {

struct DecodeOptions {
    int maxDimension = 0;   // 0 = полный размер; иначе читать уменьшенную копию (preview)
    bool autoOrient = true; // применить EXIF-ориентацию к пикселям
};

// Порт: чтение изображений. Реализации лежат в infrastructure.
class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;
    virtual std::string id() const = 0;
    virtual std::string version() const = 0; // входит в ключ кэша превью
    virtual bool supports(ImageFormat) const = 0;
    virtual Result<ImageProbe> probe(const fs::path&) = 0;
    virtual Result<ImageBuffer> decode(const fs::path&, const DecodeOptions&,
                                       const CancellationToken&, const ProgressSink&) = 0;
};

} // namespace uic
