#include "QtImageDecoder.h"
#include "QtConvert.h"

#include <QByteArray>
#include <QColorSpace>
#include <QImageReader>
#include <QList>
#include <QtGlobal>

namespace uic {

namespace {
int toExif(QImageIOHandler::Transformations t) {
    switch (static_cast<int>(t)) {
    case QImageIOHandler::TransformationMirror: return 2;
    case QImageIOHandler::TransformationRotate180: return 3;
    case QImageIOHandler::TransformationFlip: return 4;
    case QImageIOHandler::TransformationMirrorAndRotate90: return 5;
    case QImageIOHandler::TransformationRotate90: return 6;
    case QImageIOHandler::TransformationFlipAndRotate90: return 7;
    case QImageIOHandler::TransformationRotate270: return 8;
    default: return 1;
    }
}

Error mapReaderError(const QImageReader& r) {
    using E = QImageReader::ImageReaderError;
    std::string msg = r.errorString().toStdString();
    switch (r.error()) {
    case E::FileNotFoundError: return makeError(ErrorCode::NotFound, "Файл не найден");
    case E::DeviceError: return makeError(ErrorCode::IoError, "Ошибка чтения файла: " + msg);
    case E::UnsupportedFormatError: return makeError(ErrorCode::UnsupportedFormat, "Формат не поддерживается: " + msg);
    case E::InvalidDataError: return makeError(ErrorCode::CorruptedFile, "Файл повреждён: " + msg);
    default: return makeError(ErrorCode::CorruptedFile, "Не удалось прочитать изображение: " + msg);
    }
}
} // namespace

std::string QtImageDecoder::version() const {
    return std::string("qt-") + qVersion();
}

bool QtImageDecoder::supports(ImageFormat f) const {
    if (f == ImageFormat::Unknown) return false;
    static const QList<QByteArray> sup = QImageReader::supportedImageFormats();
    return sup.contains(QByteArray(traits(f).qtName));
}

Result<ImageProbe> QtImageDecoder::probe(const fs::path& path) {
    QImageReader r(toQString(path));
    r.setDecideFormatFromContent(true); // содержимое важнее расширения
    if (!r.canRead()) return mapReaderError(r);
    const QSize sz = r.size();
    ImageProbe p;
    p.width = sz.width();
    p.height = sz.height();
    if (!sz.isValid() || sz.isEmpty()) {
        // Некоторые плагины не отдают размер без чтения — читаем кадр целиком.
        QImage img = r.read();
        if (img.isNull()) return mapReaderError(r);
        p.width = img.width();
        p.height = img.height();
        p.hasAlpha = img.hasAlphaChannel();
        p.hasIccProfile = img.colorSpace().isValid();
    } else {
        p.hasAlpha = QImage::toPixelFormat(r.imageFormat()).alphaUsage() == QPixelFormat::UsesAlpha;
        p.hasIccProfile = false;
    }
    const int n = r.imageCount();
    p.frameCount = n > 0 ? n : 1;
    p.exifOrientation = toExif(r.transformation());
    // Ориентация 5..8 меняет местами ширину и высоту при автоповороте.
    if (p.exifOrientation >= 5) std::swap(p.width, p.height);
    return p;
}

Result<ImageBuffer> QtImageDecoder::decode(const fs::path& path, const DecodeOptions& opt,
                                           const CancellationToken& token, const ProgressSink& progress) {
    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");
    QImageReader r(toQString(path));
    r.setDecideFormatFromContent(true);
    r.setAutoTransform(opt.autoOrient);

    if (opt.maxDimension > 0) {
        const QSize raw = r.size();
        if (raw.isValid() && (raw.width() > opt.maxDimension || raw.height() > opt.maxDimension))
            r.setScaledSize(raw.scaled(opt.maxDimension, opt.maxDimension, Qt::KeepAspectRatio));
    }

    QImage img = r.read();
    if (img.isNull()) return mapReaderError(r);
    if (opt.maxDimension > 0 && (img.width() > opt.maxDimension || img.height() > opt.maxDimension))
        img = img.scaled(opt.maxDimension, opt.maxDimension, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");
    ImageBuffer buf = toBuffer(img);
    if (buf.empty()) return makeError(ErrorCode::CorruptedFile, "Получено пустое изображение");
    if (progress) progress(100);
    return buf;
}

} // namespace uic
