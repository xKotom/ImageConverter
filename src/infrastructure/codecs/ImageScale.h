#pragma once
#include "domain/Models.h"

#include <algorithm>
#include <cmath>

namespace uic {

// Уменьшение «в ящик» (усреднение блока исходных пикселей). Нужно для превью форматов,
// у декодеров которых нет аппаратного scaled read (HEIF/AVIF).
inline ImageBuffer downscaleToFit(const ImageBuffer& src, int maxDim) {
    if (src.empty() || maxDim <= 0 || (src.width <= maxDim && src.height <= maxDim)) return src;
    const double k = static_cast<double>(maxDim) / std::max(src.width, src.height);
    ImageBuffer dst;
    dst.width = std::max(1, static_cast<int>(std::lround(src.width * k)));
    dst.height = std::max(1, static_cast<int>(std::lround(src.height * k)));
    dst.hasAlpha = src.hasAlpha;
    dst.iccProfile = src.iccProfile;
    dst.rgba.resize(static_cast<size_t>(dst.width) * dst.height * 4);
    for (int y = 0; y < dst.height; ++y) {
        const int y0 = static_cast<int>(static_cast<long long>(y) * src.height / dst.height);
        const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<long long>(y + 1) * src.height / dst.height));
        for (int x = 0; x < dst.width; ++x) {
            const int x0 = static_cast<int>(static_cast<long long>(x) * src.width / dst.width);
            const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<long long>(x + 1) * src.width / dst.width));
            unsigned long long sum[4] = {0, 0, 0, 0};
            for (int yy = y0; yy < y1; ++yy) {
                const std::uint8_t* p = src.rgba.data() + (static_cast<size_t>(yy) * src.width + x0) * 4;
                for (int xx = x0; xx < x1; ++xx, p += 4)
                    for (int c = 0; c < 4; ++c) sum[c] += p[c];
            }
            const unsigned long long n = static_cast<unsigned long long>(y1 - y0) * (x1 - x0);
            std::uint8_t* o = dst.rgba.data() + (static_cast<size_t>(y) * dst.width + x) * 4;
            for (int c = 0; c < 4; ++c) o[c] = static_cast<std::uint8_t>((sum[c] + n / 2) / n);
        }
    }
    return dst;
}

} // namespace uic
