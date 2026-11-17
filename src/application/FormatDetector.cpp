#include "FormatDetector.h"
#include <array>
#include <cstring>
#include <fstream>

namespace uic {

namespace {
bool startsWith(std::span<const std::uint8_t> h, const char* sig, size_t n, size_t offset = 0) {
    return h.size() >= offset + n && std::memcmp(h.data() + offset, sig, n) == 0;
}
} // namespace

ImageFormat FormatDetector::detect(std::span<const std::uint8_t> h) {
    if (startsWith(h, "\xFF\xD8\xFF", 3)) return ImageFormat::Jpeg;
    if (startsWith(h, "\x89PNG\r\n\x1A\n", 8)) return ImageFormat::Png;
    if (startsWith(h, "GIF87a", 6) || startsWith(h, "GIF89a", 6)) return ImageFormat::Gif;
    if (startsWith(h, "RIFF", 4) && startsWith(h, "WEBP", 4, 8)) return ImageFormat::Webp;
    if (startsWith(h, "II*\0", 4) || startsWith(h, "MM\0*", 4)) return ImageFormat::Tiff;
    // HEIF/AVIF: ISO BMFF — «ftyp» + brand; одной сигнатуры недостаточно, смотрим brand.
    if (startsWith(h, "ftyp", 4, 4) && h.size() >= 12) {
        if (startsWith(h, "avif", 4, 8) || startsWith(h, "avis", 4, 8)) return ImageFormat::Avif;
        static const char* brands[] = {"heic", "heix", "hevc", "hevx", "mif1", "msf1", "heim", "heis"};
        for (auto b : brands) if (startsWith(h, b, 4, 8)) return ImageFormat::Heif;
    }
    // «BM» — слабая сигнатура; окончательно подтверждается probe декодера.
    if (startsWith(h, "BM", 2) && h.size() >= 14) return ImageFormat::Bmp;
    return ImageFormat::Unknown;
}

Result<ImageFormat> FormatDetector::detectFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return makeError(ErrorCode::AccessDenied, "Не удалось открыть файл для чтения");
    std::array<std::uint8_t, 32> buf{};
    in.read(reinterpret_cast<char*>(buf.data()), buf.size());
    auto n = static_cast<size_t>(in.gcount());
    return detect(std::span<const std::uint8_t>(buf.data(), n));
}

} // namespace uic
