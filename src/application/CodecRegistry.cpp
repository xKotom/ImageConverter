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

IImageDecoder* CodecRegistry::sequenceDecoderFor(ImageFormat f) const {
    for (const auto& d : decoders_) if (d->supports(f) && d->supportsSequence(f)) return d.get();
    return nullptr;
}

IImageEncoder* CodecRegistry::sequenceEncoderFor(ImageFormat f) const {
    for (const auto& e : encoders_) if (e->supports(f) && e->supportsSequence(f)) return e.get();
    return nullptr;
}

FormatCapability CodecRegistry::capability(ImageFormat f) const {
    FormatCapability c;
    c.format = f;
    c.read = canRead(f);
    c.write = canWrite(f);
    c.readSequence = sequenceDecoderFor(f) != nullptr;
    c.writeSequence = sequenceEncoderFor(f) != nullptr;
    c.writeMetadata = metadata_ && metadata_->canWrite(f);
    return c;
}

std::vector<FormatCapability> CodecRegistry::capabilities() const {
    std::vector<FormatCapability> r;
    for (auto f : allKnownFormats()) {
        auto c = capability(f);
        if (c.read || c.write) r.push_back(c);
    }
    return r;
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
