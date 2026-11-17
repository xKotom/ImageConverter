#pragma once
#include "domain/Models.h"

#include <QByteArray>
#include <QString>

namespace uic {

// Хранит настройки пользователя (QSettings: реестр Windows / ini на других ОС).
class SettingsManager {
public:
    ConversionSettings loadConversion() const;
    void saveConversion(const ConversionSettings& s);

    QString lastDirectory() const;
    void setLastDirectory(const QString& dir);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& g);
    QByteArray windowState() const;
    void setWindowState(const QByteArray& s);

    // Оформление: 0 = системное, 1 = светлое, 2 = тёмное; стеклянный стиль — отдельный флаг.
    int appearance() const;
    void setAppearance(int a);
    bool glass() const;
    void setGlass(bool on);

    int maxParallel() const;
    void setMaxParallel(int n);
};

} // namespace uic
