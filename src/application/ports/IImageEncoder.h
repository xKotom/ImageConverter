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

    // Запись анимации / многостраничного файла.
    virtual bool supportsSequence(ImageFormat) const { return false; }
    virtual Status encodeSequence(const ImageSequence&, const fs::path& /*temporaryPath*/,
                                  const EncoderOptions&, const CancellationToken&,
                                  const ProgressSink&) {
        return makeError(ErrorCode::NoEncoder, "Запись анимации не поддерживается этим кодировщиком");
    }
};

} // namespace uic
