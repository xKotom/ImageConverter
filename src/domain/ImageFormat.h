#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace uic {

enum class ImageFormat { Unknown, Jpeg, Png, Bmp, Webp, Gif, Tiff, Heif, Avif };

struct FormatTraits {
    ImageFormat id;
    const char* name;      // каноническое имя
    const char* label;     // подпись для UI
    const char* qtName;    // имя формата в Qt image I/O
    std::vector<std::string> extensions; // первое — основное расширение результата
    bool supportsAlpha;
    bool lossy;
    bool supportsAnimation;
    bool supportsIcc;
};

const FormatTraits& traits(ImageFormat f);
const std::vector<ImageFormat>& allKnownFormats();
ImageFormat formatFromExtension(std::string_view ext); // с точкой или без, без учёта регистра
ImageFormat formatFromName(std::string_view name);     // "jpeg", "jpg", "png" ...
std::string primaryExtension(ImageFormat f);           // без точки

} // namespace uic
