#include "CodecRegistry.h"

namespace uic {

IImageDecoder* CodecRegistry::decoderFor(ImageFormat f) const {
    for (const auto& d : decoders_) if (d->supports(f)) return d.get();
    return nullptr;
}

IImageEncoder* CodecRegistry::encoderFor(ImageFormat f) const {
    for (const auto& e : encoders_) if (e->supports(f)) return e.get();
    return nullptr;
}

std::vector<ImageFormat> CodecRegistry::readableFormats() const {
    std::vector<ImageFormat> r;
    for (auto f : allKnownFormats()) if (canRead(f)) r.push_back(f);
    return r;
}

std::vector<ImageFormat> CodecRegistry::writableFormats() const {
    std::vector<ImageFormat> r;
    for (auto f : allKnownFormats()) if (canWrite(f)) r.push_back(f);
    return r;
}

} // namespace uic
