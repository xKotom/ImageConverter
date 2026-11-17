#pragma once
#include "ports/IImageDecoder.h"
#include "ports/IImageEncoder.h"
#include "ports/IMetadataService.h"

#include <memory>
#include <vector>

namespace uic {

// Реестр возможностей: формат «поддерживается» только при наличии
// подтверждённого decoder/encoder, а не по списку расширений.
// После настройки при старте используется только для чтения (потокобезопасно).
// Подтверждённые возможности формата: что реально умеют зарегистрированные адаптеры.
struct FormatCapability {
    ImageFormat format = ImageFormat::Unknown;
    bool read = false;
    bool write = false;
    bool readSequence = false;
    bool writeSequence = false;
    bool writeMetadata = false;
};

class CodecRegistry {
public:
    void addDecoder(std::shared_ptr<IImageDecoder> d) { decoders_.push_back(std::move(d)); }
    void addEncoder(std::shared_ptr<IImageEncoder> e) { encoders_.push_back(std::move(e)); }

    void setMetadataService(std::shared_ptr<IMetadataService> m) { metadata_ = std::move(m); }
    IMetadataService* metadataService() const { return metadata_.get(); }

    IImageDecoder* decoderFor(ImageFormat f) const;
    IImageDecoder* sequenceDecoderFor(ImageFormat f) const;
    IImageEncoder* sequenceEncoderFor(ImageFormat f) const;
    FormatCapability capability(ImageFormat f) const;
    std::vector<FormatCapability> capabilities() const; // только форматы, где что-то доступно
    IImageEncoder* encoderFor(ImageFormat f) const;
    bool canRead(ImageFormat f) const { return decoderFor(f) != nullptr; }
    bool canWrite(ImageFormat f) const { return encoderFor(f) != nullptr; }
    std::vector<ImageFormat> readableFormats() const;
    std::vector<ImageFormat> writableFormats() const;

private:
    std::vector<std::shared_ptr<IImageDecoder>> decoders_;
    std::vector<std::shared_ptr<IImageEncoder>> encoders_;
    std::shared_ptr<IMetadataService> metadata_;
};

} // namespace uic
