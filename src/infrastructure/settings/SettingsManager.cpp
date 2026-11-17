#include "SettingsManager.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QSettings>
#include <algorithm>

namespace uic {

namespace {
QSettings make() { return QSettings(QSettings::IniFormat, QSettings::UserScope, "UniversalImageConverter", "settings"); }
}

ConversionSettings SettingsManager::loadConversion() const {
    QSettings q = make();
    ConversionSettings s;
    s.targetFormat = formatFromName(q.value("target", "png").toString().toStdString());
    if (s.targetFormat == ImageFormat::Unknown) s.targetFormat = ImageFormat::Png;
    s.quality = std::clamp(q.value("quality", 90).toInt(), 1, 100);
    s.outputDir = toPath(q.value("outputDir").toString());
    s.conflict = static_cast<ConflictPolicy>(std::clamp(q.value("conflict", 0).toInt(), 0, 2));
    s.animation = q.value("rejectAnimation", false).toBool() ? AnimationPolicy::Reject
                                                              : AnimationPolicy::FirstFrameWithWarning;
    s.backgroundRgb = q.value("background", 0xFFFFFF).toUInt() & 0xFFFFFF;
    return s;
}

void SettingsManager::saveConversion(const ConversionSettings& s) {
    QSettings q = make();
    q.setValue("target", QString::fromLatin1(traits(s.targetFormat).name));
    q.setValue("quality", s.quality);
    q.setValue("outputDir", toQString(s.outputDir));
    q.setValue("conflict", static_cast<int>(s.conflict));
    q.setValue("rejectAnimation", s.animation == AnimationPolicy::Reject);
    q.setValue("background", s.backgroundRgb);
}

QString SettingsManager::lastDirectory() const { return make().value("lastDir").toString(); }
void SettingsManager::setLastDirectory(const QString& d) { make().setValue("lastDir", d); }
QByteArray SettingsManager::windowGeometry() const { return make().value("window/geometry").toByteArray(); }
void SettingsManager::setWindowGeometry(const QByteArray& g) { make().setValue("window/geometry", g); }
QByteArray SettingsManager::windowState() const { return make().value("window/state").toByteArray(); }
void SettingsManager::setWindowState(const QByteArray& s) { make().setValue("window/state", s); }
int SettingsManager::maxParallel() const { return std::clamp(make().value("maxParallel", 2).toInt(), 1, 16); }
void SettingsManager::setMaxParallel(int n) { make().setValue("maxParallel", n); }

} // namespace uic
