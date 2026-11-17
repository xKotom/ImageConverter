#include "QtConvert.h"
#include <QColorSpace>
#include <cstring>

namespace uic {

ImageBuffer toBuffer(const QImage& src) {
    ImageBuffer b;
    if (src.isNull()) return b;
    const bool alpha = src.hasAlphaChannel();
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    b.width = img.width();
    b.height = img.height();
    b.hasAlpha = alpha;
    b.rgba.resize(static_cast<size_t>(b.width) * b.height * 4);
    for (int y = 0; y < b.height; ++y)
        std::memcpy(b.rgba.data() + static_cast<size_t>(y) * b.width * 4, img.constScanLine(y),
                    static_cast<size_t>(b.width) * 4);
    if (src.colorSpace().isValid()) {
        const QByteArray icc = src.colorSpace().iccProfile();
        b.iccProfile.assign(icc.begin(), icc.end());
    }
    return b;
}

QImage toQImage(const ImageBuffer& b) {
    if (b.empty()) return {};
    QImage wrap(b.rgba.data(), b.width, b.height, b.stride(), QImage::Format_RGBA8888);
    QImage img = wrap.copy(); // владеет данными
    if (!b.hasAlpha) img.convertTo(QImage::Format_RGB32);
    if (!b.iccProfile.empty()) {
        QColorSpace cs = QColorSpace::fromIccProfile(
            QByteArray(reinterpret_cast<const char*>(b.iccProfile.data()), static_cast<qsizetype>(b.iccProfile.size())));
        if (cs.isValid()) img.setColorSpace(cs);
    }
    return img;
}

} // namespace uic
