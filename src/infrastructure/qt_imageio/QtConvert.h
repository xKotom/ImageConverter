#pragma once
#include "domain/Models.h"
#include <QImage>
#include <QString>

namespace uic {

inline QString toQString(const fs::path& p) { return QString::fromStdU16String(p.u16string()); }
inline fs::path toPath(const QString& s) { return fs::path(s.toStdU16String()); }
inline QString fromUtf8(const std::string& s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }

// QImage <-> ImageBuffer (RGBA8888 non-premultiplied)
ImageBuffer toBuffer(const QImage& img);
QImage toQImage(const ImageBuffer& buf);

} // namespace uic
