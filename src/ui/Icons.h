#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace uic::Icons {

enum class Kind { Sliders, Plus, Folder, TrayDown, Checkmark };

// Векторные значки, нарисованные QPainter (линии со скруглёнными концами, как в SF Symbols).
QPixmap pixmap(Kind kind, int logicalSize, const QColor& color, qreal devicePixelRatio = 2.0);
QIcon icon(Kind kind, int logicalSize, const QColor& color);

// Стрелка выпадающего списка для QSS: сохраняется как PNG в кэше, возвращается путь.
QString chevronFile(const QColor& color);

} // namespace uic::Icons
