#include "FileAnalyzer.h"
#include "FileValidator.h"
#include "FormatDetector.h"

namespace uic {

Result<FileInfo> FileAnalyzer::analyze(const fs::path& path) const {
    if (auto st = FileValidator::validate(path); !st) return st.error();

    auto fmt = FormatDetector::detectFile(path);
    if (!fmt) return fmt.error();
    if (fmt.value() == ImageFormat::Unknown)
        return makeError(ErrorCode::UnsupportedFormat,
                         "Формат не распознан: ни один декодер не опознал содержимое файла");

    const ImageFormat format = fmt.value();
    IImageDecoder* dec = registry_.decoderFor(format);
    if (!dec)
        return makeError(ErrorCode::NoDecoder,
                         std::string("Формат ") + traits(format).label +
                             " распознан, но декодер в этой сборке недоступен");

    auto probe = dec->probe(path);
    if (!probe) return probe.error();

    FileInfo info;
    info.path = path;
    info.actualFormat = format;
    info.extensionHint = path.extension().string();
    if (!info.extensionHint.empty() && info.extensionHint.front() == '.') info.extensionHint.erase(0, 1);
    std::error_code ec;
    info.fileSize = fs::file_size(path, ec);
    info.width = probe.value().width;
    info.height = probe.value().height;
    info.exifOrientation = probe.value().exifOrientation;
    info.hasAlpha = probe.value().hasAlpha;
    info.frameCount = probe.value().frameCount;
    info.hasIccProfile = probe.value().hasIccProfile;
    return info;
}

} // namespace uic
