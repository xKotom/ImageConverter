#pragma once
#include "domain/Models.h"
#include <string>

namespace uic {

struct EncoderOptions {
    ImageFormat format = ImageFormat::Png;
    int quality = 90; // применяется только к форматам с потерями
};

// Порт: запись изображений.
class IImageEncoder {
public:
    virtual ~IImageEncoder() = default;
    virtual std::string id() const = 0;
    virtual bool supports(ImageFormat) const = 0;
    virtual Status encode(const ImageBuffer&, const fs::path& temporaryPath,
                          const EncoderOptions&, const CancellationToken&,
                          const ProgressSink&) = 0;
};

} // namespace uic
