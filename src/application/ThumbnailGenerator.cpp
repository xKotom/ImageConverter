#include "ThumbnailGenerator.h"
#include "FormatDetector.h"

namespace uic {

Result<ImageBuffer> ThumbnailGenerator::generate(const fs::path& path, int maxDimension,
                                                 const CancellationToken& token) const {
    auto fmt = FormatDetector::detectFile(path);
    if (!fmt) return fmt.error();
    IImageDecoder* dec = registry_.decoderFor(fmt.value());
    if (!dec) return makeError(ErrorCode::NoDecoder, "Нет декодера для предпросмотра");
    return dec->decode(path, DecodeOptions{maxDimension, true}, token, nullptr);
}

std::string ThumbnailGenerator::decoderVersionFor(const fs::path& path) const {
    auto fmt = FormatDetector::detectFile(path);
    if (!fmt) return {};
    IImageDecoder* dec = registry_.decoderFor(fmt.value());
    return dec ? dec->id() + ":" + dec->version() : std::string();
}

} // namespace uic
