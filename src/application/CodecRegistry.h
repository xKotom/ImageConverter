#pragma once
#include "ports/IImageDecoder.h"
#include "ports/IImageEncoder.h"

#include <memory>
#include <vector>

namespace uic {

// Реестр возможностей: формат «поддерживается» только при наличии
// подтверждённого decoder/encoder, а не по списку расширений.
// После настройки при старте используется только для чтения (потокобезопасно).
class CodecRegistry {
public:
    void addDecoder(std::shared_ptr<IImageDecoder> d) { decoders_.push_back(std::move(d)); }
    void addEncoder(std::shared_ptr<IImageEncoder> e) { encoders_.push_back(std::move(e)); }

    IImageDecoder* decoderFor(ImageFormat f) const;
    IImageEncoder* encoderFor(ImageFormat f) const;
    bool canRead(ImageFormat f) const { return decoderFor(f) != nullptr; }
    bool canWrite(ImageFormat f) const { return encoderFor(f) != nullptr; }
    std::vector<ImageFormat> readableFormats() const;
    std::vector<ImageFormat> writableFormats() const;

private:
    std::vector<std::shared_ptr<IImageDecoder>> decoders_;
    std::vector<std::shared_ptr<IImageEncoder>> encoders_;
};

} // namespace uic
