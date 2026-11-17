#include "Icons.h"

#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

namespace uic::Icons {

QPixmap pixmap(Kind kind, int size, const QColor& color, qreal dpr) {
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal s = size;
    QPen pen(color, s * 0.085, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (kind) {
    case Kind::Sliders: { // «slider.horizontal.3»
        const qreal ys[3] = {0.25, 0.5, 0.75};
        const qreal xs[3] = {0.65, 0.33, 0.58};
        for (int i = 0; i < 3; ++i) {
            p.drawLine(QPointF(s * 0.12, s * ys[i]), QPointF(s * 0.88, s * ys[i]));
            p.setBrush(color);
            p.drawEllipse(QPointF(s * xs[i], s * ys[i]), s * 0.09, s * 0.09);
            p.setBrush(Qt::NoBrush);
        }
        break;
    }
    case Kind::Plus:
        p.drawLine(QPointF(s * 0.5, s * 0.2), QPointF(s * 0.5, s * 0.8));
        p.drawLine(QPointF(s * 0.2, s * 0.5), QPointF(s * 0.8, s * 0.5));
        break;
    case Kind::Folder: {
        QPainterPath path;
        path.moveTo(s * 0.12, s * 0.30);
        path.quadTo(s * 0.12, s * 0.22, s * 0.20, s * 0.22);
        path.lineTo(s * 0.40, s * 0.22);
        path.lineTo(s * 0.48, s * 0.31);
        path.lineTo(s * 0.80, s * 0.31);
        path.quadTo(s * 0.88, s * 0.31, s * 0.88, s * 0.39);
        path.lineTo(s * 0.88, s * 0.72);
        path.quadTo(s * 0.88, s * 0.80, s * 0.80, s * 0.80);
        path.lineTo(s * 0.20, s * 0.80);
        path.quadTo(s * 0.12, s * 0.80, s * 0.12, s * 0.72);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Kind::TrayDown: { // стрелка вниз в лоток
        p.drawLine(QPointF(s * 0.5, s * 0.12), QPointF(s * 0.5, s * 0.58));
        p.drawLine(QPointF(s * 0.32, s * 0.42), QPointF(s * 0.5, s * 0.60));
        p.drawLine(QPointF(s * 0.68, s * 0.42), QPointF(s * 0.5, s * 0.60));
        QPainterPath tray;
        tray.moveTo(s * 0.14, s * 0.62);
        tray.lineTo(s * 0.14, s * 0.78);
        tray.quadTo(s * 0.14, s * 0.86, s * 0.22, s * 0.86);
        tray.lineTo(s * 0.78, s * 0.86);
        tray.quadTo(s * 0.86, s * 0.86, s * 0.86, s * 0.78);
        tray.lineTo(s * 0.86, s * 0.62);
        p.drawPath(tray);
        break;
    }
    case Kind::Checkmark:
        p.drawLine(QPointF(s * 0.22, s * 0.54), QPointF(s * 0.43, s * 0.74));
        p.drawLine(QPointF(s * 0.43, s * 0.74), QPointF(s * 0.80, s * 0.28));
        break;
    }
    return pm;
}

QIcon icon(Kind kind, int size, const QColor& color) { return QIcon(pixmap(kind, size, color)); }

QString chevronFile(const QColor& color) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/theme";
    QDir().mkpath(dir);
    const QString path = dir + "/chevron_" + color.name().mid(1) + ".png";
    if (!QFileInfo::exists(path)) {
        QPixmap pm(20, 20);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(5, 7.5), QPointF(10, 12.5));
        p.drawLine(QPointF(10, 12.5), QPointF(15, 7.5));
        p.end();
        pm.save(path, "PNG");
    }
    return path;
}

} // namespace uic::Icons
