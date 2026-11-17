#include "QtImageEncoder.h"
#include "QtConvert.h"

#include <algorithm>
#include <QImageWriter>
#include <QList>

namespace uic {

bool QtImageEncoder::supports(ImageFormat f) const {
    if (f == ImageFormat::Unknown) return false;
    static const QList<QByteArray> sup = QImageWriter::supportedImageFormats();
    return sup.contains(QByteArray(traits(f).qtName));
}

Status QtImageEncoder::encode(const ImageBuffer& buf, const fs::path& tempPath, const EncoderOptions& opt,
                              const CancellationToken& token, const ProgressSink& progress) {
    if (token.cancelled()) return makeError(ErrorCode::Cancelled, "Операция отменена");
    if (!supports(opt.format)) return makeError(ErrorCode::NoEncoder, "Кодировщик недоступен");

    QImage img = toQImage(buf);
    if (img.isNull()) return makeError(ErrorCode::InvalidInput, "Пустое изображение");

    // Формат задаём явно: временный файл имеет расширение .tmp.
    QImageWriter w(toQString(tempPath), QByteArray(traits(opt.format).qtName));
    if (traits(opt.format).lossy) w.setQuality(std::clamp(opt.quality, 1, 100));
    if (!w.write(img))
        return makeError(ErrorCode::EncodeFailed, "Ошибка кодирования: " + w.errorString().toStdString());
    if (progress) progress(100);
    return Ok{};
}

} // namespace uic
