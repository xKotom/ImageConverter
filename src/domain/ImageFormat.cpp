#include "ImageFormat.h"
#include <algorithm>
#include <cctype>

namespace uic {

namespace {
const std::vector<FormatTraits>& table() {
    static const std::vector<FormatTraits> t = {
        {ImageFormat::Unknown, "unknown", "Неизвестный", "", {}, false, false, false, false},
        {ImageFormat::Jpeg, "jpeg", "JPEG", "jpeg", {"jpg", "jpeg", "jpe", "jfif"}, false, true, false, true},
        {ImageFormat::Png,  "png",  "PNG",  "png",  {"png"}, true, false, false, true},
        {ImageFormat::Bmp,  "bmp",  "BMP",  "bmp",  {"bmp", "dib"}, false, false, false, false},
        {ImageFormat::Webp, "webp", "WEBP", "webp", {"webp"}, true, true, true, true},
        {ImageFormat::Gif,  "gif",  "GIF",  "gif",  {"gif"}, true, false, true, false},
        {ImageFormat::Tiff, "tiff", "TIFF", "tiff", {"tif", "tiff"}, true, false, true, true},
        {ImageFormat::Heif, "heif", "HEIC/HEIF", "heif", {"heic", "heif"}, true, true, true, true},
        {ImageFormat::Avif, "avif", "AVIF", "avif", {"avif"}, true, true, true, true},
    };
    return t;
}
std::string lower(std::string_view s) {
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    return r;
}
} // namespace

const FormatTraits& traits(ImageFormat f) { return table()[static_cast<size_t>(f)]; }

const std::vector<ImageFormat>& allKnownFormats() {
    static const std::vector<ImageFormat> v = [] {
        std::vector<ImageFormat> r;
        for (const auto& t : table()) if (t.id != ImageFormat::Unknown) r.push_back(t.id);
        return r;
    }();
    return v;
}

ImageFormat formatFromExtension(std::string_view ext) {
    std::string e = lower(ext);
    if (!e.empty() && e.front() == '.') e.erase(0, 1);
    for (const auto& t : table())
        if (std::find(t.extensions.begin(), t.extensions.end(), e) != t.extensions.end()) return t.id;
    return ImageFormat::Unknown;
}

ImageFormat formatFromName(std::string_view name) {
    std::string n = lower(name);
    for (const auto& t : table()) if (n == t.name) return t.id;
    return formatFromExtension(n);
}

std::string primaryExtension(ImageFormat f) {
    const auto& t = traits(f);
    return t.extensions.empty() ? std::string() : t.extensions.front();
}

} // namespace uic
